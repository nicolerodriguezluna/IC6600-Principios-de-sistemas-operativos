#ifndef FRECUENCIAS_H
#define FRECUENCIAS_H

#include "huffman.h"

void contar_frecuencias(const char* texto, int* frecuencias);
struct Nodo* nuevo_nodo(unsigned char caracter, int frecuencia);
int contar_caracteres_con_frecuencia(int* frecuencias);
struct ListaNodos crear_lista_nodos(int* frecuencias);
void liberar_lista_nodos(struct ListaNodos* lista);

#endif