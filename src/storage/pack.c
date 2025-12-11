#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <arpa/inet.h>  // for ntohl (network to host byte order)
#include "object.h"
#include "../utils/utils.h"
#include "../git/git.h"

/**
 * @brief read pack header
 * 
 * @param data: raw pack file data
 * @param dataLen: length of pack file data
 * @return PackHeader: parsed pack header
 */
PackHeader readPackHeader(const unsigned char *data, size_t dataLen) {
    PackHeader header = {0, 0};

    if (dataLen < 12) {
        fprintf(stderr, "Error: Pack data too small to contain header\n");
        return header;
    }

    // Check signature "PACK"
    if (memcmp(data, "PACK", 4) != 0) {
        fprintf(stderr, "Error: Invalid pack file signature\n");
        return header;
    }

    // Version (big-endian)
    header.version = ntohl(*(uint32_t *)(data + 4));
    if (header.version != 2) {
        fprintf(stderr, "Error: Unsupported pack version %d\n", header.version);
        return header;
    }

    // Object count (big-endian)
    header.objects = ntohl(*(uint32_t *)(data + 8));

    return header;
}

/**
 * @brief read variable-length size encoding
 * 
 * @param data: raw pack data
 * @param type: pointer to store object type
 * @param size: pointer to store object size
 * @return int: bytes consumed, fills type, and size
 */
int readTypeAndSize(const unsigned char *data, int * type, size_t *size) {
    // First byte: CTTTSSSS
    // C: continuation bit, TTT: type, SSSS: size bits
    // TTT: 3 bits for type
    // SSSS: 4 bits for size

    int offset = 0;
    unsigned char byte = data[offset++];

    *type = (byte >> 4) & 0x07; // extract type (bits 4-6)
    *size = byte & 0x0F;        // extract initial size bits (bits 0-3)

    int shift = 4;
    while (byte & 0x80) { // Continue bit set
        byte = data[offset++];
        *size |= (size_t)(byte & 0x7F) << shift; // bitwise OR for size
        shift += 7;
    }

    return offset;
};

/**
 * @brief Decompress zlib data
 * 
 * @param compressed: pointer to compressed data
 * @param compLen: length of compressed data
 * @param decompressed: pointer to buffer for decompressed data 
 * @param decompSize: size of decompressed buffer 
 * @return int: decompressed size, fills outData
 */
int zlibDecompress(const unsigned char *compressed, size_t compLen, unsigned char *decompressed, size_t decompSize, size_t *compressedUsed) {
    z_stream stream = {0};
    stream.next_in = (unsigned char *)compressed;
    stream.avail_in = compLen;
    stream.next_out = decompressed;
    stream.avail_out = decompSize;

    if (inflateInit(&stream) != Z_OK) return -1;

    int ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (ret != Z_STREAM_END) return -1;

    *compressedUsed = stream.total_in;

    return stream.total_out;
}

/**
 * @brief read object from .git/objects by sha
 * 
 * @param hexSha 
 * @return UnpackedObject* 
 */
UnpackedObject* readObjectBySha(const char *hexSha) {
    // build path: .git/objects/xx/yyyy...
    char path[256];
    snprintf(path, sizeof(path), ".git/objects/%c%c/%s", hexSha[0], hexSha[1], hexSha + 2);

    // Read and decompress object file
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
    unsigned long decompSize = filesize* 10;
    unsigned char *decompressed = malloc(decompSize);
    uncompress(decompressed, &decompSize, compressed, filesize);
    free(compressed);

    // Parse header: "<type> <size>\0content"
    UnpackedObject *obj = malloc(sizeof(UnpackedObject));
    strcpy(obj->sha, hexSha);

    char *header = (char *)decompressed;
    char *space = strchr(header, ' ');
    char *nullByte = strchr(header, '\0');

    // Determine type
    if (strncmp(header, "blob", 4) == 0) {
        obj->type = OBJ_BLOB;
    } else if (strncmp(header, "tree", 4) == 0) {
        obj->type = OBJ_TREE;
    } else if (strncmp(header, "commit", 6) == 0) {
        obj->type = OBJ_COMMIT;
    } else {
        fprintf(stderr, "Error: Unknown object type in %s\n", path);
        free(decompressed);
        free(obj);
        return NULL;
    }
    // Get size from header
    obj->size = atol(space + 1);

    // Allocate memory for data
    obj->data = malloc(obj->size);
    
    // Copy content
    memcpy(obj->data, nullByte + 1, obj->size);

    free(decompressed);
    return obj;
}

/**
 * @brief read object from pack by offset
 * 
 * @param packData 
 * @param packSize 
 * @param offset 
 * @return UnpackedObject* 
 */
UnpackedObject* readObjectByOffset(const unsigned char *packData, size_t packSize, size_t offset) {
    const unsigned char *ptr = packData + offset;

    int type;
    size_t size;
    int consumed = readTypeAndSize(ptr, &type, &size);
    ptr += consumed;

    // If its another delta type, we would need to handle recursively
    if (type != OBJ_COMMIT && type != OBJ_TREE && type != OBJ_BLOB) {
        fprintf(stderr, "Warning: Nested delta not fully supported\n");
        return NULL;
    }

    // Decompress
    unsigned char *decompressed = malloc(size);
    size_t compressedUsed;
    zlibDecompress(ptr, packSize - (ptr - packData), decompressed, size, &compressedUsed);

    UnpackedObject *obj = malloc(sizeof(UnpackedObject));

    obj->type = type;
    obj->size = size;
    obj->data = decompressed;
    obj->packOffset = offset;

    return obj;
}

/**
 * @brief unpack pack files
 * 
 * @param packdata: raw pack file data
 * @param packSize: size of pack file data 
 * @param directory: git/objects/pack/pack-<hash>.pack
 */
void unpack(unsigned char *packData, size_t packSize, const char *directory) {
    PackHeader header = readPackHeader(packData, packSize);

    if (header.objects == 0) {
        fprintf(stderr, "Error: No objects in pack file\n");
        return;
    }

    printf("Pack version: %d, objects: %d\n", header.version, header.objects);
    unsigned char *ptr = packData + 12; // skip header
    size_t remaining = packSize - 12 -20; // exclude header and trailer

    for (int i = 0; i < header.objects; i++) {
        size_t objStart = ptr - packData;  // Save position BEFORE reading type/size
        int type;
        size_t size;
        int consumed = readTypeAndSize(ptr, &type, &size);
        ptr += consumed;

        // Handle different object types
        // - For OBJ_COMMIT, OBJ_TREE, OBJ_BLOB: decompress and write
        if (type == OBJ_COMMIT || type == OBJ_TREE || type == OBJ_BLOB) {
            // Decompress object
            unsigned char *decompressed = malloc(size);
            size_t compressedUsed;
            int decompSize = zlibDecompress(ptr, remaining, decompressed, size, &compressedUsed);
            if (decompSize < 0) {
                fprintf(stderr, "Error: Failed to decompress object %d\n", i);
                free(decompressed);
                return;
            }

            // Move ptr forward
            ptr += compressedUsed;
            remaining -= compressedUsed;

            // Write object to .git/objects/
            char hexSha[41];
            const char *typeName = (type == OBJ_COMMIT) ? "commit" : 
                                   (type == OBJ_TREE) ? "tree" : "blob";
            writeObject(typeName, decompressed, decompSize, hexSha);
            printf("Unpacked object %d: %s\n", i, hexSha);

            free(decompressed);
        }
        // - For OBJ_REF_DELTA: read 20-byte base SHA, decompress delta, apply
        else if (type == OBJ_REF_DELTA) {
            // Read base SHA
            unsigned char baseSha[20];
            memcpy(baseSha, ptr, 20);
            ptr += 20;
            remaining -= 20;

            // Convert to hex for lookup
            char basHex[41];
            rawToHex(baseSha, basHex);

            // Decompress delta data
            unsigned char *decompressed = malloc(size);
            size_t compressedUsed;
            int deltaSize = zlibDecompress(ptr, remaining, decompressed, size, &compressedUsed);
            if (deltaSize < 0) {
                fprintf(stderr, "Error: Failed to decompress ref-delta object %d\n", i);
                free(decompressed);
                return;
            }

            // Move ptr past compressed data
            ptr += compressedUsed;
            remaining -= compressedUsed;

            // Apply delta to base object
            // 1. Read base object from .git/objects using baseSha
            UnpackedObject *baseObj = readObjectBySha(basHex);
            
            // 2. Apply delta instructions to produce final object
            size_t resultSize;
            unsigned char *resultData = applyDelta(baseObj->data, baseObj->size, decompressed, deltaSize, &resultSize);
            
            // 3. Write final object
            char hexSha[41];
            const char *typeName = (baseObj->type == OBJ_COMMIT) ? "commit" : 
                                   (baseObj->type == OBJ_TREE) ? "tree" : "blob";
            writeObject(typeName, resultData, resultSize, hexSha);

            free(decompressed);
            free(resultData);
            free(baseObj->data);
            free(baseObj);
        }
        // - For OBJ_OFS_DELTA: read offset, decompress delta, apply
        else if (type == OBJ_OFS_DELTA) {
            // read offset (variable-length)
            size_t offset = 0;
            unsigned char byte = *ptr++;
            remaining--;
            offset = byte & 0x7F;
            while (byte & 0x80) {
                byte = *ptr++;
                remaining--;
                offset = ((offset + 1) << 7) | (byte & 0x7F);
            }

            // Decompress delta data
            unsigned char *decompressed = malloc(size);
            size_t compressedUsed;
            int deltaSize = zlibDecompress(ptr, remaining, decompressed, size, &compressedUsed);

            ptr += compressedUsed;
            remaining -= compressedUsed;
            
            if (deltaSize < 0) {
                fprintf(stderr, "Error: Failed to decompress ofs-delta object %d\n", i);
                free(decompressed);
                return;
            }

            // Apply delta to base object at (objStart - offset)
            // 1. Locate base object using offset from object start
            size_t basePos = objStart - offset;
            // 2. Read base object from .git/objects
            UnpackedObject *baseObj = readObjectByOffset(packData, packSize, basePos);

            // 3. Apply delta instructions to produce final object
            size_t resultSize;
            unsigned char *resultData = applyDelta(baseObj->data, baseObj->size, decompressed, deltaSize, &resultSize);
            // 4. Write final object
            char hexSha[41];
            const char *typeName = (baseObj->type == OBJ_COMMIT) ? "commit" : 
                                   (baseObj->type == OBJ_TREE) ? "tree" : "blob";
            writeObject(typeName, resultData, resultSize, hexSha);

            free(decompressed);
            free(resultData);
            free(baseObj->data);

        }
    }

}
