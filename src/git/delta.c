#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../network/network.h"
#include "../utils/utils.h"
#include "../storage/object.h"

/**
 * @brief Read variable-length size from delta header
 */
size_t readDeltaSize(const unsigned char **ptr) {
    size_t size = 0;
    int shift = 0;
    unsigned char byte;

    do {
        byte = *(*ptr)++;
        size |= (size_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);

    return size;
}

/**
 * @brief Apply delta instructions to base object
 * 
 * @param base: base object data
 * @param baseSize: size of base object
 * @param delta: decompressed delta data
 * @param deltaSize: size of delta data
 * @param resultSize: OUTPUT - size of resulting object
 * @return unsigned char*: resulting object data (caller must free)
*/
unsigned char* applyDelta(const unsigned char *base, size_t baseSize, const unsigned char *delta, size_t deltaSize, size_t *resultSize) {
    const unsigned char *ptr = delta;
    const unsigned char *deltaEnd = delta + deltaSize;

    // Read base size
    size_t expectedBaseSize = readDeltaSize(&ptr);
    if (expectedBaseSize != baseSize) {
        fprintf(stderr, "Error: Base size mismatch in delta application\n");
        return NULL;
    }

    // Read result size
    *resultSize = readDeltaSize(&ptr);

    // Allocate result buffer
    unsigned char *result = malloc(*resultSize);
    unsigned char *out = result;

    // Process instructions 
    while (ptr < deltaEnd) {
        unsigned char cmd = *ptr++;

        if (cmd & 0x80) {
            // COPY
            size_t offset = 0;
            size_t size = 0;

            // Read offset (little-endian, variable-length)
            if (cmd & 0x01) offset |= (*ptr++) << 0;
            if (cmd & 0x02) offset |= (*ptr++) << 8;
            if (cmd & 0x04) offset |= (*ptr++) << 16;
            if (cmd & 0x08) offset |= (*ptr++) << 24;

            // Read size (little-endian, variable-length)
            if (cmd & 0x10) size |= (*ptr++) << 0;
            if (cmd & 0x20) size |= (*ptr++) << 8;
            if (cmd & 0x40) size |= (*ptr++) << 16;

            // If size of 0 means 0x10000
            if (size == 0) size = 0x10000;

            // Copy from base object
            memcpy(out, base + offset, size);
            out += size;
        } 
        else if (cmd > 0) {
            // INSERT  
            memcpy(out, ptr, cmd);
            ptr += cmd;
            out += cmd;
        } else {
            // cmd == 0 is reserved/invalid
            fprintf(stderr, "Error: Invalid delta instruction 0\n");
            free(result);
            return NULL;
        }
    }
    return result;
}