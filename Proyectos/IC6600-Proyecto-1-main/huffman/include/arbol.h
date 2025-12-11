#ifndef ARBOL_H
#define ARBOL_H

#include "huffman.h"

void encontrar_dos_minimos(struct ListaNodos lista, int* min1, int* min2);
struct Nodo* construir_arbol_huffman(struct ListaNodos lista_inicial);
void imprimir_arbol(struct Nodo* raiz, int nivel);
void liberar_arbol(struct Nodo* nodo);
void generar_codigos_huffman(struct Nodo* raiz, char* codigo, int profundidad, char** tabla);
char* comprimir_texto(const char* texto, char** tabla);
char* descomprimir_texto(struct Nodo* raiz, const char* texto_comprimido, long tamaño_esperado);
int serializar_arbol(struct Nodo* raiz, FILE* archivo);
struct Nodo* deserializar_arbol(FILE* archivo);

#endif