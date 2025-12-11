#include <stdio.h>
#include <time.h>

//Fibonacci Recursion de Pila
long fibonacci_pila(long n){
  printf(".");
  if (n <= 1){
    return n;
  } else {
    return fibonacci_pila(n-1) + fibonacci_pila(n-2);
  }
}

int main(){
  long n;
  long res;
  clock_t inicio, fin;
  double tiempo_transcurrido;
  
  printf("Ingrese un numero entero positivo: ");
  scanf("%d", &n);
  
  if (n < 0){
    printf("Error: Solo se aceptan numeros enteros positivos");
    return 1;
  }
  inicio = clock();
  res = fibonacci_pila(n);
  fin = clock();
  tiempo_transcurrido = (double)(fin - inicio)/CLOCKS_PER_SEC;
  printf("El valor de fibonacci de %d es %ld\n", n, res);
  printf("El calculo tardo: %f segundos\n", tiempo_transcurrido);
  return 0;
}