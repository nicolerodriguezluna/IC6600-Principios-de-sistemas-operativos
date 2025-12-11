/* ========================================================================================================================
 * compress_dir.c — Comprime en paralelo todos los .txt de un directorio en un único archivo .hfa y luego elimina los .txt.
 * ======================================================================================================================== */

#include "../include/common.h"
#include "../include/thread_pool.h"
#include "../include/io_utils.h"
#include "../include/huffio.h"
#include <time.h>

/* task_arg_t: Argumentos por tarea de compresión. */
typedef struct
{
    char path[PATH_MAX];
    char name[PATH_MAX];
} task_arg_t;

/* shared_t: Estado compartido entre hilos para acumular los resultados
   con protección por mutex. */
typedef struct
{
    pthread_mutex_t mtx;
    hfa_entry_t *vec; /* vector de resultados uno por archivo */
    size_t idx;       /* índice de escritura actual */
} shared_t;

/* función worker que comprime un archivo:
 * - Lee el texto
 * - Calcula frecuencias y construye el árbol de Huffman
 * - Genera la tabla y el bitstring y lo empaqueta en bytes
 * - Inserta el resultado en el vector compartido */
static void do_compress(void *arg)
{
    task_arg_t *t = (task_arg_t *)arg;
    shared_t *S = (shared_t *)t->name;
    S = *(shared_t **)t->name;

    /* 1) Leer texto completo a memoria */
    char *texto = NULL;
    size_t tlen = 0;
    if (read_file_text(t->path, &texto, &tlen) != 0)
    {
        free(t);
        return;
    }

    /* 2) Construir Huffman */
    int freq[TAM_MAX];
    contar_frecuencias(texto, freq);
    struct ListaNodos L = crear_lista_nodos(freq);
    struct Nodo *raiz = construir_arbol_huffman(L);
    char *tabla[TAM_MAX] = {0};
    char cod[TAM_MAX];
    generar_codigos_huffman(raiz, cod, 0, tabla);
    char *bitstr = comprimir_texto(texto, tabla);

    /* 3) Empaquetar bits en bytes */
    uint8_t *packed = NULL;
    size_t packed_len = 0;
    uint64_t bit_count = 0;
    pack_bits_from_bitstr(bitstr, &packed, &packed_len, &bit_count);

    /* 4) Guardar en vector compartido */
    pthread_mutex_lock(&S->mtx);
    size_t i = S->idx++;
    S->vec[i].name = strdup(strrchr(t->path, '/') ? strrchr(t->path, '/') + 1 : t->path);
    S->vec[i].txt = texto;
    S->vec[i].txt_len = tlen;
    S->vec[i].raiz = raiz;
    S->vec[i].tabla = calloc(TAM_MAX, sizeof(char *));
    for (int k = 0; k < TAM_MAX; k++)
        S->vec[i].tabla[k] = tabla[k];
    S->vec[i].bitstr = bitstr;
    S->vec[i].bit_count = bit_count;
    S->vec[i].packed = packed;
    S->vec[i].packed_len = packed_len;
    pthread_mutex_unlock(&S->mtx);

    /* 5) Liberar arg de tarea */
    free(t);
}

/* imprime sintaxis del binario */
static void usage(const char *a) { fprintf(stderr, "Uso: %s <dir> [hilos] [nombre_salida.hfa]\n", a); }

/* coordina la compresión paralela:
 * - Enumera .txt, lanza tareas, espera el fin
 * - Escribe un único .hfa
 * - Si todo OK, elimina los .txt
 * - Mide y reporta tiempo total en ms */
int main(int argc, char **argv)
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }
    const char *dir = argv[1];
    int threads = (argc >= 3) ? atoi(argv[2]) : num_cpus();
    const char *outname = (argc >= 4) ? argv[3] : "archive.hfa";

    strvec_t files;
    sv_init(&files);
    if (list_files_with_suffix(dir, ".txt", &files) != 0)
    {
        DIE("No se pudo abrir %s", dir);
    }
    if (!files.len)
    {
        WARN("No hay .txt en %s", dir);
        sv_free(&files);
        return 0;
    }

    thread_pool_t tp;
    if (tp_init(&tp, threads) != 0)
        DIE("pool");
    shared_t S = {.vec = calloc(files.len, sizeof(hfa_entry_t)), .idx = 0};
    pthread_mutex_init(&S.mtx, NULL);

    /* Encolar una tarea por archivo */
    for (size_t i = 0; i < files.len; i++)
    {
        task_arg_t *t = calloc(1, sizeof(*t));
        strncpy(t->path, files.paths[i], PATH_MAX - 1);
        /* Pasar puntero compartido mediante un pequeño truco */
        shared_t **pp = (shared_t **)&t->name[0];
        *pp = &S;
        tp_submit(&tp, do_compress, t);
    }
    tp_wait(&tp);
    tp_destroy(&tp);

    /* Escribir un único .hfa y, si salió bien, borrar .txt */
    char arch_path[PATH_MAX];
    join_path(dir, outname, arch_path);
    if (hfa_write(arch_path, S.vec, (uint32_t)files.len) == 0)
    {
        printf("[OK] Se escribio %s con %zu archivos\n", arch_path, files.len);
        for (size_t i = 0; i < files.len; i++)
            remove(files.paths[i]);
    }
    else
    {
        WARN("No se pudo escribir %s", arch_path);
    }

    /* Limpieza de memoria/estructuras */
    for (size_t i = 0; i < files.len; i++)
    {
        free(S.vec[i].name);
        free(S.vec[i].txt);
        for (int k = 0; k < TAM_MAX; k++)
            free(S.vec[i].tabla[k]);
        free(S.vec[i].tabla);
        free(S.vec[i].bitstr);
        free(S.vec[i].packed);
        liberar_arbol(S.vec[i].raiz);
    }
    free(S.vec);
    pthread_mutex_destroy(&S.mtx);
    sv_free(&files);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Tiempo de compresión: %ld ms\n", elapsed_ms(t0, t1));
    return 0;
}
