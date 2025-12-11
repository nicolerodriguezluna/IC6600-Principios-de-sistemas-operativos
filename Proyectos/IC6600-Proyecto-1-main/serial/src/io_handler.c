#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/io_handler.h"
#include "../../huffman/include/arbol.h"
#include "../../huffman/include/frecuencias.h"

// Función para comprimir un solo archivo y agregarlo al archivo de salida
int comprimir_archivo(const char* nombre_archivo, struct Nodo* raiz, char** tabla_codigos, FILE* archivo_salida) {
    FILE* archivo_entrada = fopen(nombre_archivo, "r");
    if (!archivo_entrada) {
        printf("Error al abrir archivo de entrada: %s\n", nombre_archivo);
        return 0;
    }

    // Obtener el tamaño del archivo
    fseek(archivo_entrada, 0, SEEK_END);
    long tamaño_archivo = ftell(archivo_entrada);
    fseek(archivo_entrada, 0, SEEK_SET);

    // Leer el contenido del archivo
    char* contenido = (char*)malloc(tamaño_archivo + 1);
    if (!contenido) {
        fclose(archivo_entrada);
        printf("Error de memoria al leer: %s\n", nombre_archivo);
        return 0;
    }

    size_t leidos = fread(contenido, 1, tamaño_archivo, archivo_entrada);
    if (leidos != tamaño_archivo) {
        printf("Error al leer archivo: %s\n", nombre_archivo);
        free(contenido);
        fclose(archivo_entrada);
        return 0;
    }
    
    contenido[tamaño_archivo] = '\0';
    fclose(archivo_entrada);

    // Comprimir el contenido
    char* texto_comprimido = comprimir_texto(contenido, tabla_codigos);
    free(contenido);

    if (!texto_comprimido) {
        printf("Error al comprimir: %s\n", nombre_archivo);
        return 0;
    }

    // Escribir metadatos del archivo
    int longitud_nombre = strlen(nombre_archivo);
    fwrite(&longitud_nombre, sizeof(int), 1, archivo_salida);
    fwrite(nombre_archivo, 1, longitud_nombre, archivo_salida);
    fwrite(&tamaño_archivo, sizeof(long), 1, archivo_salida);

    // Escribir contenido comprimido
    int longitud_comprimida = strlen(texto_comprimido);
    fwrite(&longitud_comprimida, sizeof(int), 1, archivo_salida);
    fwrite(texto_comprimido, 1, longitud_comprimida, archivo_salida);

    free(texto_comprimido);
    return 1;
}

// Función para comprimir todos los archivos de texto en un directorio
int comprimir_directorio(const char* directorio_entrada, const char* archivo_salida) {
    DIR* dir = opendir(directorio_entrada);
    if (!dir) {
        printf("Error al abrir directorio: %s\n", directorio_entrada);
        return 0;
    }

    FILE* archivo_comprimido = fopen(archivo_salida, "wb");
    if (!archivo_comprimido) {
        printf("Error al crear archivo de salida: %s\n", archivo_salida);
        closedir(dir);
        return 0;
    }

    // Primero, leer todos los archivos y calcular frecuencias globales
    int frecuencias[TAM_MAX] = {0};
    struct dirent* entrada;
    char ruta_completa[1024];

    printf("Calculando frecuencias...\n");
    while ((entrada = readdir(dir)) != NULL) {
        if (entrada->d_type == DT_REG) {
            snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", directorio_entrada, entrada->d_name);
            
            FILE* archivo = fopen(ruta_completa, "r");
            if (archivo) {
                printf("Procesando: %s\n", entrada->d_name);
                char buffer[1024];
                while (fgets(buffer, sizeof(buffer), archivo)) {
                    for (int i = 0; buffer[i] != '\0'; i++) {
                        frecuencias[(unsigned char)buffer[i]]++;
                    }
                }
                fclose(archivo);
            }
        }
    }
    closedir(dir);

    // Construir árbol de Huffman global
    printf("Construyendo árbol de Huffman...\n");
    struct ListaNodos lista_nodos = crear_lista_nodos(frecuencias);
    struct Nodo* raiz = construir_arbol_huffman(lista_nodos);
    
    // Generar tabla de códigos
    printf("Generando códigos...\n");
    char* tabla_codigos[TAM_MAX] = {NULL};
    char codigo_actual[TAM_MAX];
    generar_codigos_huffman(raiz, codigo_actual, 0, tabla_codigos);

    // Serializar y guardar el árbol en el archivo comprimido
    printf("Guardando árbol...\n");
    if (!serializar_arbol(raiz, archivo_comprimido)) {
        printf("Error al serializar el árbol\n");
        fclose(archivo_comprimido);
        liberar_arbol(raiz);
        for (int i = 0; i < TAM_MAX; i++) free(tabla_codigos[i]);
        return 0;
    }

    // Volver a abrir el directorio para procesar cada archivo
    dir = opendir(directorio_entrada);
    if (!dir) {
        printf("Error al reabrir directorio: %s\n", directorio_entrada);
        fclose(archivo_comprimido);
        liberar_arbol(raiz);
        for (int i = 0; i < TAM_MAX; i++) free(tabla_codigos[i]);
        return 0;
    }

    // Contar archivos
    int contador_archivos = 0;
    while ((entrada = readdir(dir)) != NULL) {
        if (entrada->d_type == DT_REG) contador_archivos++;
    }
    rewinddir(dir);

    printf("Comprimiendo %d archivos...\n", contador_archivos);
    if (fwrite(&contador_archivos, sizeof(int), 1, archivo_comprimido) != 1) {
        printf("Error al escribir número de archivos\n");
        closedir(dir);
        fclose(archivo_comprimido);
        liberar_arbol(raiz);
        for (int i = 0; i < TAM_MAX; i++) free(tabla_codigos[i]);
        return 0;
    }

    // Comprimir cada archivo
    int archivos_comprimidos = 0;
    while ((entrada = readdir(dir)) != NULL) {
        if (entrada->d_type == DT_REG) {
            snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", directorio_entrada, entrada->d_name);
            printf("Comprimiendo: %s\n", entrada->d_name);
            
            if (comprimir_archivo(ruta_completa, raiz, tabla_codigos, archivo_comprimido)) {
                archivos_comprimidos++;
            } else {
                printf("Error al comprimir archivo: %s\n", entrada->d_name);
            }
        }
    }

    printf("Archivos comprimidos exitosamente: %d/%d\n", archivos_comprimidos, contador_archivos);

    // Liberar recursos
    closedir(dir);
    fclose(archivo_comprimido);

    // Primero liberar la tabla de códigos
    for (int i = 0; i < TAM_MAX; i++) {
        if (tabla_codigos[i]) free(tabla_codigos[i]);
    }

    // Luego liberar el árbol (que incluye todos los nodos)
    liberar_arbol(raiz);

    return 1;
}

// Función para crear directorios recursivamente
int crear_directorios_recursivamente(const char* path) {
    char tmp[1024];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

// Función para descomprimir un archivo comprimido
int descomprimir_archivo_completo(const char* archivo_entrada, const char* directorio_salida) {
    FILE* archivo_comprimido = fopen(archivo_entrada, "rb");
    if (!archivo_comprimido) {
        printf("Error al abrir archivo comprimido: %s\n", archivo_entrada);
        return 0;
    }

    // Crear directorio de salida si no existe
    struct stat st = {0};
    if (stat(directorio_salida, &st) == -1) {
        if (mkdir(directorio_salida, 0700) == -1) {
            printf("Error al crear directorio de salida: %s\n", directorio_salida);
            fclose(archivo_comprimido);
            return 0;
        }
    }

    // Deserializar el árbol de Huffman
    printf("Cargando árbol de Huffman...\n");
    struct Nodo* raiz = deserializar_arbol(archivo_comprimido);
    if (!raiz) {
        printf("Error al deserializar el árbol\n");
        fclose(archivo_comprimido);
        return 0;
    }

    // Leer número de archivos
    int numero_archivos;
    if (fread(&numero_archivos, sizeof(int), 1, archivo_comprimido) != 1) {
        printf("Error al leer número de archivos\n");
        fclose(archivo_comprimido);
        liberar_arbol(raiz);
        return 0;
    }

    printf("Descomprimiendo %d archivos...\n", numero_archivos);

    // Procesar cada archivo
    for (int i = 0; i < numero_archivos; i++) {
        // Leer metadatos del archivo
        int longitud_nombre;
        if (fread(&longitud_nombre, sizeof(int), 1, archivo_comprimido) != 1) {
            printf("Error al leer longitud del nombre del archivo %d\n", i);
            break;
        }
        
        // Verificar que la longitud del nombre sea válida
        if (longitud_nombre <= 0 || longitud_nombre > 255) {
            printf("Longitud de nombre inválida: %d\n", longitud_nombre);
            break;
        }
        
        char nombre_archivo[256];
        if (fread(nombre_archivo, 1, longitud_nombre, archivo_comprimido) != longitud_nombre) {
            printf("Error al leer nombre del archivo %d\n", i);
            break;
        }
        nombre_archivo[longitud_nombre] = '\0';
        
        long tamaño_original;
        if (fread(&tamaño_original, sizeof(long), 1, archivo_comprimido) != 1) {
            printf("Error al leer tamaño original del archivo %d\n", i);
            break;
        }
        
        int longitud_comprimida;
        if (fread(&longitud_comprimida, sizeof(int), 1, archivo_comprimido) != 1) {
            printf("Error al leer longitud comprimida del archivo %d\n", i);
            break;
        }
        
        // Leer contenido comprimido
        char* texto_comprimido = (char*)malloc(longitud_comprimida + 1);
        if (!texto_comprimido) {
            printf("Error de memoria para archivo %d\n", i);
            break;
        }
        
        if (fread(texto_comprimido, 1, longitud_comprimida, archivo_comprimido) != longitud_comprimida) {
            printf("Error al leer contenido comprimido del archivo %d\n", i);
            free(texto_comprimido);
            break;
        }
        texto_comprimido[longitud_comprimida] = '\0';
        
        // Descomprimir contenido
        printf("Descomprimiendo: %s (tamaño: %ld bytes)\n", nombre_archivo, tamaño_original);
        char* texto_original = descomprimir_texto(raiz, texto_comprimido, tamaño_original);
        free(texto_comprimido);
        
        if (!texto_original) {
            printf("Error al descomprimir: %s\n", nombre_archivo);
            continue;
        }
        
        // Crear ruta completa de salida
        char ruta_salida[1024];
        snprintf(ruta_salida, sizeof(ruta_salida), "%s/%s", directorio_salida, nombre_archivo);
        
        // Crear directorios necesarios si la ruta contiene subdirectorios
        char* ultimo_slash = strrchr(ruta_salida, '/');
        if (ultimo_slash) {
            // Temporalmente cortamos la cadena en la última barra
            *ultimo_slash = '\0';
            
            // Creamos los directorios necesarios
            if (crear_directorios_recursivamente(ruta_salida) != 0) {
                printf("Error al crear directorios para: %s\n", ruta_salida);
                *ultimo_slash = '/';
                free(texto_original);
                continue;
            }
            
            // Restauramos la barra
            *ultimo_slash = '/';
        }
        
        // Escribir archivo descomprimido
        FILE* archivo_salida = fopen(ruta_salida, "w");
        if (archivo_salida) {
            fwrite(texto_original, 1, tamaño_original, archivo_salida);
            fclose(archivo_salida);
            printf("✓ %s descomprimido exitosamente (%ld bytes)\n", nombre_archivo, tamaño_original);
        } else {
            printf("Error al crear archivo: %s\n", ruta_salida);
        }
        
        free(texto_original);
    }

    liberar_arbol(raiz);
    fclose(archivo_comprimido);
    return 1;
}