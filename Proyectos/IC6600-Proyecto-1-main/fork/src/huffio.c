#define _POSIX_C_SOURCE 200809L
/* ===============================================================================================================
 * huffio.c — Implementa el formato .hfa (archivo único) y utilidades de empaquetado/desempaquetado de bits.
 * =============================================================================================================== */

#include "../include/huffio.h"
#include "../include/io_utils.h"
#include <errno.h>

/* Escritura/lectura de enteros */
static void put_u16(FILE *f, uint16_t v){ fwrite(&v, sizeof(v), 1, f); }
static void put_u64(FILE *f, uint64_t v){ fwrite(&v, sizeof(v), 1, f); }
static uint16_t get_u16(FILE *f){ uint16_t v; fread(&v,sizeof(v),1,f); return v; }
static uint64_t get_u64(FILE *f){ uint64_t v; fread(&v,sizeof(v),1,f); return v; }

/* Recorre el árbol serializado sin construir nodos para ubicar el final de la estructura. */
static int skip_tree(FILE *f) {
    unsigned char m;
    if (fread(&m,1,1,f)!=1) return -1;

    if (m == 0) {
        return 0; /* nodo nulo */
    } else if (m == 1) {
        unsigned char c;
        if (fread(&c,1,1,f)!=1) return -1; /* hoja + byte de carácter */
        return 0;
    } else if (m == 2) {
        if (skip_tree(f)!=0) return -1;    /* izquierda */
        if (skip_tree(f)!=0) return -1;    /* derecha */
        return 0;
    } else {
        return -1; /* marcador inválido */
    }
}

/* Copia el árbol serializado (incluyendo el byte final 0xFF) a un buffer contiguo. */
static int read_tree_blob(FILE *f, uint8_t **out, size_t *out_len) {
    long start = ftell(f);
    if (start < 0) return -1;

    if (skip_tree(f)!=0) return -1;

    unsigned char end;
    if (fread(&end,1,1,f)!=1 || end != 255) return -1;

    long endpos = ftell(f);
    if (endpos < 0) return -1;

    size_t len = (size_t)(endpos - start);
    if (fseek(f, start, SEEK_SET)!=0) return -1;

    uint8_t *buf = (uint8_t*)malloc(len);
    if (!buf) return -1;

    if (fread(buf,1,len,f)!=len) { free(buf); return -1; }

    *out = buf; *out_len = len;
    return 0;
}

/* Construye un índice de metadatos por entrada sin materializar payloads, copiando el blob del árbol. */
int hfa_index(const char *archive_path, hfa_meta_t **out_meta, uint32_t *out_nfiles) {
    FILE *f = fopen(archive_path, "rb");
    if (!f) return -1;

    hfa_header_t hdr;
    if (fread(&hdr,sizeof(hdr),1,f)!=1 || memcmp(hdr.magic,"HFA1",4)!=0) { fclose(f); return -1; }

    hfa_meta_t *M = (hfa_meta_t*)calloc(hdr.nfiles, sizeof(hfa_meta_t));
    if (!M) { fclose(f); return -1; }

    for (uint32_t i=0;i<hdr.nfiles;i++){
        uint16_t name_len = get_u16(f);
        M[i].name = (char*)malloc(name_len+1);
        if (!M[i].name){ fclose(f); hfa_free_index(M,i); return -1; }
        if (fread(M[i].name,1,name_len,f)!=name_len){ fclose(f); hfa_free_index(M,i+1); return -1; }
        M[i].name[name_len]='\0';

        M[i].orig_len   = get_u64(f);

        if (read_tree_blob(f, &M[i].tree_blob, &M[i].tree_len)!=0) {
            fclose(f); hfa_free_index(M,i+1); return -1;
        }

        M[i].bit_count  = get_u64(f);
        M[i].byte_count = get_u64(f);
        long off = ftell(f);
        if (off < 0) { fclose(f); hfa_free_index(M,i+1); return -1; }
        M[i].payload_off = off;

        if (fseek(f, (long)M[i].byte_count, SEEK_CUR)!=0) { fclose(f); hfa_free_index(M,i+1); return -1; }
    }

    fclose(f);
    *out_meta   = M;
    *out_nfiles = hdr.nfiles;
    return 0;
}

/* Libera el índice construido por hfa_index. */
void hfa_free_index(hfa_meta_t *M, uint32_t n){
    if (!M) return;
    for (uint32_t i=0;i<n;i++){
        free(M[i].name);
        free(M[i].tree_blob);
    }
    free(M);
}

/* Empaqueta una cadena de '0'/'1' a un buffer de bytes, informando conteo de bits. */
void pack_bits_from_bitstr(const char *bitstr, uint8_t **out, size_t *out_len, uint64_t *out_bit_count){
    uint64_t bits = (uint64_t)strlen(bitstr);
    size_t bytes = (size_t)((bits + 7) / 8);
    uint8_t *buf = (uint8_t*)calloc(bytes ? bytes : 1, 1);
    uint64_t b = 0;
    for (; bitstr[b]; ++b){
        if (bitstr[b] == '1'){
            size_t byte = (size_t)(b >> 3);
            int    off  = 7 - (b & 7);
            buf[byte] |= (1u << off);
        }
    }
    *out = buf; *out_len = bytes; *out_bit_count = bits;
}

/* Expande un buffer de bytes a la cadena original de '0'/'1' usando bit_count como límite. */
char* unpack_bits_to_bitstr(const uint8_t *buf, size_t len, uint64_t bit_count){
    (void)len; /* len no es estrictamente necesario si se confía en bit_count */
    char *s = (char*)malloc((size_t)bit_count + 1);
    for (uint64_t b=0;b<bit_count;b++){
        size_t byte = (size_t)(b >> 3);
        int    off  = 7 - (b & 7);
        s[b] = (buf[byte] & (1u << off)) ? '1' : '0';
    }
    s[bit_count] = '\0';
    return s;
}

/* Escribe un .hfa completo a partir de un vector de entradas. */
int hfa_write(const char *archive_path, hfa_entry_t *E, uint32_t n){
    FILE *f = fopen(archive_path, "wb");
    if (!f) return -1;

    hfa_header_t hdr; memcpy(hdr.magic,"HFA1",4); hdr.nfiles=n;
    fwrite(&hdr,sizeof(hdr),1,f);

    for (uint32_t i=0;i<n;i++){
        size_t name_len = strlen(E[i].name);
        if (name_len > UINT16_MAX){ fclose(f); return -1; }

        put_u16(f, (uint16_t)name_len);
        fwrite(E[i].name, 1, name_len, f);
        put_u64(f, (uint64_t)E[i].txt_len);

        if (!serializar_arbol(E[i].raiz, f)) { fclose(f); return -1; }

        put_u64(f, E[i].bit_count);
        put_u64(f, (uint64_t)E[i].packed_len);
        if (E[i].packed_len){
            if (fwrite(E[i].packed, 1, E[i].packed_len, f)!=E[i].packed_len){ fclose(f); return -1; }
        }
    }

    return fclose(f)==0 ? 0 : -1;
}

/* Extrae secuencialmente todas las entradas de un .hfa; elimina el archivo si tuvo éxito. */
int hfa_read_and_extract(const char *archive_path, const char *dir){
    FILE *f = fopen(archive_path, "rb");
    if (!f) return -1;

    hfa_header_t hdr;
    if (fread(&hdr,sizeof(hdr),1,f)!=1 || memcmp(hdr.magic,"HFA1",4)!=0){ fclose(f); return -1; }

    for (uint32_t i=0;i<hdr.nfiles;i++){
        uint16_t name_len = get_u16(f);
        char *name = (char*)malloc(name_len+1);
        if (!name){ fclose(f); return -1; }
        if (fread(name,1,name_len,f)!=name_len){ free(name); fclose(f); return -1; }
        name[name_len]='\0';

        uint64_t orig_len = get_u64(f);

        struct Nodo *raiz = deserializar_arbol(f);
        if (!raiz){ free(name); fclose(f); return -1; }

        uint64_t bit_count = get_u64(f);
        uint64_t byte_count= get_u64(f);

        uint8_t *payload = NULL;
        if (byte_count){
            payload = (uint8_t*)malloc((size_t)byte_count);
            if (!payload){ liberar_arbol(raiz); free(name); fclose(f); return -1; }
            if (fread(payload,1,(size_t)byte_count,f)!=(size_t)byte_count){
                free(payload); liberar_arbol(raiz); free(name); fclose(f); return -1;
            }
        }

        char *bitstr = unpack_bits_to_bitstr(payload, (size_t)byte_count, bit_count);
        char *texto  = descomprimir_texto(raiz, bitstr, (long)orig_len);

        char out_path[PATH_MAX]; join_path(dir, name, out_path);
        if (texto){
            if (write_file_text(out_path, texto, (size_t)orig_len)!=0){
                free(texto); free(bitstr); free(payload); liberar_arbol(raiz); free(name); fclose(f); return -1;
            }
        } else {
            free(bitstr); free(payload); liberar_arbol(raiz); free(name); fclose(f); return -1;
        }

        free(texto); free(bitstr); free(payload); liberar_arbol(raiz); free(name);
    }

    fclose(f);
    remove(archive_path);
    return 0;
}
