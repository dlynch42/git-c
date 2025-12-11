#include <stdio.h>

/**
 * @brief Build the path to the object file based on its hash
 * 
 * @param hash The SHA-1 hash of the object
 * @return char* The path to the object file
 */
char *buildPath(const char *hash) {
    static char path[256];
    snprintf(path, sizeof(path), ".git/objects/%c%c/%s", hash[0], hash[1], hash + 2);
    return path;
}