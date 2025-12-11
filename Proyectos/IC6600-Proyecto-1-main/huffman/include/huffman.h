#ifndef HUFFMAN_H
#define HUFFMAN_H

#define TAM_MAX 256

// Nodo del árbol de Huffman
struct Nodo {
    unsigned char caracter;
    int frecuencia;
    struct Nodo *izquierda, *derecha;
};

// Estructura para retornar múltiples valores de forma limpia
struct ListaNodos {
    struct Nodo** nodos;
    int cantidad;
};

#endif