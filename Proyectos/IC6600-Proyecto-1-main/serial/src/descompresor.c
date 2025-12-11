#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/io_handler.h"
#include "../../huffman/include/huffman.h"
#include "../../huffman/include/frecuencias.h"
#include "../../huffman/include/arbol.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s <archivo_comprimido> <directorio_salida>\n", argv[0]);
        return 1;
    }

    printf("Descomprimiendo archivo '%s' en directorio '%s'...\n", argv[1], argv[2]);

    // Iniciar medición de tiempo
    clock_t start = clock();
    
    if (descomprimir_archivo_completo(argv[1], argv[2])) {
        // Calcular tiempo transcurrido
        clock_t end = clock();
        double tiempo_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;
        
        printf("Descompresión completada exitosamente.\n");
        printf("Tiempo de descompresión: %.2f ms\n", tiempo_ms);
        return 0;
    } else {
        printf("Error en la descompresión.\n");
        return 1;
    }
}