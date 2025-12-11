#define _POSIX_C_SOURCE 200809L
/* ===============================================================================================================
 * compress_dir_fork.c — Compresión paralela por procesos de todos los .txt en un directorio a un único .hfa
 * =============================================================================================================== */

#include "../include/common.h"
#include "../include/huffio.h"
#include "../include/io_utils.h"

/* Límite de procesos concurrentes */
static void wait_until_slots(int *running, int max_procs){
    while (*running >= max_procs){
        int status;
        if (waitpid(-1, &status, 0) > 0) (*running)--;
    }
}

/* Empaqueta y escribe una entrada .hfa (sin header global) en un FILE* */
static int write_hfa_entry(FILE *out,
                           const char *name,
                           const char *texto, size_t texto_len,
                           struct Nodo *raiz,
                           const char *bitstr)
{
    uint8_t *packed = NULL;
    size_t   packed_len = 0;
    uint64_t bit_count  = 0;
    pack_bits_from_bitstr(bitstr, &packed, &packed_len, &bit_count);
    (void)texto;
    size_t name_len = strlen(name);
    if (name_len > UINT16_MAX){ free(packed); return -1; }
    uint16_t n16 = (uint16_t)name_len;
    if (fwrite(&n16, sizeof(n16), 1, out) != 1){ free(packed); return -1; }
    if (fwrite(name, 1, name_len, out) != name_len){ free(packed); return -1; }

    uint64_t orig = (uint64_t)texto_len;
    if (fwrite(&orig, sizeof(orig), 1, out) != 1){ free(packed); return -1; }

    if (!serializar_arbol(raiz, out)){ free(packed); return -1; }

    if (fwrite(&bit_count, sizeof(bit_count), 1, out) != 1){ free(packed); return -1; }
    uint64_t bcount = (uint64_t)packed_len;
    if (fwrite(&bcount, sizeof(bcount), 1, out) != 1){ free(packed); return -1; }
    if (packed_len){
        if (fwrite(packed, 1, packed_len, out) != packed_len){ free(packed); return -1; }
    }

    free(packed);
    return 0;
}

/* Trabajo del proceso hijo: comprimir un archivo y dejar la entrada en <dir>/.hfp.<pid>.part */
static int child_compress_to_part(const char *dir, const char *fullpath)
{
    char *texto = NULL;
    size_t tlen = 0;
    if (read_file_text(fullpath, &texto, &tlen) != 0) return 2;

    int freq[TAM_MAX] = {0};
    contar_frecuencias(texto, freq);
    struct ListaNodos L = crear_lista_nodos(freq);
    struct Nodo *raiz = construir_arbol_huffman(L);

    char *tabla[TAM_MAX] = {0};
    char  cod[TAM_MAX];
    generar_codigos_huffman(raiz, cod, 0, tabla);

    char *bitstr = comprimir_texto(texto, tabla);

    char part_path[PATH_MAX];
    snprintf(part_path, sizeof(part_path), "%s/.hfp.%d.part", dir, (int)getpid());
    FILE *pf = fopen(part_path, "wb");
    if (!pf){
        free(texto);
        for (int i=0;i<TAM_MAX;i++) free(tabla[i]);
        liberar_arbol(raiz);
        free(bitstr);
        return 3;
    }

    int rc = write_hfa_entry(pf, base_name(fullpath), texto, tlen, raiz, bitstr);
    fclose(pf);

    free(texto);
    for (int i=0;i<TAM_MAX;i++) free(tabla[i]);
    liberar_arbol(raiz);
    free(bitstr);

    return (rc==0)? 0 : 4;
}

/* Copia el contenido de un archivo en un FILE* destino */
static int copy_file_into(FILE *dst, const char *path){
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char buf[1<<15];
    size_t n;
    while ((n = fread(buf,1,sizeof buf,f)) > 0){
        if (fwrite(buf,1,n,dst) != n){ fclose(f); return -1; }
    }
    int rc = ferror(f) ? -1 : 0;
    fclose(f);
    return rc;
}

/* Orden determinista por nombre base */
static int cmp_strptr(const void *a, const void *b){
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcmp(base_name(sa), base_name(sb));
}

static void usage(const char *a){
    fprintf(stderr, "Uso: %s <dir> [nprocs] [archivo_salida.hfa]\n", a);
}

int main(int argc, char **argv){
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    if (argc < 2){ usage(argv[0]); return 1; }
    const char *dir      = argv[1];
    int         maxproc  = (argc >= 3)? atoi(argv[2]) : num_cpus();
    if (maxproc <= 0) maxproc = 2;
    const char *outname  = (argc >= 4)? argv[3] : "archive.hfa";

    strvec_t files; sv_init(&files);
    if (list_files_with_suffix(dir, ".txt", &files)!=0){ DIE("No se pudo abrir %s", dir); }
    if (!files.len){ WARN("No hay .txt en %s", dir); sv_free(&files); return 0; }

    qsort(files.paths, files.len, sizeof(char*), cmp_strptr);

    pid_t *pids = calloc(files.len, sizeof(pid_t));
    int running = 0, any_fail = 0;

    for (size_t i=0;i<files.len;i++){
        wait_until_slots(&running, maxproc);

        pid_t pid = fork();
        if (pid < 0){ any_fail = 1; break; }

        if (pid == 0){
            int rc = child_compress_to_part(dir, files.paths[i]);
            _exit(rc);
        } else {
            pids[i] = pid;
            running++;
        }
    }

    while (running > 0){
        int status;
        if (waitpid(-1, &status, 0) > 0){
            running--;
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) any_fail = 1;
        }
    }
    if (any_fail){ sv_free(&files); free(pids); return 1; }

    char out_path[PATH_MAX]; join_path(dir, outname, out_path);
    FILE *out = fopen(out_path, "wb");
    if (!out){ sv_free(&files); free(pids); DIE("No se pudo crear %s", out_path); }

    hfa_header_t hdr; memcpy(hdr.magic,"HFA1",4); hdr.nfiles = (uint32_t)files.len;
    if (fwrite(&hdr, sizeof(hdr), 1, out) != 1){ fclose(out); sv_free(&files); free(pids); DIE("No se pudo escribir header"); }

    for (size_t i=0;i<files.len;i++){
        char part[PATH_MAX];
        snprintf(part, sizeof(part), "%s/.hfp.%d.part", dir, (int)pids[i]);
        if (copy_file_into(out, part)!=0){
            fclose(out); sv_free(&files); free(pids); DIE("No se pudo copiar %s", part);
        }
        /*remove(part);*/ 
    }
    fclose(out);

    /*for (size_t i=0;i<files.len;i++) remove(files.paths[i]);*/

    sv_free(&files);
    free(pids);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("[OK] Escribí %s con %u archivos\n", out_path, hdr.nfiles);
    printf("Tiempo de compresión: %ld ms\n", elapsed_ms(t0,t1));
    return 0;
}
