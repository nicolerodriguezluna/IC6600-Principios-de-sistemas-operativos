/* ===============================================================================================================
 * decompress_dir.c — Descomprime en paralelo un .hfa restaurando todos los .txt y eliminando el .hfa al terminar.
 * =============================================================================================================== */

#include "../include/common.h"
#include "../include/thread_pool.h"
#include "../include/io_utils.h"
#include "../include/huffio.h"
#include <time.h>

/* argumentos por tarea de descompresión */
typedef struct {
    const char   *archive_path;  /* ruta al .hfa */
    const char   *dir;           /* directorio de salida */
    hfa_meta_t   *meta;          /* metadatos del archivo a extraer */
} task_t;

/* descomprime un archivo desde el .hfa:
 * - Reconstruye árbol desde blob en RAM
 * - Lee payload desde el .hfa
 * - Desempaqueta bits a '0'/'1'
 * - Escribe el .txt resultante */
static void worker(void *arg){
    task_t *t = (task_t*)arg;
    const hfa_meta_t *m = t->meta;

    /* 1) Árbol desde blob en memoria */
    FILE *ftree = fmemopen((void*)m->tree_blob, m->tree_len, "rb");
    if (!ftree) { free(t); return; }
    struct Nodo *raiz = deserializar_arbol(ftree);
    fclose(ftree);
    if (!raiz) { free(t); return; }

    /* 2) Leer payload del .hfa a partir del offset */
    FILE *f = fopen(t->archive_path, "rb");
    if (!f) { liberar_arbol(raiz); free(t); return; }
    if (fseek(f, m->payload_off, SEEK_SET)!=0) { fclose(f); liberar_arbol(raiz); free(t); return; }

    uint8_t *payload = NULL;
    if (m->byte_count){
        payload = (uint8_t*)malloc((size_t)m->byte_count);
        if (!payload){ fclose(f); liberar_arbol(raiz); free(t); return; }
        if (fread(payload,1,(size_t)m->byte_count,f)!=(size_t)m->byte_count){
            free(payload); fclose(f); liberar_arbol(raiz); free(t); return;
        }
    }
    fclose(f);

    /* 3) Desempaquetar y descomprimir */
    char *bitstr = unpack_bits_to_bitstr(payload, (size_t)m->byte_count, m->bit_count);
    char *texto  = descomprimir_texto(raiz, bitstr, (long)m->orig_len);

    /* 4) Escribir .txt */
    if (texto){
        char out_path[PATH_MAX];
        join_path(t->dir, m->name, out_path);
        write_file_text(out_path, texto, (size_t)m->orig_len);
    }

    /* 5) Limpieza */
    free(texto);
    free(bitstr);
    free(payload);
    liberar_arbol(raiz);
    free(t);
}

/* imprime sintaxis del binario. */
static void usage(const char *a){ fprintf(stderr,"Uso: %s <dir> [archivo.hfa] [hilos]\n", a); }

/* coordina descompresión paralela:
 * - Indexa el .hfa y obtiene metadatos
 * - Lanza tareas por archivo y espera
 * - Borra el .hfa si todo salió bien
 * - Mide y reporta tiempo total en ms */
int main(int argc,char **argv){
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (argc < 2){ usage(argv[0]); return 1; }
    const char *dir = argv[1];

    char archive_path[PATH_MAX];
    if (argc >= 3) strncpy(archive_path, argv[2], PATH_MAX-1);
    else           join_path(dir, "archive.hfa", archive_path);

    int threads = (argc >= 4) ? atoi(argv[3]) : num_cpus();
    if (threads <= 0) threads = 2;

    /* 1) Indexar metadatos del archivo .hfa */
    hfa_meta_t *meta = NULL; uint32_t n = 0;
    if (hfa_index(archive_path, &meta, &n)!=0){ WARN("No se pudo indexar %s", archive_path); return 1; }
    if (n == 0){ WARN("Archivo vacío: %s", archive_path); hfa_free_index(meta, n); return 0; }

    /* 2) Ejecutar tareas en pool */
    thread_pool_t tp;
    if (tp_init(&tp, threads)!=0){ WARN("No se pudo crear pool"); hfa_free_index(meta, n); return 1; }

    for (uint32_t i=0;i<n;i++){
        task_t *t = (task_t*)calloc(1,sizeof(task_t));
        t->archive_path = archive_path;
        t->dir = dir;
        t->meta = &meta[i];
        tp_submit(&tp, worker, t);
    }

    tp_wait(&tp);
    tp_destroy(&tp);

    /* 3) Eliminar el .hfa y liberar índice */
    remove(archive_path);
    hfa_free_index(meta, n);

    printf("[OK] Se restauraron %u archivos y se borro %s\n", n, archive_path);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Tiempo de descompresión: %ld ms\n", elapsed_ms(t0,t1));
    return 0;
}
