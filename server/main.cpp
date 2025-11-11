#include <crow.h>
#include <pqxx/pqxx>
#include <mbedtls/md.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <vector>
#include <string>
#include <sstream>

using namespace std;

#define HMAC_TAG_LEN 16


// Global ECDH key pair for server
static mbedtls_ecdh_context server_ecdh;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static bool ecdh_initialized = false;

// Global master key storage
static DerivedKeys baseKeys;
static bool masterKeySet = false;

// Initialize server ECDH keys
static bool server_keys_init() {
    if (ecdh_initialized) return true;
    
    mbedtls_ecdh_init(&server_ecdh);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    const char* pers = "server_ecdh";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char*)pers, strlen(pers));
    if (ret != 0) return false;

    // Setup P-256 curve and generate key pair
    ret = mbedtls_ecdh_setup(&server_ecdh, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) return false;

    ret = mbedtls_ecdh_gen_public(&server_ecdh.grp, &server_ecdh.d, &server_ecdh.Q,
                                  mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) return false;

    ecdh_initialized = true;
    return true;
}

// Get server public key as uncompressed hex string
static string server_pubkey_uncompressed() {
    if (!ecdh_initialized) return "";
    
    uint8_t pub_bytes[65]; // 1 byte (0x04) + 32 bytes X + 32 bytes Y
    size_t olen;
    int ret = mbedtls_ecp_point_write_binary(&server_ecdh.grp, &server_ecdh.Q,
                                             MBEDTLS_ECP_PF_UNCOMPRESSED,
                                             &olen, pub_bytes, sizeof(pub_bytes));
    if (ret != 0 || olen != 65) return "";

    ostringstream ss;
    ss << hex << uppercase << setfill('0');
    for (size_t i = 0; i < olen; ++i) {
        ss << setw(2) << (int)pub_bytes[i];
    }
    return ss.str();
}

// Compute master key from device public key hex
static bool compute_master_from_device_pub_hex(const string& devicePubHex, uint8_t masterOut[32]) {
    if (!ecdh_initialized) return false;

    // Convert hex to binary
    vector<uint8_t> pubBytes;
    for (size_t i = 0; i < devicePubHex.length(); i += 2) {
        unsigned int byte;
        sscanf(devicePubHex.substr(i, 2).c_str(), "%x", &byte);
        pubBytes.push_back(static_cast<uint8_t>(byte));
    }

    // Parse device public key
    mbedtls_ecp_point device_pub;
    mbedtls_ecp_point_init(&device_pub);
    
    int ret = mbedtls_ecp_point_read_binary(&server_ecdh.grp, &device_pub,
                                            pubBytes.data(), pubBytes.size());
    if (ret != 0) {
        mbedtls_ecp_point_free(&device_pub);
        return false;
    }

    // Compute shared secret
    mbedtls_mpi shared_secret;
    mbedtls_mpi_init(&shared_secret);
    ret = mbedtls_ecdh_compute_shared(&server_ecdh.grp, &shared_secret,
                                      &device_pub, &server_ecdh.d,
                                      mbedtls_ctr_drbg_random, &ctr_drbg);

    bool success = false;
    if (ret == 0) {
        // Export shared secret to bytes
        uint8_t secret_bytes[32];
        ret = mbedtls_mpi_write_binary(&shared_secret, secret_bytes, 32);
        if (ret == 0) {
            memcpy(masterOut, secret_bytes, 32);
            success = true;
        }
    }

    mbedtls_mpi_free(&shared_secret);
    mbedtls_ecp_point_free(&device_pub);
    return success;
}

static vector<uint8_t> removeSalt(const vector<uint8_t>& salted, size_t saltedLen, const SaltMeta& meta) {
    if (meta.len == 0 || meta.pos > saltedLen) return salted;
    vector<uint8_t> out(saltedLen - meta.len);
    copy(salted.begin(), salted.begin() + meta.pos, out.begin());
    copy(salted.begin() + meta.pos + meta.len, salted.end(), out.begin() + meta.pos);
    return out;
}

static string pipelineDecryptPacket(const DerivedKeys& baseKeys, const vector<uint8_t>& packet, size_t packetLen) {
    if (packetLen < 8 + HMAC_TAG_LEN) return "";

    vector<uint8_t> header(packet.begin(), packet.begin() + 8);
    uint8_t version = header[0];
    bool hasNonce = (version & 0x80) != 0;
    size_t nonceLen = hasNonce ? 4 : 0;
    if (packetLen < 8 + nonceLen + HMAC_TAG_LEN) return "";

    uint8_t saltLen = header[1];
    uint16_t saltPos = (header[2] | (header[3] << 8));
    uint16_t payloadLen = (header[4] | (header[5] << 8));
    uint8_t rows = header[6];
    uint8_t cols = header[7];
    GridSpec grid = {rows, cols};
    SaltMeta saltMeta = {saltPos, saltLen};

    vector<uint8_t> noncePtr = hasNonce ? vector<uint8_t>(packet.begin() + 8, packet.begin() + 8 + nonceLen) : vector<uint8_t>();
    uint32_t nonce = hasNonce ? ((noncePtr[0] << 24) | (noncePtr[1] << 16) | (noncePtr[2] << 8) | noncePtr[3]) : 0;
    
    // Get encrypted data and HMAC tag
    vector<uint8_t> ct(packet.begin() + 8 + nonceLen, packet.end() - HMAC_TAG_LEN);
    vector<uint8_t> tag(packet.end() - HMAC_TAG_LEN, packet.end());

    // Debug: print header fields
    {
        cout << "[Server] Header: ver=0x" << hex << uppercase << (int)version
             << " saltLen=" << dec << (int)saltLen
             << " saltPos=" << saltPos
             << " payloadLen=" << payloadLen
             << " rows=" << (int)rows
             << " cols=" << (int)cols
             << " nonce=0x" << hex << uppercase << nonce << dec
             << endl;
    }

    // Derive message-specific keys using consistent big-endian nonce
    MessageKeys messageKeys;
    if (!deriveMessageKeys(baseKeys, nonce, messageKeys)) {
        cerr << "Key derivation failed!" << endl;
        return "";
    }

    // Verify HMAC using consistent big-endian nonce
    uint8_t nonce_be[4];
    nonce_be[0] = static_cast<uint8_t>((nonce >> 24) & 0xFF);
    nonce_be[1] = static_cast<uint8_t>((nonce >> 16) & 0xFF);
    nonce_be[2] = static_cast<uint8_t>((nonce >> 8) & 0xFF);
    nonce_be[3] = static_cast<uint8_t>(nonce & 0xFF);

    vector<uint8_t> hmac_input;
    hmac_input.insert(hmac_input.end(), packet.begin(), packet.end() - HMAC_TAG_LEN);
    hmac_input.insert(hmac_input.end(), nonce_be, nonce_be + 4);

    uint8_t computed_hmac[32];
    hmac_sha256(messageKeys.hmac_key, sizeof(messageKeys.hmac_key),
                hmac_input.data(), hmac_input.size(), computed_hmac);

    // Debug HMAC verification
    {
        ostringstream hk, tp, tc;
        hk << hex << uppercase << setfill('0');
        for (int i = 0; i < 16; ++i) hk << setw(2) << (int)messageKeys.hmac_key[i];
        
        tp << hex << uppercase << setfill('0');
        for (int i = 0; i < HMAC_TAG_LEN; ++i) tp << setw(2) << (int)tag[i];
        
        tc << hex << uppercase << setfill('0');
        for (int i = 0; i < HMAC_TAG_LEN; ++i) tc << setw(2) << (int)computed_hmac[i];

        cout << "[Server] Base HMAC key[0..15]=" << hk.str()
             << " tag(prov)=" << tp.str()
             << " tag(calc)=" << tc.str() << endl;
    }

    // Compare first HMAC_TAG_LEN bytes
    if (memcmp(tag.data(), computed_hmac, HMAC_TAG_LEN) != 0) {
        cerr << "MAC verification failed!" << endl;
        return "";
    }

    // Reverse transformations using per-message keys
    // Transposition (Inverse)
    std::vector<uint8_t> step1 = ct;
    applyTransposition(step1.data(), grid, messageKeys.transposition_key, PermuteMode::Inverse);

    // Tinkerbell (bitwise XOR)
    Tinkerbell tk(messageKeys.tinkerbell_key);
    tk.xorBitwise(step1.data(), step1.size());

    // LFSR (XOR)
    ChaoticLFSR32 lfsr(messageKeys.lfsr_seed, messageKeys.tinkerbell_key, 0x0029u);
    lfsr.xorBuffer(step1.data(), step1.size());

    // Remove salt
    auto desalted = removeSalt(step1, step1.size(), saltMeta);

    // Return as string (truncate to payload length)
    size_t resultLen = min(static_cast<size_t>(payloadLen), desalted.size());
    return string(desalted.begin(), desalted.begin() + resultLen);
}

int main() {
    // Initialize ECDH keys
    if (!server_keys_init()) {
        cerr << "Failed to initialize server ECDH keys" << endl;
        return 1;
    }

    crow::SimpleApp app;

    // Enable CORS
    app.get_middleware<crow::CORSHandler>().global()
        .headers("Content-Type", "Authorization")
        .methods("POST"_method, "GET"_method, "OPTIONS"_method)
        .origin("*");

    // Database connection
    pqxx::connection conn("dbname=health_monitor user=health_user host=localhost port=5432 password=secure_password");
    if (!conn.is_open()) {
        cerr << "Failed to connect to database" << endl;
        return 1;
    }

    // Prepare statements
    pqxx::work txn(conn);
    txn.exec("CREATE TABLE IF NOT EXISTS health_data ("
             "id SERIAL PRIMARY KEY, "
             "heart_rate INTEGER, "
             "spo2 INTEGER, "
             "steps INTEGER, "
             "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
    txn.prepare("insert_health_data", "INSERT INTO health_data (heart_rate, spo2, steps) VALUES ($1, $2, $3)");
    txn.commit();

    // New endpoint: Get server public key for ECDH
    CROW_ROUTE(app, "/public-key").methods("GET"_method)
    ([&](const crow::request& req) {
        try {
            string pubHex = server_pubkey_uncompressed();
            if (pubHex.empty()) {
                crow::response res(500, crow::json::wvalue{{"error", "Failed to get public key"}});
                res.add_header("Access-Control-Allow-Origin", "*");
                return res;
            }

            crow::json::wvalue response;
            response["public_key"] = pubHex;
            
            crow::response res(200, response);
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        } catch (const exception& e) {
            cerr << "Error in /public-key: " << e.what() << endl;
            crow::response res(500, crow::json::wvalue{{"error", "Internal server error"}});
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
    });

    // New endpoint: Receive device public key and derive master key
    CROW_ROUTE(app, "/master-key").methods("POST"_method)
    ([&](const crow::request& req) {
        try {
            cout << "Received POST /master-key request" << endl;
            
            auto json_data = crow::json::load(req.body);
            if (!json_data) {
                crow::response res(400, crow::json::wvalue{{"error", "Invalid JSON"}});
                res.add_header("Access-Control-Allow-Origin", "*");
                return res;
            }

            string devicePubHex = json_data["device_public_key"].s();
            if (devicePubHex.empty()) {
                crow::response res(400, crow::json::wvalue{{"error", "Missing device_public_key"}});
                res.add_header("Access-Control-Allow-Origin", "*");
                return res;
            }

            uint8_t masterKey[32];
            if (!compute_master_from_device_pub_hex(devicePubHex, masterKey)) {
                crow::response res(500, crow::json::wvalue{{"error", "ECDH computation failed"}});
                res.add_header("Access-Control-Allow-Origin", "*");
                return res;
            }

            // Derive base keys from master key
            if (!deriveKeys(masterKey, sizeof(masterKey), baseKeys)) {
                crow::response res(500, crow::json::wvalue{{"error", "Key derivation failed"}});
                res.add_header("Access-Control-Allow-Origin", "*");
                return res;
            }

            masterKeySet = true;

            // Debug output
            {
                ostringstream mk;
                mk << hex << uppercase << setfill('0');
                for (int i = 0; i < 32; ++i) mk << setw(2) << (int)masterKey[i];
                cout << "[Server] Derived master key (full): " << mk.str() << endl;

                mk.str("");
                for (int i = 0; i < 16; ++i) mk << setw(2) << (int)masterKey[i];
                cout << "[Server] Derived master key[0..15]: " << mk.str() << endl;

                ostringstream hk;
                hk << hex << uppercase << setfill('0');
                for (int i = 0; i < 16; ++i) hk << setw(2) << (int)baseKeys.hmac_key[i];
                cout << "[Server] HMAC key[0..15] after /master-key: " << hk.str() << endl;
            }

            cout << "Master key derived and stored" << endl;

            crow::json::wvalue response;
            response["status"] = "ECDH_OK:Keys_Derived";
            
            crow::response res(200, response);
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        } catch (const exception& e) {
            cerr << "Error in /master-key: " << e.what() << endl;
            crow::response res(500, crow::json::wvalue{{"error", "Internal server error"}});
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
    });

    // Health data endpoint
    CROW_ROUTE(app, "/health-data").methods("POST"_method)
    ([&](const crow::request& req) {
        try {
            if (!masterKeySet) {
                crow::response res(400, crow::json::wvalue{{"error", "Master key not set. Call /master-key first."}});
                res.add_header("Access-Control-Allow-Origin", "*");
                return res;
            }

            vector<uint8_t> packet(req.body.begin(), req.body.end());

            // Debug master key
            {
                cout << "=== SERVER MASTER KEY DEBUG ===" << endl;
                ostringstream mk;
                mk << hex << uppercase << setfill('0');
                for (int i = 0; i < 16; ++i) mk << setw(2) << (int)baseKeys.master[i];
                cout << "Master key[0..F]: " << mk.str() << endl;
                
                mk.str("");
                for (int i = 16; i < 32; ++i) mk << setw(2) << (int)baseKeys.master[i];
                cout << "Master key[10..1F]: " << mk.str() << endl;
                cout << "================================" << endl;
            }

            {
                ostringstream hk;
                hk << hex << uppercase << setfill('0');
                for (int i = 0; i < 16; ++i) hk << setw(2) << (int)baseKeys.hmac_key[i];
                cout << "[Server] HMAC key[0..15] at /health-data: " << hk.str() << endl;
            }
            
            string decrypted = pipelineDecryptPacket(baseKeys, packet, packet.size());
            
            if (decrypted.empty()) {
                cerr << "Decryption failed" << endl;
                crow::response res(400, crow::json::wvalue{{"error", "Decryption failed"}});
                res.add_header("Access-Control-Allow-Origin", "*");
                return res;
            }

            // Parse health data
            regex regex_pattern("HR-(\\d+) SPO2-(\\d+) STEPS-(\\d+)");
            smatch match;
            if (!regex_match(decrypted, match, regex_pattern)) {
                cerr << "Invalid data format: " << decrypted << endl;
                crow::response res(400, crow::json::wvalue{{"error", "Invalid data format"}});
                res.add_header("Access-Control-Allow-Origin", "*");
                return res;
            }
            
            int heart_rate = stoi(match[1]);
            int spo2 = stoi(match[2]);
            int steps = stoi(match[3]);

            cout << "Health data received - HR: " << heart_rate << ", SPO2: " << spo2 << ", Steps: " << steps << endl;

            pqxx::work txn(conn);
            txn.exec_prepared("insert_health_data", heart_rate, spo2, steps);
            txn.commit();
            
            crow::json::wvalue response;
            response["status"] = "ENC_OK:Stored";
            
            crow::response res(200, response);
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
            
        } catch (const exception& e) {
            cerr << "Error in /health-data: " << e.what() << endl;
            crow::response res(500, crow::json::wvalue{{"error", "Internal server error"}});
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
    });

    app.port(8081).multithreaded().run();
    return 0;
}