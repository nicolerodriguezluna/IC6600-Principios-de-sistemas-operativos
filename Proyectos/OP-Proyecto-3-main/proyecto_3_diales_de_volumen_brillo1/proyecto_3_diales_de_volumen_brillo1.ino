// Pines para LEDs de VOLUMEN
int ledVol1 = 7;
int ledVol2 = 6;
int ledVol3 = 5;

// Pines para LEDs de BRILLO
int ledBri1 = 10;
int ledBri2 = 9;
int ledBri3 = 8;

// Pines para potenciómetros
int potVolumen = A0;
int potBrillo = A1;

// Variables para controlar cambios
int volumenAnterior = -1;
int brilloAnterior = -1;

void setup() {
  // Configurar LEDs de VOLUMEN como salidas
  pinMode(ledVol1, OUTPUT);
  pinMode(ledVol2, OUTPUT);
  pinMode(ledVol3, OUTPUT);
  
  // Configurar LEDs de BRILLO como salidas
  pinMode(ledBri1, OUTPUT);
  pinMode(ledBri2, OUTPUT);
  pinMode(ledBri3, OUTPUT);
  
  // Iniciar comunicación serial con la computadora
  Serial.begin(9600);
  
  // Encender todos los LEDs momentáneamente para indicar inicio
  encenderTodosLEDs();
  delay(500);
  apagarTodosLEDs();
  
  Serial.println("ARDUINO_INICIADO");
}

void loop() {
  // Leer valores de los potenciómetros (0-1023)
  int valorVolumen = analogRead(potVolumen);
  int valorBrillo = analogRead(potBrillo);
  
  // Convertir a porcentaje (0-100%)
  int volumenActual = map(valorVolumen, 0, 1023, 0, 101);
  if (volumenActual > 100) volumenActual = 100;

  int brilloActual = map(valorBrillo, 0, 1023, 0, 101);
  if (brilloActual > 100) brilloActual = 100;

  // Controlar LEDs de VOLUMEN según el porcentaje
  controlarLEDsVolumen(volumenActual);
  
  // Controlar LEDs de BRILLO según el porcentaje
  controlarLEDsBrillo(brilloActual);
  
  // ENVIAR DATOS A LA COMPUTADORA solo si hay cambios
  if (volumenActual != volumenAnterior) {
    enviarDatosComputadora('V', volumenActual);
    volumenAnterior = volumenActual;
  }
  
  if (brilloActual != brilloAnterior) {
    enviarDatosComputadora('B', brilloActual);
    brilloAnterior = brilloActual;
  }
  
  // Leer mensajes de la computadora (comunicación bidireccional)
  if (Serial.available() > 0) {
    String mensaje = Serial.readString();
    mensaje.trim();
    
    if (mensaje == "COMPUTADORA_LISTA") {
      // La computadora confirma que está recibiendo datos
      indicadorConexionExitosa();
    }
  }
  
  delay(100); // Pequeña pausa para estabilidad
}

void controlarLEDsVolumen(int porcentaje) {
  // LEDs se encienden progresivamente: 33%, 66%, 100%
  digitalWrite(ledVol1, porcentaje > 33 ? HIGH : LOW);
  digitalWrite(ledVol2, porcentaje > 66 ? HIGH : LOW);
  digitalWrite(ledVol3, porcentaje > 90 ? HIGH : LOW);
}

void controlarLEDsBrillo(int porcentaje) {
  // LEDs se encienden progresivamente: 33%, 66%, 100%
  digitalWrite(ledBri1, porcentaje > 33 ? HIGH : LOW);
  digitalWrite(ledBri2, porcentaje > 66 ? HIGH : LOW);
  digitalWrite(ledBri3, porcentaje > 90 ? HIGH : LOW);
}

void enviarDatosComputadora(char tipo, int valor) {
  // Enviar datos en formato: V:75 o B:50
  Serial.print(tipo);
  Serial.print(":");
  Serial.println(valor);
}

void indicadorConexionExitosa() {
  // Parpadeo especial cuando la computadora se conecta
  for(int i = 0; i < 3; i++) {
    digitalWrite(ledVol1, HIGH);
    digitalWrite(ledBri1, HIGH);
    delay(200);
    digitalWrite(ledVol1, LOW);
    digitalWrite(ledBri1, LOW);
    delay(200);
  }
}

void encenderTodosLEDs() {
  digitalWrite(ledVol1, HIGH);
  digitalWrite(ledVol2, HIGH);
  digitalWrite(ledVol3, HIGH);
  digitalWrite(ledBri1, HIGH);
  digitalWrite(ledBri2, HIGH);
  digitalWrite(ledBri3, HIGH);
}

void apagarTodosLEDs() {
  digitalWrite(ledVol1, LOW);
  digitalWrite(ledVol2, LOW);
  digitalWrite(ledVol3, LOW);
  digitalWrite(ledBri1, LOW);
  digitalWrite(ledBri2, LOW);
  digitalWrite(ledBri3, LOW);
}