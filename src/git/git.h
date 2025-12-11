#ifndef GIT_H
#define GIT_H

void checkout(const char *directory, const char *headSha);
char* discoverRefs(const char *repoUrl);
unsigned char* requestPackfile(const char *repoUrl, const char *headSha, size_t *packSize);

size_t readDeltaSize(const unsigned char **ptr);
unsigned char* applyDelta(const unsigned char *base, size_t baseSize, const unsigned char *delta, size_t deltaSize, size_t *resultSize);

#endif // GIT_H 