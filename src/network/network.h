#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>

// Response buffer for curl
typedef struct {
    unsigned char *data;
    size_t size;
} HttpResponse;

// Generic GET request
int httpGet(const char *repoUrl, HttpResponse *response);

// Generic POST request
int httpPost(const char *repoUrl, const char *contentType, 
            const unsigned char *body, size_t bodyLen,
            HttpResponse *response);

// Encode a line with 4-char hex length prefix
// "want <sha>\n" -> "0032want <sha>\n"
int pktLineEncode(const char *line, char *output, size_t outputSize);

// Decode packet-line format from buffer
// Returns bytes consumed, fills 'line' with content
int pktLineDecode(const unsigned char *data, size_t dataLen, 
                char *line, size_t lineSize);

// Create flush packet "0000"
void pktLineFlush(char *output);

#endif // NETWORK_H