#include <stdio.h>
#include <stdint.h>
#include "djb2.h"

/* from http://www.cse.yorku.ca/~oz/hash.html */
uint64_t djb2_hash(const char *str, size_t len)
{
    uint64_t hash = 5381;
    int i;

    for (i = 0; i < len; i++) {
        int c = str[i];
        /* hash * 33 + c */
        hash = ((hash << 5) + hash) + c;
        /* hash * 33 ^ c */
        hash = ((hash << 5) + hash) ^ c;
    }
    return hash;
}

#ifdef LINUX_BUILD

#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage: %s <file>\n", argv[0]);
        exit(-1);
    }
    printf("%lx %s\n", djb2_hash(argv[1], strlen(argv[1])), argv[1]);
    return 0;
}
#endif
