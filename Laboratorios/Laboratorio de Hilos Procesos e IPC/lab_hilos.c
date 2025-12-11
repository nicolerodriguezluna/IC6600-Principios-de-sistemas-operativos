#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define TAM_MAX 1000
#define MAX_HILOS 16  // Número máximo de hilos trabajando simultáneamente
#define MAX_PROCESOS_SIMULTANEOS 100

// Declarar las matrices de forma estática y global para evitar segmentation fault
int matriz_A[TAM_MAX][TAM_MAX];
int matriz_B[TAM_MAX][TAM_MAX];
int matriz_C[TAM_MAX][TAM_MAX]; // Matriz para el resultado

// Variables globales para el pool de hilos
int m_global, n_global;
int siguiente_celda = 0;
int total_celdas = 0;
pthread_mutex_t mutex_trabajo = PTHREAD_MUTEX_INITIALIZER;

// Estructura para compartir datos entre procesos usando mmap
typedef struct {
    int matriz_A[TAM_MAX][TAM_MAX];
    int matriz_B[TAM_MAX][TAM_MAX];
    int matriz_C[TAM_MAX][TAM_MAX];
    int m;  // dimensiones
    int n;
    int proceso_terminado[TAM_MAX * TAM_MAX];  // flag para cada celda
} shared_data_t;

// Variables globales
shared_data_t *shared_mem = NULL;
int shm_fd = -1;

// Función para inicializar
void crear_matriz(int filas, int columnas, int matriz[TAM_MAX][TAM_MAX]) {
    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < columnas; j++) {
            // Genera un número aleatorio entre 1 y 5
            matriz[i][j] = rand() % 5 + 1;
        }
    }
}

// Función para mostrar una matriz
void mostrar_matriz(int filas, int columnas, int matriz[TAM_MAX][TAM_MAX]) {
    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < columnas; j++) {
            printf("%d\t", matriz[i][j]);
        }
        printf("\n");
    }
}

// Función para multiplicar dos matrices y guardar el resultado en una tercera matriz
void multiplicar_matrices(int m, int n, int matriz_B[TAM_MAX][TAM_MAX], int matriz_A[TAM_MAX][TAM_MAX], int matriz_C[TAM_MAX][TAM_MAX]) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < m; j++) {
            matriz_C[i][j] = 0; // Inicializa el elemento C[i][j] a 0
            for (int k = 0; k < n; k++) {
                matriz_C[i][j] += matriz_B[i][k] * matriz_A[k][j];
            }
        }
    }
}

// Función para obtener la siguiente celda a calcular
int obtener_siguiente_celda(int* i, int* j) {
    pthread_mutex_lock(&mutex_trabajo);
    
    if (siguiente_celda >= total_celdas) {
        pthread_mutex_unlock(&mutex_trabajo);
        return 0; // No hay más trabajo
    }
    
    *i = siguiente_celda / m_global;
    *j = siguiente_celda % m_global;
    siguiente_celda++;
    
    pthread_mutex_unlock(&mutex_trabajo);
    return 1; // Hay trabajo disponible
}

// Función que cada hilo ejecutará
void* trabajador_hilos(void* arg) {
    int i, j;
    
    // Cada hilo procesa múltiples celdas hasta que no haya más trabajo
    while (obtener_siguiente_celda(&i, &j)) {
        // Calcular la celda [i][j]
        matriz_C[i][j] = 0;
        for (int k = 0; k < n_global; k++) {
            matriz_C[i][j] += matriz_B[i][k] * matriz_A[k][j];
        }
    }
    
    pthread_exit(NULL);
}

// Función para multiplicar matrices usando pool de hilos
void multiplicar_matrices_con_hilos(int m, int n) {
    // Configurar variables globales
    m_global = m;
    n_global = n;
    siguiente_celda = 0;
    total_celdas = m * m;
    
    // Determinar número óptimo de hilos (no más que el número de CPUs)
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int num_hilos = (num_cpus > MAX_HILOS) ? MAX_HILOS : num_cpus;
    
    // Crear array de hilos
    pthread_t hilos[MAX_HILOS];
    
    // Crear los hilos trabajadores
    for (int i = 0; i < num_hilos; i++) {
        if (pthread_create(&hilos[i], NULL, trabajador_hilos, NULL) != 0) {
            perror("Error al crear hilo");
            exit(EXIT_FAILURE);
        }
    }
    
    // Esperar a que todos los hilos terminen
    for (int i = 0; i < num_hilos; i++) {
        pthread_join(hilos[i], NULL);
    }
}

// Función para limpiar memoria compartida
void cleanup() {
    if (shared_mem != NULL) {
        munmap(shared_mem, sizeof(shared_data_t));
    }
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink("/matrix_multiplication");
    }
}

// Manejador de señales para limpieza
void signal_handler(int sig) {
    printf("\nRecibida señal %d, limpiando...\n", sig);
    cleanup();
    exit(1);
}

// Función para inicializar memoria compartida
int inicializar_memoria_compartida() {
    // Crear objeto de memoria compartida
    shm_fd = shm_open("/matrix_multiplication", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error en shm_open");
        return -1;
    }
    
    // Establecer el tamaño
    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) {
        perror("Error en ftruncate");
        close(shm_fd);
        shm_unlink("/matrix_multiplication");
        return -1;
    }
    
    // Mapear la memoria
    shared_mem = (shared_data_t *)mmap(NULL, sizeof(shared_data_t), 
                                       PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("Error en mmap");
        close(shm_fd);
        shm_unlink("/matrix_multiplication");
        return -1;
    }
    
    return 0;
}

// Función que ejecuta cada proceso hijo para calcular una celda específica
void calcular_celda_proceso(int fila, int columna) {
    // Cada proceso hijo mapea la memoria compartida
    int child_shm_fd = shm_open("/matrix_multiplication", O_RDWR, 0666);
    if (child_shm_fd == -1) {
        exit(EXIT_FAILURE);
    }
    
    shared_data_t *child_shared = (shared_data_t *)mmap(NULL, sizeof(shared_data_t),
                                                        PROT_READ | PROT_WRITE, MAP_SHARED,
                                                        child_shm_fd, 0);
    if (child_shared == MAP_FAILED) {
        close(child_shm_fd);
        exit(EXIT_FAILURE);
    }
    
    // Calcular la celda asignada: C[fila][columna] = suma(B[fila][k] * A[k][columna])
    int resultado = 0;
    for (int k = 0; k < child_shared->n; k++) {
        resultado += child_shared->matriz_B[fila][k] * child_shared->matriz_A[k][columna];
    }
    
    // Escribir el resultado en la matriz C
    child_shared->matriz_C[fila][columna] = resultado;
    
    // Marcar como terminado
    int celda_index = fila * child_shared->m + columna;
    child_shared->proceso_terminado[celda_index] = 1;
    
    // Limpiar memoria del proceso hijo
    munmap(child_shared, sizeof(shared_data_t));
    close(child_shm_fd);
    
    exit(EXIT_SUCCESS);
}

// Función para multiplicar matrices usando procesos fork
void multiplicar_matrices_con_fork(int m, int n) {
    pid_t procesos[TAM_MAX * TAM_MAX];
    int total_celdas = m * m;
    int procesos_activos = 0;
    int celdas_completadas = 0;
    
    // Inicializar flags de procesos terminados
    memset(shared_mem->proceso_terminado, 0, sizeof(shared_mem->proceso_terminado));
    
    printf("Creando %d procesos para calcular matriz %dx%d...\n", total_celdas, m, m);
    
    // Crear procesos en lotes para evitar sobrecargar el sistema
    for (int celda = 0; celda < total_celdas; celda++) {
        int fila = celda / m;
        int columna = celda % m;
        
        // Limitar procesos simultáneos
        while (procesos_activos >= MAX_PROCESOS_SIMULTANEOS) {
            // Esperar a que termine algún proceso
            int status;
            pid_t pid_terminado = wait(&status);
            if (pid_terminado > 0) {
                procesos_activos--;
                celdas_completadas++;
                
                // Mostrar progreso cada 100000 celdas
                if (celdas_completadas % 100000 == 0) {
                    printf("Completadas %d/%d celdas (%.1f%%)\n", 
                           celdas_completadas, total_celdas, 
                           (float)celdas_completadas * 100 / total_celdas);
                }
            }
        }
        
        // Crear nuevo proceso hijo
        pid_t pid = fork();
        
        if (pid == 0) {
            // Proceso hijo: calcular la celda específica
            calcular_celda_proceso(fila, columna);
        } else if (pid > 0) {
            // Proceso padre: guardar el PID
            procesos[celda] = pid;
            procesos_activos++;
        } else {
            perror("Error en fork");
            exit(EXIT_FAILURE);
        }
    }
    
    // Esperar a que terminen todos los procesos restantes
    while (procesos_activos > 0) {
        int status;
        pid_t pid_terminado = wait(&status);
        if (pid_terminado > 0) {
            procesos_activos--;
            celdas_completadas++;
            
            if (celdas_completadas % 1000 == 0) {
                printf("Completadas %d/%d celdas (%.1f%%)\n", 
                       celdas_completadas, total_celdas, 
                       (float)celdas_completadas * 100 / total_celdas);
            }
        }
    }
    
    printf("Todos los procesos terminaron. Celdas completadas: %d/%d\n", 
           celdas_completadas, total_celdas);
}

int main() {
    int n, m, ciclos;
    double tiempos[100]; // Array para almacenar los tiempos de cada ciclo
    double suma, promedio, desviacion = 0.0;
    
    // Configurar manejadores de señales para limpieza
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Ingrese un numero de entero positivo (N): ");
    scanf("%d", &n);

    printf("Ingrese otro numero de entero positivo (M): ");
    scanf("%d", &m);

    printf("Ingrese otro numero de entero positivo (Ciclos): ");
    scanf("%d", &ciclos);

    if (n > 0 && m > 0 && n <= TAM_MAX && m <= TAM_MAX && ciclos > 0) {
        //printf("Usando hasta %d hilos simultáneos\n", (sysconf(_SC_NPROCESSORS_ONLN) > MAX_HILOS) ? MAX_HILOS : sysconf(_SC_NPROCESSORS_ONLN));
        
        // Inicializar memoria compartida
        if (inicializar_memoria_compartida() == -1) {
            printf("Error al inicializar memoria compartida\n");
            return 1;
        }
        
        // Configurar dimensiones en memoria compartida
        shared_mem->m = m;
        shared_mem->n = n;
        
        for (int i = 0; i < ciclos; i++) {
            // Mostrar progreso
            if (i % 10 == 0) {
                printf("Ciclo %d/%d...\n", i + 1, ciclos);
            }
            // Medir tiempo de inicio
            clock_t inicio = clock();
            
            // Llenar y mostrar la matriz de N x M
            crear_matriz(n, m, matriz_A);
            //printf("\nMatriz de %d x %d:\n", n, m);
            //mostrar_matriz(n, m, matriz_A);

            // Llenar y mostrar la matriz de M x N
            crear_matriz(m, n, matriz_B);
            //printf("\nMatriz de %d x %d:\n", m, n);
            //mostrar_matriz(m, n, matriz_B);
            
            // Multiplicar B x A y almacenar el resultado en C
            //multiplicar_matrices(m, n, matriz_B, matriz_A, matriz_C);
            
            // Multiplicar matrices usando hilos
            //multiplicar_matrices_con_hilos(m, n);
            
            // Multiplicar matrices usando fork
            multiplicar_matrices_con_fork(m, n);
            
            // Medir tiempo de inicio
            clock_t fin = clock();
            
            // Calcular tiempo transcurrido en segundos
            double tiempo = ((double)(fin - inicio)) / CLOCKS_PER_SEC;
            tiempos[i] = tiempo;
            suma += tiempo;

            // Mostrar la matriz resultante C (m x m)
            //printf("\n--- Matriz C (%d x %d) = B x A ---\n", m, m);
            //mostrar_matriz(m, m, matriz_C);
        }
        // Calcular promedio
        promedio = suma / ciclos;
        
        // Calcular desviación estándar
        for (int i = 0; i < ciclos; i++) {
            desviacion += pow(tiempos[i] - promedio, 2);
        }
        desviacion = sqrt(desviacion / ciclos);
        
        // Mostrar resultados
        printf("\n--- Resultados después de %d ciclos ---\n", ciclos);
        printf("Tiempo promedio: %.4f segundos\n", promedio);
        printf("Desviación estándar: %.4f segundos\n", desviacion);
        printf("Tiempo total: %.4f segundos\n", suma);
        
        // Análisis del orden del algoritmo
        printf("\n--- Análisis del orden del algoritmo ---\n");
        printf("La multiplicación de matrices de %dx%d y %dx%d tiene complejidad O(n^3)\n", m, n, n, m);
        printf("Para n=1000, se esperan aproximadamente 1000^3 = 1,000,000,000 operaciones\n");
        printf("El tiempo teórico esperado depende del hardware, pero con %.4f segundos,\n", promedio);
        printf("podemos estimar un rendimiento de %.2f GFLOP/s\n", 
               (2.0 * n * n * n) / (promedio * 1e9)); // 2 operaciones por elemento (multiplicación y suma)
    } else {
        printf("\nError: Debes ingresar numeros enteros positivos y no mayores de %d.\n", TAM_MAX);
        return 1;
    }
    // Limpiar memoria compartida
    cleanup();
    
    return 0;
}