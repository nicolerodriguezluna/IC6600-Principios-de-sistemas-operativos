#ifndef HUFFIO_H
#define HUFFIO_H

/* ======================================================================
 *  HUFFIO.H — Formato de archivo único (.hfa) y helpers de empaquetado
 * ====================================================================== */

#include "common.h"

/* --- Header global del .hfa --- */
typedef struct __attribute__((packed)) {
    char magic[4];
    uint32_t nfiles;
} hfa_header_t;

/* --- Entrada preparada para escritura del .hfa en compresión --- */
typedef struct {
    char  *name;          /* nombre base */ 
    char  *txt;           /* contenido original mientras se comprime */
    size_t txt_len;       /* largo del archivo */ 
    struct Nodo *raiz;    /* arbol por archivo */ 
    char  **tabla;        /* tabla de códigos */
    char  *bitstr;        /* '0'/'1' string */ 
    uint64_t bit_count;   /* bits del payload */ 
    uint8_t *packed;      /* bits empaquetados */
    size_t packed_len;    /* longitud en bytes de 'packed' */
} hfa_entry_t;

/* --- Metadata para descompresión paralela --- */
typedef struct {
    char     *name;           /*  nombre del archivo a restaurar */ 
    uint64_t  orig_len;       /* bytes del .txt original */ 
    uint64_t  bit_count;      /* bits válidos del payload */ 
    uint64_t  byte_count;     /* bytes del payload */ 
    long      payload_off;    /* offset en el .hfa donde empieza el payload */
    uint8_t  *tree_blob;      /* copia en memoria del árbol serializado */
    size_t    tree_len;       /* tamaño de ese blob */ 
} hfa_meta_t;

/* --- Helpers de empaquetado/desempaquetado de bits --- */
void     pack_bits_from_bitstr(const char *bitstr, uint8_t **out, size_t *out_len, uint64_t *out_bit_count);
char*    unpack_bits_to_bitstr(const uint8_t *buf, size_t len, uint64_t bit_count);

/* --- Escritura/lectura de archivo binario --- */
int hfa_write(const char *archive_path, hfa_entry_t *entries, uint32_t nfiles);
int hfa_read_and_extract(const char *archive_path, const char *dir);

/* --- Indexado para descompresión paralela --- */
int  hfa_index(const char *archive_path, hfa_meta_t **out_meta, uint32_t *out_nfiles);
void hfa_free_index(hfa_meta_t *meta, uint32_t nfiles);

#endif
