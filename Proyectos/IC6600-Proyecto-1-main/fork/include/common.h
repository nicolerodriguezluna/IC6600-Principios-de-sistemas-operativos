#ifndef FORK_COMMON_H
#define FORK_COMMON_H

/* ======================================================================
 *  COMMON.H — Includes base, helpers y acceso a la implementación Huffman
 * ====================================================================== */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>  
#include <unistd.h>
#include <limits.h>
#include <time.h>

#include "../../huffman/include/huffman.h"
#include "../../huffman/include/frecuencias.h"
#include "../../huffman/include/arbol.h"

#define DIE(...) do {                         \
    fprintf(stderr, "[ERROR] ");              \
    fprintf(stderr, __VA_ARGS__);             \
    fprintf(stderr, "\n");                    \
    exit(EXIT_FAILURE);                       \
} while (0)

#define WARN(...) do {                        \
    fprintf(stderr, "[WARN] ");               \
    fprintf(stderr, __VA_ARGS__);             \
    fprintf(stderr, "\n");                    \
} while (0)

static inline int num_cpus(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0 && n < 256) ? (int)n : 2;
}

static inline long elapsed_ms(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L;
}

static inline bool has_suffix(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return (ls >= lf) && (strcmp(s + (ls - lf), suf) == 0);
}

static inline const char* base_name(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

#endif /* FORK_COMMON_H */
