#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../storage/object.h"

/**
 * @brief Compare two Entry structs by name for qsort
 * 
 * @param a 
 * @param b 
 * @return int 
 */
int compareEntries(const void *a, const void *b) {
    return strcmp(((Entry *)a)->name, ((Entry *)b)->name);
}