#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "../include/huffman.h"
#include "../include/frecuencias.h"
#include "../include/arbol.h"

int main() {
    setlocale(LC_ALL, "");

    // Leer texto de forma dinámica
    printf("Por favor, introduce el texto que deseas analizar:\n");
    
    size_t buffer_size = 1024;
    char* texto = malloc(buffer_size);
    size_t texto_length = 0;
    int c;
    
    while ((c = getchar()) != EOF && c != '\n') {
        texto[texto_length++] = c;
        if (texto_length >= buffer_size) {
            buffer_size *= 2;
            texto = realloc(texto, buffer_size);
        }
    }
    texto[texto_length] = '\0';

    int frecuencias[TAM_MAX];
    texto[strcspn(texto, "\n")] = 0;

    // 1. Contar frecuencias
    contar_frecuencias(texto, frecuencias);

    // 2. Crear lista de nodos
    struct ListaNodos lista_nodos = crear_lista_nodos(frecuencias);

    // 3. Construir árbol de Huffman
    struct Nodo* raiz = construir_arbol_huffman(lista_nodos);
    
    // Imprimir el árbol
    printf("\nÁrbol de Huffman (estructura):\n");
    imprimir_arbol(raiz, 0);

    // 4. Generar códigos de Huffman
    char* tabla_codigos[TAM_MAX] = {NULL};
    char codigo_actual[TAM_MAX];
    generar_codigos_huffman(raiz, codigo_actual, 0, tabla_codigos);
    
    // Mostrar tabla de códigos
    printf("\nTabla de códigos de Huffman:\n");
    for (int i = 0; i < TAM_MAX; i++) {
        if (tabla_codigos[i] != NULL) {
            unsigned char c = i;
            if (c == ' ') {
                printf("'ESPACIO': %s\n", tabla_codigos[i]);
            } else if (c == '\t') {
                printf("'TAB': %s\n", tabla_codigos[i]);
            } else if (c == '\n') {
                printf("'NUEVA_LINEA': %s\n", tabla_codigos[i]);
            } else if (c < 32 || c == 127) {
                printf("'CTRL' (ASCII %d): %s\n", c, tabla_codigos[i]);
            } else if (c >= 128) {
                printf("'EXT' (ASCII %d): %s\n", c, tabla_codigos[i]);
            } else {
                printf("'%c': %s\n", c, tabla_codigos[i]);
            }
        }
    }

    // 5. Comprimir y mostrar el resultado
    char* texto_comprimido = comprimir_texto(texto, tabla_codigos);
    printf("\nTexto comprimido:\n%s\n", texto_comprimido);
    
    // Calcular y mostrar tasa de compresión
    int longitud_original = strlen(texto);
    int longitud_comprimida = strlen(texto_comprimido);
    float tasa_compresion = (1 - (float)longitud_comprimida / (longitud_original * 8)) * 100;
    printf("\nTasa de compresión: %.2f%%\n", tasa_compresion);
    printf("Longitud original: %d bits\n", longitud_original * 8);
    printf("Longitud comprimida: %d bits\n", longitud_comprimida);

    // 6. Descomprimir y mostrar el resultado
    char* texto_descomprimido = descomprimir_texto(raiz, texto_comprimido, texto_length);
    printf("\nTexto descomprimido:\n%s\n", texto_descomprimido);
    
    // Verificar integridad
    if (strcmp(texto, texto_descomprimido) == 0) {
        printf("\n✓ La descompresión fue exitosa (texto idéntico al original).\n");
    } else {
        printf("\n✗ Error en la descompresión (texto diferente al original).\n");
    }

    // 7. Liberar memoria
    for (int i = 0; i < TAM_MAX; i++) {
        free(tabla_codigos[i]);
    }
    free(texto_comprimido);
    free(texto_descomprimido);
    liberar_arbol(raiz);

    return 0;
}