#include <stdint.h>
#include <stdio.h>

// http://www.cse.yorku.ca/~oz/hash.html
uint32_t djb2_hash_string(const char *string) {
    uint32_t hash = 5381;
    
    int c;
    while ((c = *string++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}

int main(int argc, char **argv) {
    if (argc!=2) {
        printf("hashstr (string)\n");
        return 0;
    }

    uint32_t hash = djb2_hash_string(argv[1]);
    printf("%s = 0x%08x\n", argv[1], hash);
    return 0;
}