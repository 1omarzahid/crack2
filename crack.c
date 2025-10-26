#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "md5.h"    // provided md5() implementation

#define INITIAL_HASH_CAP 128
#define MAX_LINE 1024
#define HASH_STR_LEN 33   // 32 hex chars + '\0'

// Read all hashes from hashFilename into a malloc'd array.
// Returns number of hashes via *out_count and an array of char* (malloc'd).
// On failure, returns NULL and *out_count is 0.
char **load_hashes(const char *hashFilename, int *out_count) {
    FILE *hf = fopen(hashFilename, "r");
    if (!hf) {
        perror("Error opening hash file");
        *out_count = 0;
        return NULL;
    }

    int cap = INITIAL_HASH_CAP;
    char **list = malloc(sizeof(char*) * cap);
    if (!list) {
        fclose(hf);
        *out_count = 0;
        return NULL;
    }

    char line[MAX_LINE];
    int n = 0;
    while (fgets(line, sizeof(line), hf)) {
        // trim newline and\r
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
            len--;
        }
        if (len == 0) continue;

        if (n >= cap) {
            cap *= 2;
            char **tmp = realloc(list, sizeof(char*) * cap);
            if (!tmp) {
                // cleanup and bail out
                for (int i = 0; i < n; ++i) free(list[i]);
                free(list);
                fclose(hf);
                *out_count = 0;
                return NULL;
            }
            list = tmp;
        }

        // store a copy of the hash line
        list[n] = strdup(line);
        if (!list[n]) {
            // cleanup
            for (int i = 0; i < n; ++i) free(list[i]);
            free(list);
            fclose(hf);
            *out_count = 0;
            return NULL;
        }
        n++;
    }

    fclose(hf);
    *out_count = n;
    return list;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hash_file> <dictionary_file>\n", argv[0]);
        return 1;
    }

    const char *hashfile = argv[1];
    const char *dictfile = argv[2];

    // load hashes into memory
    int hash_count = 0;
    char **hashes = load_hashes(hashfile, &hash_count);
    if (hashes == NULL) {
        fprintf(stderr, "Failed to load hashes from %s\n", hashfile);
        return 1;
    }

    // Bookkeeping array: whether each hash was already cracked
    bool *cracked = calloc(hash_count, sizeof(bool));
    if (!cracked) {
        perror("calloc");
        for (int i = 0; i < hash_count; ++i) free(hashes[i]);
        free(hashes);
        return 1;
    }

    FILE *df = fopen(dictfile, "r");
    if (df == NULL) {
        perror("Error opening dictionary");
        for (int i = 0; i < hash_count; ++i) free(hashes[i]);
        free(hashes);
        free(cracked);
        return 1;
    }

    char word[MAX_LINE];
    int cracked_count = 0;

    // For each candidate word, compute md5 and compare against all hashes
    while (fgets(word, sizeof(word), df)) {
        // trim newline / CR
        size_t wl = strlen(word);
        while (wl > 0 && (word[wl-1] == '\n' || word[wl-1] == '\r')) {
            word[wl-1] = '\0';
            wl--;
        }
        if (wl == 0) continue;

        // compute md5 (md5 returns a malloc'd string we must free)
        char *digest = md5(word, (int)wl);
        if (digest == NULL) {
            // unexpected, but skip this word if md5 failed
            continue;
        }

        // compare against all hashes
        for (int i = 0; i < hash_count; ++i) {
            if (cracked[i]) continue;              // already found this one
            if (strcmp(digest, hashes[i]) == 0) {
                // found a match
                printf("%s %s\n", hashes[i], word);
                cracked[i] = true;
                cracked_count++;
                // do NOT break here — same word could (theoretically)
                // match multiple hashes if duplicates exist
            }
        }

        free(digest);

        // small optimization: stop if we've cracked everything
        if (cracked_count == hash_count) break;
    }

    fclose(df);

    printf("%d hashes cracked!\n", cracked_count);

    // cleanup
    for (int i = 0; i < hash_count; ++i) free(hashes[i]);
    free(hashes);
    free(cracked);

    return 0;
}
