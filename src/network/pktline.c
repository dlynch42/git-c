#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"

/**
 * @brief Encode a line with 4-char hex length prefix
 *        "want <sha>\n" -> "0032want <sha>\n"
 * 
 * @param line 
 * @param output 
 * @param outputSize 
 * @return total bytes written (including 4-char prefix)
 */
int pktLineEncode(const char *line, char *output, size_t outputSize) {
    size_t lineLen = strlen(line);
    size_t totalLen = lineLen + 4; // 4 bytes for length prefix

    if (outputSize < totalLen) {
        return -1; // not enough space; buffer too small
    }

    // Write 4-char hex length, includes the chars itself
    snprintf(output, 5, "%04x", (unsigned int)totalLen);

    // copy the line content
    memcpy(output + 4, line, lineLen);

    return (int)totalLen;
};

/**
 * @brief Decode packet-line format from buffer
 * 
 * @param data 
 * @param dataLen 
 * @param line 
 * @param lineSize 
 * @return bytes consumed, fills 'line' with content (null-terminated); -1 on error
 */
int pktLineDecode(const unsigned char *data, size_t dataLen, char *line, size_t lineSize) {
    if (dataLen < 4) {
        return -1; // not enough data for length prefix
    }

    // Parse 4-char hex length
    char lenStr[5] = {0};
    memcpy(lenStr, data, 4);

    unsigned int pktLen;
    if (sscanf(lenStr, "%04x", &pktLen) != 1) {
        return -1; // invalid length format
    }

    // Flush packet
    if (pktLen == 0) {
        if (line && lineSize > 0) {
            line[0] = '\0'; // empty line
        }
        return 4;
    }

    // Check we have enough data
    if (dataLen < pktLen) {
        return -1; // not enough data
    }

    // Content length (excluding the 4-byte length prefix)
    size_t contentLen = pktLen - 4;

    // Copy content to output
    if (line && lineSize > 0) {
        size_t copyLen = (contentLen < lineSize - 1) ? contentLen : lineSize - 1;
        memcpy(line, data + 4, copyLen);
        line[copyLen] = '\0'; // null-terminate
    }

    return (int)pktLen;
};

/**
 * @brief Create flush packet "0000"
 * 
 * @param output 
 */
void pktLineFlush(char *output) {
    memcpy(output, "0000", 4);
};

