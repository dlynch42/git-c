#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../utils/utils.h"
#include "../storage/object.h"
#include "../git/git.h"
#include "../network/network.h"
#include "../cmd/cmd.h"

/**
 * @brief clone command 
 * 
 * @param argc len of argv
 * @param argv clone <https://github.com/blah/blah> <some_dir>
 * @return int 
 */

int clone(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: clone <repo_url> <directory>\n");
        return 1;
    }

    char *repoUrl = argv[2];
    char *directory = argv[3];

    // create directory and init git
    mkdir(directory, 0755);
    char originalDir[256];
    getcwd(originalDir, sizeof(originalDir));
    chdir(directory);
    init();

    // discover refs (HTTP GET)
    //    GET https://github.com/user/repo.git/info/refs?service=git-upload-pack
    char *headSha = discoverRefs(repoUrl);
    if (!headSha || strlen(headSha) == 0) {
        fprintf(stderr, "Error: Could not discover refs from %s\n", repoUrl);
        return 1;
    }
    printf("HEAD SHA: %s\n", headSha);

    // Request packfile
    //    POST https://github.com/user/repo.git/git-upload-pack
    //    Body: "want <sha>\n... done\n"
    size_t packSize;
    unsigned char *packData = requestPackfile(repoUrl, headSha, &packSize);
    if (!packData) {
        fprintf(stderr, "Error: Could not request packfile from %s\n", repoUrl);
        free(headSha);
        return 1;
    }
    printf("Received packfile of size %zu bytes\n", packSize);

    // Parse and unpqck the packfile
    unpack(packData, packSize, directory);

    // checkout HEAD (read commit -> read tree -> write files)
    checkout(".", headSha);

    // cleanup
    free(headSha);
    free(packData);
    chdir(originalDir);

    return 0;
}