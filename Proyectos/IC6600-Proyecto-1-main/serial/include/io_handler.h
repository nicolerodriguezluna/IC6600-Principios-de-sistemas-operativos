#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include "../../huffman/include/huffman.h"

int comprimir_archivo(const char* nombre_archivo, struct Nodo* raiz, char** tabla_codigos, FILE* archivo_salida);
int descomprimir_archivo(FILE* archivo_entrada, const char* directorio_salida);
int comprimir_directorio(const char* directorio_entrada, const char* archivo_salida);
int descomprimir_archivo_completo(const char* archivo_entrada, const char* directorio_salida);

#endif