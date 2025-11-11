cmake_minimum_required(VERSION 3.15)
project(XenoCipherServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define Windows target version for ASIO
add_definitions(-D_WIN32_WINNT=0x0601) # Windows 7 or later

# Try to find Crow via vcpkg first, then fallback to manual installation
find_package(Crow QUIET)
if(NOT Crow_FOUND)
    message(STATUS "Crow not found via vcpkg, trying manual installation...")
    set(CROW_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    if(EXISTS "${CROW_INCLUDE_DIR}/crow.h" AND EXISTS "${CROW_INCLUDE_DIR}/crow/app.h")
        message(STATUS "Found Crow at ${CROW_INCLUDE_DIR}")
        add_library(Crow::Crow INTERFACE IMPORTED)
        set_target_properties(Crow::Crow PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${CROW_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "Crow not found! Please install Crow either via vcpkg or manually in server/include/")
    endif()
endif()

# Find other packages
find_package(MbedTLS REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

# XenoCipher lib includes (relative to server/ directory)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/NTRU/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/CryptoKDF/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/HMAC/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/LFSR/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/Tinkerbell/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/Transposition/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common
    ${MbedTLS_INCLUDE_DIRS}
)

# Crow include directory is already set via the Crow::Crow target

# libpqxx is provided by vcpkg; no manual include/lib paths needed

# Source files
add_executable(server
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/NTRU/src/ntru.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/CryptoKDF/src/crypto_kdf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/HMAC/src/hmac.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/LFSR/src/lfsr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/Tinkerbell/src/tinkerbell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/Transposition/src/transposition.cpp
)

# Link libraries
target_link_libraries(server PRIVATE
    Crow::Crow
    libpqxx::pqxx
    PostgreSQL::PostgreSQL
    MbedTLS::mbedtls
    MbedTLS::mbedcrypto
)