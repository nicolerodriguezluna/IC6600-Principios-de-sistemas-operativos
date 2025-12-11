#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/arbol.h"
#include "../include/frecuencias.h"

/**
 * Encuentra los dos nodos con las frecuencias más bajas en una lista.
 */
void encontrar_dos_minimos(struct ListaNodos lista, int* min1, int* min2) {
    *min1 = 0;
    *min2 = 1;
    if (lista.nodos[0]->frecuencia > lista.nodos[1]->frecuencia) {
        *min1 = 1;
        *min2 = 0;
    }
    for (int i = 2; i < lista.cantidad; i++) {
        if (lista.nodos[i]->frecuencia < lista.nodos[*min1]->frecuencia) {
            *min2 = *min1;
            *min1 = i;
        } else if (lista.nodos[i]->frecuencia < lista.nodos[*min2]->frecuencia) {
            *min2 = i;
        }
    }
}

/**
 * Construye el árbol de Huffman a partir de una lista de nodos.
 */
struct Nodo* construir_arbol_huffman(struct ListaNodos lista_inicial) {
    struct ListaNodos lista_actual = lista_inicial;
    while (lista_actual.cantidad > 1) {
        int min1, min2;
        encontrar_dos_minimos(lista_actual, &min1, &min2);
        
        struct Nodo* nuevo = nuevo_nodo(0, 
            lista_actual.nodos[min1]->frecuencia + lista_actual.nodos[min2]->frecuencia);
        nuevo->izquierda = lista_actual.nodos[min1];
        nuevo->derecha = lista_actual.nodos[min2];
        
        // Crear nueva lista sin los dos mínimos y con el nuevo nodo
        struct ListaNodos nueva_lista;
        nueva_lista.cantidad = lista_actual.cantidad - 1;
        nueva_lista.nodos = (struct Nodo**)malloc(nueva_lista.cantidad * sizeof(struct Nodo*));
        
        int j = 0;
        for (int i = 0; i < lista_actual.cantidad; i++) {
            if (i != min1 && i != min2) {
                nueva_lista.nodos[j] = lista_actual.nodos[i];
                j++;
            }
        }
        nueva_lista.nodos[j] = nuevo;
        
        // Liberar solo el arreglo de la lista actual (los nodos se conservan en el árbol)
        free(lista_actual.nodos);
        lista_actual = nueva_lista;
    }
    
    struct Nodo* raiz = lista_actual.nodos[0];
    free(lista_actual.nodos); // Liberar el arreglo final
    return raiz;
}

/**
 * Imprime el árbol de Huffman con indentación para mostrar la estructura.
 */
void imprimir_arbol(struct Nodo* raiz, int nivel) {
    if (raiz == NULL) return;
    
    for (int i = 0; i < nivel; i++) {
        printf("   ");
    }
    
    if (raiz->caracter == 0) {
        printf("Nodo interno: %d\n", raiz->frecuencia);
    } else {
        // Mostrar caracteres especiales
        unsigned char c = raiz->caracter;
        if (c == ' ') {
            printf("Hoja: 'ESPACIO' (ASCII: %d) - Frecuencia: %d\n", c, raiz->frecuencia);
        } else if (c == '\t') {
            printf("Hoja: 'TAB' (ASCII: %d) - Frecuencia: %d\n", c, raiz->frecuencia);
        } else if (c == '\n') {
            printf("Hoja: 'NUEVA_LINEA' (ASCII: %d) - Frecuencia: %d\n", c, raiz->frecuencia);
        } else if (c == '\r') {
            printf("Hoja: 'RETORNO_CARRO' (ASCII: %d) - Frecuencia: %d\n", c, raiz->frecuencia);
        } else if (c < 32 || c == 127) {
            printf("Hoja: 'CTRL' (ASCII: %d) - Frecuencia: %d\n", c, raiz->frecuencia);
        } else if (c >= 128) {
            printf("Hoja: 'EXT' (ASCII: %d) - Frecuencia: %d\n", c, raiz->frecuencia);
        } else {
            printf("Hoja: '%c' (ASCII: %d) - Frecuencia: %d\n", c, c, raiz->frecuencia);
        }
    }
    
    imprimir_arbol(raiz->izquierda, nivel + 1);
    imprimir_arbol(raiz->derecha, nivel + 1);
}

/**
 * Libera la memoria del árbol de Huffman.
 */
void liberar_arbol(struct Nodo* nodo) {
    if (nodo == NULL) return;
    liberar_arbol(nodo->izquierda);
    liberar_arbol(nodo->derecha);
    free(nodo);
}

/**
 * Genera los códigos de Huffman para cada carácter recursivamente.
 */
void generar_codigos_huffman(struct Nodo* raiz, char* codigo, int profundidad, char** tabla) {
    if (raiz == NULL) return;
    
    // Si es un nodo hoja, guardar el código
    if (raiz->izquierda == NULL && raiz->derecha == NULL) {
        codigo[profundidad] = '\0';
        tabla[raiz->caracter] = strdup(codigo);
        return;
    }
    
    // Recorrer izquierda (agregar '0')
    codigo[profundidad] = '0';
    generar_codigos_huffman(raiz->izquierda, codigo, profundidad + 1, tabla);
    
    // Recorrer derecha (agregar '1')
    codigo[profundidad] = '1';
    generar_codigos_huffman(raiz->derecha, codigo, profundidad + 1, tabla);
}

/**
 * Comprime texto usando la tabla de códigos de Huffman.
 */
char* comprimir_texto(const char* texto, char** tabla) {
    int longitud_original = strlen(texto);
    int longitud_comprimida = 0;
    
    // Calcular longitud del texto comprimido
    for (int i = 0; i < longitud_original; i++) {
        unsigned char c = texto[i];
        if (tabla[c] != NULL) {
            longitud_comprimida += strlen(tabla[c]);
        }
    }
    
    // Asignar memoria para el texto comprimido
    char* texto_comprimido = (char*)malloc(longitud_comprimida + 1);
    texto_comprimido[0] = '\0';
    
    // Construir texto comprimido
    for (int i = 0; i < longitud_original; i++) {
        unsigned char c = texto[i];
        if (tabla[c] != NULL) {
            strcat(texto_comprimido, tabla[c]);
        }
    }
    
    return texto_comprimido;
}

/**
 * Descomprime texto usando el árbol de Huffman.
 */
char* descomprimir_texto(struct Nodo* raiz, const char* texto_comprimido, long tamaño_esperado) {
    if (!raiz || !texto_comprimido) {
        return NULL;
    }

    int longitud_comprimida = strlen(texto_comprimido);
    
    // Asignar memoria para el texto original basado en el tamaño esperado
    char* texto_original = (char*)malloc(tamaño_esperado + 1);
    if (!texto_original) {
        printf("Error de memoria para texto original (tamaño: %ld)\n", tamaño_esperado);
        return NULL;
    }
    
    int indice_original = 0;
    struct Nodo* actual = raiz;
    
    for (int i = 0; i < longitud_comprimida; i++) {
        if (texto_comprimido[i] == '0') {
            if (!actual->izquierda) {
                printf("Error: nodo no tiene hijo izquierdo\n");
                free(texto_original);
                return NULL;
            }
            actual = actual->izquierda;
        } else if (texto_comprimido[i] == '1') {
            if (!actual->derecha) {
                printf("Error: nodo no tiene hijo derecho\n");
                free(texto_original);
                return NULL;
            }
            actual = actual->derecha;
        } else {
            printf("Error: carácter inválido en texto comprimido: %c\n", texto_comprimido[i]);
            free(texto_original);
            return NULL;
        }
        
        // Si es un nodo hoja, agregar carácter al texto original
        if (actual->izquierda == NULL && actual->derecha == NULL) {
            if (indice_original >= tamaño_esperado) {
                printf("Error: texto descomprimido excede el tamaño esperado (%ld)\n", tamaño_esperado);
                free(texto_original);
                return NULL;
            }
            
            texto_original[indice_original++] = actual->caracter;
            actual = raiz; // Volver a la raíz para el próximo carácter
        }
    }
    
    // Verificar que terminamos en la raíz
    if (actual != raiz) {
        printf("Advertencia: texto comprimido no terminó en un nodo hoja\n");
    }
    
    texto_original[indice_original] = '\0';
    
    // Verificar que descomprimimos exactamente el tamaño esperado
    if (indice_original != tamaño_esperado) {
        printf("Advertencia: tamaño descomprimido (%d) no coincide con el esperado (%ld)\n", 
               indice_original, tamaño_esperado);
    }
    
    return texto_original;
}

// Función auxiliar para serializar el árbol recursivamente
void serializar_arbol_recursivo(struct Nodo* raiz, FILE* archivo) {
    if (raiz == NULL) {
        unsigned char marcador = 0;
        fwrite(&marcador, sizeof(unsigned char), 1, archivo);
        return;
    }
    
    if (raiz->izquierda == NULL && raiz->derecha == NULL) {
        // Nodo hoja
        unsigned char marcador = 1;
        fwrite(&marcador, sizeof(unsigned char), 1, archivo);
        fwrite(&raiz->caracter, sizeof(unsigned char), 1, archivo);
    } else {
        // Nodo interno
        unsigned char marcador = 2;
        fwrite(&marcador, sizeof(unsigned char), 1, archivo);
        serializar_arbol_recursivo(raiz->izquierda, archivo);
        serializar_arbol_recursivo(raiz->derecha, archivo);
    }
}

// Función auxiliar para deserializar el árbol recursivamente
struct Nodo* deserializar_arbol_recursivo(FILE* archivo) {
    unsigned char marcador;
    if (fread(&marcador, sizeof(unsigned char), 1, archivo) != 1) {
        printf("Error leyendo marcador del árbol\n");
        return NULL;
    }
    
    if (marcador == 0) {
        return NULL;
    } else if (marcador == 1) {
        // Nodo hoja
        unsigned char caracter;
        if (fread(&caracter, sizeof(unsigned char), 1, archivo) != 1) {
            printf("Error leyendo caracter del árbol\n");
            return NULL;
        }
        return nuevo_nodo(caracter, 0);
    } else if (marcador == 2) {
        // Nodo interno
        struct Nodo* nodo = nuevo_nodo(0, 0);
        nodo->izquierda = deserializar_arbol_recursivo(archivo);
        nodo->derecha = deserializar_arbol_recursivo(archivo);
        
        // Verificar que ambos hijos se crearon correctamente
        if (!nodo->izquierda || !nodo->derecha) {
            printf("Error al crear nodos hijos\n");
            liberar_arbol(nodo);
            return NULL;
        }
        
        return nodo;
    } else {
        printf("Marcador inválido: %d\n", marcador);
        return NULL;
    }
}

// Serializar el árbol de Huffman
int serializar_arbol(struct Nodo* raiz, FILE* archivo) {
    if (!raiz || !archivo) {
        printf("Error: parámetros inválidos para serializar_arbol\n");
        return 0;
    }
    serializar_arbol_recursivo(raiz, archivo);
    
    // Escribir marcador de finalización
    unsigned char marcador_fin = 255;
    if (fwrite(&marcador_fin, sizeof(unsigned char), 1, archivo) != 1) {
        printf("Error al escribir marcador de finalización\n");
        return 0;
    }
    
    return 1;
}

// Deserializar el árbol de Huffman
struct Nodo* deserializar_arbol(FILE* archivo) {
    if (!archivo) {
        printf("Error: archivo inválido para deserializar_arbol\n");
        return NULL;
    }
    
    struct Nodo* raiz = deserializar_arbol_recursivo(archivo);
    
    // Verificar marcador de finalización
    unsigned char marcador_fin;
    if (fread(&marcador_fin, sizeof(unsigned char), 1, archivo) != 1 || marcador_fin != 255) {
        printf("Error: marcador de finalización no encontrado\n");
        liberar_arbol(raiz);
        return NULL;
    }
    
    return raiz;
}