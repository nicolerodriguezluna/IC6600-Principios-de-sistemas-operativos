#ifndef THREAD_POOL_H
#define THREAD_POOL_H

/* ======================================================================
 *  THREAD_POOL.H — Interfaz del threadpool simple para tareas por archivo
 * ====================================================================== */

#include "common.h"

/* --- Declaracion de la función de trabajo --- */
typedef void (*work_fn)(void *arg);

/* --- Nodo de trabajo en cola FIFO --- */
typedef struct work_item
{
    work_fn fn;
    void *arg;
    struct work_item *next;
} work_item_t;

/* --- Estructura del threadpool y su cola --- */
typedef struct
{
    pthread_mutex_t mtx;        /* protección de cola/contadores */
    pthread_cond_t cv;          /* señal para nuevos trabajos o cola vacía */
    work_item_t *head, *tail;   /* cola FIFO de trabajos */
    bool shutting_down;         /* bandera de cierre */
    int active;                 /* trabajos actualmente en ejecución */
    int threads;                /* número de hilos en el pool */
    pthread_t *tids;            /* IDs de hilos */
} thread_pool_t;

/* --- API del threadpool --- */
int tp_init(thread_pool_t *tp, int threads);                /* inicializa hilos y cola */
void tp_submit(thread_pool_t *tp, work_fn fn, void *arg);   /* mete en la cola un trabajo */
void tp_wait(thread_pool_t *tp);                            /* espera cola vacía y sin activos */
void tp_destroy(thread_pool_t *tp);                         /* apaga hilos y libera recursos */

#endif
