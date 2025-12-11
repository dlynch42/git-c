#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
#include "../utils/utils.h"
#include "../storage/object.h"

/*
Checkout flow:
headSha (commit)
    → parse commit, extract tree SHA
        → read tree object
            → for each entry:
                - if blob: read blob, write file
                - if tree: mkdir, recurse

Commit object format (text):
tree <tree_sha>
parent <parent_sha>
author ...
committer ...
*/

// Forward declaration
static void checkoutTree(const char *treeSha, const char *basePath);

/**
 * @brief Read object from .git/objects and return decompressed content
 * 
 * @param hexSha: 40-char hex SHA
 * @param outSize: size of decompressed content
 * @param outType: OUTPUT - object type ("blob", "tree", "commit")
 * @return unsigned* decompressed content (caller must free)
 */
static unsigned char* readObject(const char *hexSha, size_t *outSize, char *outType) {
    char path[256];
    snprintf(path, sizeof(path), ".git/objects/%.2s/%s", hexSha, hexSha + 2);

    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open object file %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char *compressed = malloc(filesize);
    fread(compressed, 1, filesize, file);
    fclose(file);

    // Decompress 
    unsigned long decompSize = filesize * 20; // estimate
    unsigned char *decompressed = malloc(decompSize);

    if (uncompress(decompressed, &decompSize, compressed, filesize) != Z_OK) {
        fprintf(stderr, "Error: Failed to decompress object %s\n", hexSha);
        free(compressed);
        free(decompressed);
        return NULL;
    }
    free(compressed);

    // Parse header: "<type> <size>\0content"
    char *header = (char *)decompressed;
    char *space = strchr(header, ' ');
    char *nullByte = memchr(header, '\0', decompSize);

    if (!space || !nullByte) {
        fprintf(stderr, "Error: Invalid object format for %s\n", hexSha);
        free(decompressed);
        return NULL;
    }

    // Copy type
    if (outType) {
        int typeLen = space - header;
        strncpy(outType, header, typeLen);
        outType[typeLen] = '\0';
    }

    // Get content size
    *outSize = atol(space + 1);

    // Return content (after null byte)
    size_t headerLen = (nullByte - header) + 1;
    unsigned char *content = malloc(*outSize);
    memcpy(content, decompressed + headerLen, *outSize);

    free(decompressed);
    return content;
}

/**
 * @brief Get the Tree From Commit object
 * 
 * @param commitSha: commit SHA
 * @return char* tree SHA (caller must free)
 */
static char* getTreeFromCommit(const char *commitSha) {
    size_t size;
    char type[16];
    unsigned char *content = readObject(commitSha, &size, type);

    if (!content || strcmp(type, "commit") != 0) {
        fprintf(stderr, "Error: %s is not a commit\n", commitSha);
        free(content);
        return NULL;
    }

    // Find "tree <sha>" line
    // Format is "tree <40-char-sha>\n"
    if (strncmp((char *)content, "tree ", 5) != 0) {
        fprintf(stderr, "Error: Invalid commit format in %s\n", commitSha);
        free(content);
        return NULL;
    }

    char *treeSha = malloc(41);
    strncpy(treeSha, (char *)content + 5, 40);
    treeSha[40] = '\0';

    free(content);
    return treeSha;
}

/**
 * @brief write a blob to a file
 * 
 * @param blobSha 
 * @param filePath 
 */
static void writeBlob(const char *blobSha, const char *filePath) {
    size_t size;
    char type[16];
    unsigned char *content = readObject(blobSha, &size, type);

    if (!content) {
        fprintf(stderr, "Error: Could not read blob %s\n", blobSha);
        free(content);
        return;
    }

    FILE *file = fopen(filePath, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filePath);
        free(content);
        return;
    }

    fwrite(content, 1, size, file);
    fclose(file);
    free(content);
}

/**
 * @brief checkout a tree recursively
 * 
 * @param treeSha 
 * @param basePath 
 */
static void checkoutTree(const char *treeSha, const char *basePath) {
    size_t size;
    char type[16];
    unsigned char *content = readObject(treeSha, &size, type);

    if (!content || strcmp(type, "tree") != 0) {
        fprintf(stderr, "Error: %s is not a tree\n", treeSha);
        free(content);
        return;
    } 

    // Prase tree entries
    // Format: "<mode> <name>\0<20-byte-sha>"
    unsigned char *ptr = content;
    unsigned char *end = content + size;

    while (ptr < end) {
        // Read mode
        char *mode = (char *)ptr;
        while (*ptr != ' ') ptr++;
        *ptr++ = '\0'; // null-terminate mode AND advance past it

        // Read name (now ptr points to start of name)
        char *name = (char *)ptr;
        while (*ptr != '\0') ptr++;
        ptr++; // skip null byte

        // Read SHA (20 bytes)
        unsigned char rawSha[20];
        memcpy(rawSha, ptr, 20);
        ptr += 20;

        // Convert to hex
        char hexSha[41];
        rawToHex(rawSha, hexSha);
        
        // Build full path
        char fullPath[512];
        if (strlen(basePath) > 0) {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, name);
        } else {
            snprintf(fullPath, sizeof(fullPath), "%s", name);
        }

        // Check if directory (mode starts with '4') or file
        if (mode[0] == '4') {
            // Directory - create and recurse
            mkdir(fullPath, 0755);
            checkoutTree(hexSha, fullPath);
        } else {
            // File - write blob
            writeBlob(hexSha, fullPath);
        }
    }
    
    free(content);
}

/**
 * @brief checkout a commit into a directory
 * 
 * @param directory 
 * @param headSha 
 */
void checkout(char *directory, char *headSha) {
    printf("Checking out commit %s into directory %s\n", headSha, directory);

    // Get tree SHA from commit
    char *treeSha = getTreeFromCommit(headSha);
    if (!treeSha) {
        fprintf(stderr, "Error: Could not get tree from commit %s\n", headSha);
        return;
    }
    printf("Tree SHA: %s\n", treeSha);

    // Recursively checkout tree
    checkoutTree(treeSha, directory);

    free(treeSha);
    printf("Checkout complete.\n");
}