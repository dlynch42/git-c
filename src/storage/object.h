#ifndef OBJECT_H
#define OBJECT_H

#include <stdio.h>
#include <sys/types.h>
/* 
 * Will Implement later. This is for structure purposes
 * This will cover generic object operations (read any object type, decompress, parse header) 
 * since trees and commits will need similar logic later.
*/

// Data structure for objects; using for all since they follow the same basic format
typedef struct {
    size_t size;
    unsigned char *data;
} Object;

/**
 * Writes an object to .git/objects
 * 
 * @param type    - "blob" or "tree"
 * @param content - the raw content (file data or tree entries)
 * @param size    - size of content in bytes
 * @param outHash - buffer to store resulting 40-char hex SHA (must be 41 bytes)
 * @return 0 on success, -1 on error
 */
int writeObject(const char *type, const unsigned char *content, size_t size, char *outHash);

typedef struct {
    char mode[8];
    char name[256];
    unsigned char rawsha[20]; // raw SHA-1 bytes
} Entry;

/**
 * tree <tree_sha>
 * parent <parent_sha>
 * author <name> <email> <timestamp> <timezone>
 * committer <name> <email> <timestamp> <timezone>
 */
typedef struct {
    char author[256];
    char timestamp[64];
    char *treesha; // tree sha
    char *parentsha; // parent commit sha(s) if any
    char *message;
} Commit;

/**
 * @brief object type enum
 * 
 * @note pack-*.pack files have the following format:
 *  A header appears at the beginning and consists of the following:
 *       4-byte signature
 *           The signature is: {'P', 'A', 'C', 'K'}
 *       4-byte version number (network byte order):
 *           Git currently accepts version number 2 or 3 but generates version 2 only.
 *       4-byte number of objects contained in the pack (network byte order)
 *       Observation: we cannot have more than 4G versions ;-) and more than 4G objects in a pack.
 *  
 *  The header is followed by a number of object entries, each of which looks like this:
 *       (undeltified representation)
 *          n-byte type and length (3-bit type, (n-1)*7+4-bit length) compressed data
 *  
 *       (deltified representation)
 *           n-byte type and length (3-bit type, (n-1)*7+4-bit length)
 *           base object name if OBJ_REF_DELTA or a negative relative offset from the delta object's position in the pack if this is an OBJ_OFS_DELTA object
 *           compressed delta data
 *       Observation: the length of each object is encoded in a variable length format and is not constrained to 32-bit or anything.
 *  
 *  The trailer records a pack checksum of all of the above.
 * 
 */
typedef enum {
    OBJ_COMMIT=1, 
    OBJ_TREE=2,
    OBJ_BLOB=3,
    OBJ_TAG=4,
    OBJ_OFS_DELTA=6,
    OBJ_REF_DELTA=7,
} ObjectType;

/**
 * @brief pack file header
 * 
 * @note Pack file header structure:
 *       version: Pack file version, usually 2 or 3
 *       objects: Number of objects in the pack
 * 
 */
typedef struct {
    int version;
    int objects;
} PackHeader;

/** 
 * @brief pack object
 * @note Pack object structure:
 *      type: Object type (commit, tree, blob, tag, ofs-delta, ref-delta)
 *      size: Size of the object data after decompression
 *      data: Decompressed object data
 *      basesha: For REF_DELTA: SHA of base object
 *      baseoffset: For OFS_DELTA: Offset of base object
 */
typedef struct {
    ObjectType type;
    size_t size;         // size of the object data after decompression
    unsigned char *data; // decompressed object data
    unsigned char basesha[20]; // for REF_DELTA: SHA of base object
    size_t baseoffset;         // for OFS_DELTA: offset of base object
} Pack;


/**
 * @brief unpacked object structure
 */
typedef struct {
    char sha[41];
    ObjectType type;
    unsigned char *data;
    size_t size;
    size_t packOffset;  // Position in pack (for OFS_DELTA)
} UnpackedObject;

PackHeader readPackHeader(const unsigned char *data, size_t dataLen);
void unpack(unsigned char *packData, size_t packSize, const char *directory);
static size_t readDeltaSize(const unsigned char **ptr);
unsigned char* applyDelta(const unsigned char *base, size_t baseSize, const unsigned char *delta, size_t deltaSize, size_t *resultSize);
int readTypeAndSize(const unsigned char *data, int * type, size_t *size);

#endif // OBJECT_H