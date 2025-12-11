#include "utils.h"
#include <errno.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/**
 * @brief Compute SHA-1 hash of data
 * 
 * @param data: pointer to data
 * @param length: length of data
 * @param outHash: OUTPUT - 40-char hex SHA-1 hash
 * @return char*: pointer to outHash
 */
char* hash(const char *data, size_t length, char *outHash) {
    unsigned char hash[SHA_DIGEST_LENGTH]; // SHA1 produces a 20-byte hash
    SHA1((unsigned char *)data, length, hash); // From OPENSSL; compute SHA1 hash

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(&outHash[i * 2], "%02x", hash[i]);
    }
    outHash[SHA_DIGEST_LENGTH * 2] = '\0'; // Null terminate the string

    return 0;
}

/**
 * @brief Convert hex SHA-1 string to raw bytes
 * 
 * @param hex: 40-char hex SHA-1 string
 * @param raw: OUTPUT - 20-byte raw SHA-1
 */
void hexToRaw(const char *hex, unsigned char *raw) {
    for (int i = 0; i < 20; i++) {
        sscanf(hex + (i * 2), "%2hhx", &raw[i]);
    }
}

/**
 * @brief Convert raw SHA-1 bytes to hex string
 * 
 * @param raw: 20-byte raw SHA-1
 * @param hex: OUTPUT - 40-char hex SHA-1 string
 */
void rawToHex(const unsigned char *raw, char *hex) {
    for (int i = 0; i < 20; i++) {
        sprintf(&hex[i * 2], "%02x", raw[i]);
    }
    hex[40] = '\0'; // Null terminate
}