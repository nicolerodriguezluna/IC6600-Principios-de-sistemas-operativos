#define _POSIX_C_SOURCE 200809L
/* ===============================================================================================================
 * decompress_dir_fork.c — Descompresión paralela por procesos desde un .hfa
 * =============================================================================================================== */

#include "../include/common.h"
#include "../include/huffio.h"
#include "../include/io_utils.h"

static void usage(const char *a){
    fprintf(stderr, "Uso: %s <dir> [archivo.hfa] [nprocs]\n", a);
}

/* Trabajo del proceso hijo: extraer una entrada por índice */
static int child_extract_one(const char *archive_path, const char *dir, uint32_t index){
    hfa_meta_t *meta = NULL; uint32_t n = 0;
    if (hfa_index(archive_path, &meta, &n)!=0) return 2;
    if (index >= n){ hfa_free_index(meta,n); return 3; }

    const hfa_meta_t *m = &meta[index];

    FILE *ftree = fmemopen((void*)m->tree_blob, m->tree_len, "rb");
    if (!ftree){ hfa_free_index(meta,n); return 4; }
    struct Nodo *raiz = deserializar_arbol(ftree);
    fclose(ftree);
    if (!raiz){ hfa_free_index(meta,n); return 5; }

    FILE *f = fopen(archive_path, "rb");
    if (!f){ liberar_arbol(raiz); hfa_free_index(meta,n); return 6; }
    if (fseek(f, m->payload_off, SEEK_SET)!=0){ fclose(f); liberar_arbol(raiz); hfa_free_index(meta,n); return 7; }

    uint8_t *payload = NULL;
    if (m->byte_count){
        payload = (uint8_t*)malloc((size_t)m->byte_count);
        if (!payload){ fclose(f); liberar_arbol(raiz); hfa_free_index(meta,n); return 8; }
        if (fread(payload,1,(size_t)m->byte_count,f)!=(size_t)m->byte_count){
            free(payload); fclose(f); liberar_arbol(raiz); hfa_free_index(meta,n); return 9;
        }
    }
    fclose(f);

    char *bitstr = unpack_bits_to_bitstr(payload, (size_t)m->byte_count, m->bit_count);
    char *texto  = descomprimir_texto(raiz, bitstr, (long)m->orig_len);

    if (!texto){
        free(bitstr); free(payload); liberar_arbol(raiz); hfa_free_index(meta,n); return 10;
    }

    char out_path[PATH_MAX];
    join_path(dir, m->name, out_path);
    int wrc = write_file_text(out_path, texto, (size_t)m->orig_len);

    free(texto);
    free(bitstr);
    free(payload);
    liberar_arbol(raiz);
    hfa_free_index(meta, n);

    return (wrc==0)? 0 : 11;
}

int main(int argc, char **argv){
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    if (argc < 2){ usage(argv[0]); return 1; }

    const char *dir = argv[1];

    char archive_path[PATH_MAX];
    if (argc >= 3) strncpy(archive_path, argv[2], PATH_MAX-1);
    else           join_path(dir, "archive.hfa", archive_path);

    int maxproc = (argc >= 4)? atoi(argv[3]) : num_cpus();
    if (maxproc <= 0) maxproc = 2;

    hfa_meta_t *meta = NULL; uint32_t n = 0;
    if (hfa_index(archive_path, &meta, &n)!=0){ WARN("No se pudo indexar %s", archive_path); return 1; }
    hfa_free_index(meta, n);
    if (n == 0){ WARN("Archivo vacío: %s", archive_path); return 0; }

    int running = 0, any_fail = 0;
    for (uint32_t i=0;i<n;i++){
        while (running >= maxproc){
            int status;
            if (waitpid(-1, &status, 0) > 0){
                running--;
                if (!WIFEXITED(status) || WEXITSTATUS(status)!=0) any_fail = 1;
            }
        }
        pid_t pid = fork();
        if (pid < 0){ any_fail=1; break; }
        if (pid == 0){
            int rc = child_extract_one(archive_path, dir, i);
            _exit(rc);
        } else {
            running++;
        }
    }

    while (running > 0){
        int status;
        if (waitpid(-1, &status, 0) > 0){
            running--;
            if (!WIFEXITED(status) || WEXITSTATUS(status)!=0) any_fail = 1;
        }
    }

    /* if (!any_fail) remove(archive_path);*/

    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("[OK] Restauración %s (archivos: %u)%s\n", dir, n, any_fail? " con advertencias": "");
    printf("Tiempo de descompresión: %ld ms\n", elapsed_ms(t0,t1));
    return any_fail ? 1 : 0;
}
