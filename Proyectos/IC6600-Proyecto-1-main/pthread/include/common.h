#ifndef COMMON_H
#define COMMON_H

/* ======================================================================
 *  COMMON.H — Utilidades, includes comunes y helpers genéricos
 * ====================================================================== */

 /* --- C y POSIX base --- */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>

/* --- Se importa la implementacion de Huffman --- */
#include "../../huffman/include/huffman.h"
#include "../../huffman/include/frecuencias.h"
#include "../../huffman/include/arbol.h"

/* --- Logging de errores estandar del proyecto --- */
#define DIE(...)                      \
    do                                \
    {                                 \
        fprintf(stderr, "[ERROR] ");  \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
        exit(EXIT_FAILURE);           \
    } while (0)
#define WARN(...)                     \
    do                                \
    {                                 \
        fprintf(stderr, "[WARN] ");   \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
    } while (0)

/* --- Deteccion de CPUs disponibles para dimensionar el threadpool --- */
static inline int num_cpus(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0 && n < 256) ? (int)n : 2;
}

/* --- Chequeo de sufijo de archivo (".txt", ".hfa", etc.) --- */
static inline bool has_suffix(const char *s, const char *suf)
{
    size_t ls = strlen(s), lf = strlen(suf);
    return (ls >= lf) && (strcmp(s + (ls - lf), suf) == 0);
}

/* --- Calculo de milisegundos entre dos timespec para metricas --- */
static inline long elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000
         + (end.tv_nsec - start.tv_nsec) / 1000000;
}


#endif
