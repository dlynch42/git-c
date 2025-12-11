#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../network/network.h"
#include "../utils/utils.h"
#include "../storage/object.h"

/**
 * @brief discover refs (HTTP GET)
 * @note GET https://github.com/user/repo.git/info/refs?service=git-upload-pack
 * 
 * @param repoUrl: repository URL
 * @return char*: HEAD SHA
 */
char* discoverRefs(const char *repoUrl) {
    char url[512];
    if (strstr(repoUrl, ".git") == NULL) {
      snprintf(url, sizeof(url), "%s.git/info/refs?service=git-upload-pack", repoUrl);
    } else {
        snprintf(url, sizeof(url), "%s/info/refs?service=git-upload-pack", repoUrl);
    }

    HttpResponse response;
    if (httpGet(url, &response) != 0) {
        return NULL;
    };

    // parse pktline response to find "refs/heads/master"
    char *headSha = malloc(41);
    headSha[0] = '\0';

    unsigned char *ptr = response.data;
    size_t remaining = response.size;

    while (remaining > 0) {
        char line[512];
        int consumed = pktLineDecode(ptr, remaining, line, sizeof(line));

        if (consumed <= 0) {
            break; // error or done
        }

        ptr += consumed;
        remaining -= consumed;

        // Skip flush packets and service announcment
        if (consumed == 4 || line[0] == '#') {
            continue;
        }

        // Look for head reference
        // Format: "<sha> HEAD\0" or "<sha> refs/heads/master"
        if (strlen(line) >= 40) {
            //first 40 chars are sha
            if (strstr(line, "HEAD") || strstr(line, "refs/heads/master")) {
                strncpy(headSha, line, 40);
                headSha[40] = '\0';
                break;
            }
        }
    }

    free(response.data);
    return headSha;
}

/**
 * @brief Request packfile (HTTP POST)
 * 
 * @note POST https://github.com/user/repo.git/git-upload-pack
 * @note Body: "want <sha>\n... done\n"
 * 
 * @param repoUrl: repository URL
 * @param headSha: HEAD SHA
 * @return char: raw packfile data
 */
unsigned char* requestPackfile(char *repoUrl, char *headSha, size_t *packSize) {
    // Build url
    char fullUrl[512];
    if (strstr(repoUrl, ".git") == NULL) {
        snprintf(fullUrl, sizeof(fullUrl), "%s.git/git-upload-pack", repoUrl);
    } else {
        snprintf(fullUrl, sizeof(fullUrl), "%s/git-upload-pack", repoUrl);
    }
    
    // Build the want request:
    // Format: 4-char length + "want " + sha + "\n"
    // Example: "0032want 3b18e512dba79e4c8300dd08aeb37f8e728b8dad\n"
    // 0x32 = 50 decimal = 4 + 5 + 40 + 1
    char body[256];
    int offset = 0;

    // "want <sha> <capabilites>\n"
    char wantLine[128];
    snprintf(wantLine, sizeof(wantLine), "want %s multi_ack\n", headSha);
    offset += pktLineEncode(wantLine, body + offset, sizeof(body) - offset);

    // Flush
    pktLineFlush(body + offset);
    offset += 4;

    // "done\n"
    offset += pktLineEncode("done\n", body + offset, sizeof(body) - offset);

    for (int i = 0; i < offset; i++) {
        fprintf(stderr, "%02x ", (unsigned char)body[i]);
    }
    fprintf(stderr, "\n");

    // Post request
    HttpResponse response;
    if (httpPost(fullUrl, "application/x-git-upload-pack-request", (unsigned char *)body, offset, &response)!= 0) {
        return NULL;
    };

    // Skip NAK line, return packfile data
    // Response: "0008NAK\n"<packfile data>
    unsigned char *ptr = response.data;
    size_t remaining = response.size;

    while (remaining > 4) {
        // Check if we've reached PACK data
        if (memcmp(ptr, "PACK", 4) == 0) {
            break;
        }

        // Check for sideband: if first byte after length is \1, \2, or \3
        // Skip the pkt-line
        char line[512];
        int consumed = pktLineDecode(ptr, remaining, line, sizeof(line));

        if (consumed <= 0) {
            // Not a valid pkt-line, maybe we're at PACK data
            break;
        }

        ptr += consumed;
        remaining -= consumed;
    }

    // Now ptr should point to PACK data
    unsigned char *packData = malloc(remaining);
    memcpy(packData, ptr, remaining);
    *packSize = remaining;

    free(response.data);
    return packData;
}

