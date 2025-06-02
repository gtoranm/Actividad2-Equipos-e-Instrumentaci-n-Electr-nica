// ----Librerias----
#include <DHT.h>
#include <LiquidCrystal.h>
#include <SPI.h>

// --- Pines y configuración ---
#define DHT_PIN 7         // Pin digital del Arduino UNO donde está conectado el pin de datos del sensor DHT22
#define DHT_TIPO DHT22    // Tipo de sensor DHT utilizado (Necesario como parametro de entrada mas adelante)
#define BOTON_PIN 6       // Boton que sirve para encender la pantalla en el modo ahorro de energia
#define CALEFACTOR_PIN 8 //LED que simula la resistencia calefactora
#define ENFRIADOR_PIN A5  // LED azul que simula Actuador del Agente Refrigerante
#define ANEMOMETRO_PIN 2 //Voy a sumilar el Anemometro con un Contador de Pulsos
#define LED_POSICION_PIN 10 //LED luces de posicion para cuando la luz exterior no nos permita ver la boya facilmente (15%)
#define DS_PIN    A2   // Datos (DS)
#define LATCH_PIN A3   // Latch (ST_CP)
#define CLOCK_PIN A4   // Reloj (SH_CP)
#define LDR_PIN A0 // Pin analógico conectado a la salida AO del módulo LDR
#define VELETA A1 // Pin Analogico que segun su valor simula la posicion de una veleta

DHT dht(DHT_PIN, DHT_TIPO);
LiquidCrystal lcd(12, 11, 5, 4, 3, 9); // LCD RS, E, D4, D5, D6, D7

unsigned long encendidoTimeout = 5000;  // milisegundos que estará encendida la pantalla
unsigned long contador_botonPulsado = 0;
bool lcd_On = false;
bool carga_On = false;

//Configuracion de un timer para evitar usar el delay()
//Es una mala practica ya que detiene arduino completamente durasnte ese tiempo y podria obviar eventos importantes como el pulsado del boton
unsigned long tiempoAnteriorSimulacion = 0;
const unsigned long retardoTemperatura = 500;  // 500 ms de retardo térmico

unsigned long tiempoActual = 0;
unsigned long tiempoAnterior = 0;
unsigned long intervaloLectura = 1000;

bool mostrandoBienvenida = false;
bool mostrandoTempHum = false;
//Timer para mostrar el mensaje de error
bool mostrandoErrorLectura = false;
bool mostrarAlertaBateria = true;
unsigned long tiempoinicioError = 0;
unsigned long duracionMensaje = 1000;

volatile unsigned long ultimaInterrupcion = 0;
const unsigned long reboteMinimo = 100; // en milisegundos

// --- Variables globales compartidas ---
float temperatura = 22.0;
float temperaturaAnterior = 22.0;
float lecturaTemp = 22.0;
float ultimaTempDHT = -1000.0;
float setpoint = 25.0;
float limiteSuperior = setpoint + 3.0;
float limiteInferior = setpoint - 3.0;
unsigned long tiempoCalefaccion = 0; //En ms
unsigned long tiempoRefrigeracion = 0; //En ms

float humedad = 0.0;

int valorLDR = 0;
int luzPorcentaje = 0;
int numLEDs = 0;
byte ledsEstado = 0;

volatile unsigned int pulsos = 0; //número de activaciones del sensor en 1 segundo
float velocidadViento = 0;
float factor_conversion = 2.4; // km/h por pulso lo proporciona el fabricante del aneometro
String direccionViento = "";
int valorVeleta = 0;

unsigned long bateria = 1000000; //mA PRUEBA FUNCIONAMIENTO NORMAL (BATERIA CARGADA)
//unsigned long bateria = 200000; //mA PRUEBA FUNCIONAMIENTO BATERIA  BAJA
//unsigned long bateria = 100000; //mA PRUEBA FUNCIONAMIENTO BATERIA  MUY BAJA
float PorcentajeBateria = 0.00;
float VelocidadCarga = 0.0;
unsigned long VelocidadGasto = 0;

//---------------------------------FUNCIONES PRINCIPALES--------------------------------------------------------

void leerSensores() { 
  //------------------------DHT22------------------------------------------------------

  // Lectura de temperatura en ºC desde el sensor DHT22
  lecturaTemp = dht.readTemperature();

  if (!isnan(lecturaTemp) && lecturaTemp != ultimaTempDHT) {
    temperatura = lecturaTemp;
    ultimaTempDHT = lecturaTemp;
  }

  // Lectura de humedad relativa en % desde el sensor DHT22
  humedad = dht.readHumidity();

  if (isnan(lecturaTemp) || isnan(humedad)) { //Ante error de lectura dht.readTemperature() y dht.readHumidity() devuelve 'NaN'
    if (!mostrandoErrorLectura) {
      lcd.display();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error DHT22");
      tiempoActual = millis();
      tiempoinicioError = millis();
      mostrandoErrorLectura = true;
    }

    if (millis() - tiempoinicioError < duracionMensaje) {
      return;  // Mientras el mensaje de error esté activo, no hacer nada más
    } else {
      mostrandoErrorLectura = false;  // Resetear para detectar futuros errores
      lcd.clear();             // Limpia mensaje de error antes de continuar
      return;                  // Salir de este ciclo
    }
  }

  //---------------------------LDR--------------------------------------------

  // Lectura del valor analógico desde el módulo LDR
  valorLDR = analogRead(LDR_PIN);

  // Conversión de ese valor a porcentaje de luz (0% = oscuro, 100% = mucha luz)
  luzPorcentaje = map(valorLDR, 0, 1023, 100, 0); // Invertido: +luz = menor voltaje
  numLEDs = map(100 - luzPorcentaje, 0, 100, 0, 8);
  actualizarLEDs(ledsEstado);

  //---------------------------VELETA-----------------------------------------------

  //Lectura de un Potenciometro que segun su valor simula la posicion de una veleta
  valorVeleta = analogRead(VELETA);

  if (valorVeleta < 128) direccionViento = "N";
  else if (valorVeleta < 256) direccionViento = "NE";
  else if (valorVeleta < 384) direccionViento = "E";
  else if (valorVeleta < 512) direccionViento = "SE";
  else if (valorVeleta < 640) direccionViento = "S";
  else if (valorVeleta < 768) direccionViento = "SW";
  else if (valorVeleta < 896) direccionViento = "W";
  else direccionViento = "NW";  // el resto del rango hasta 1023

}


//Control de que se muestra en cada momento por la pantalla LCD
void controlLCD() {

  // --- Mostrar en pantalla solo si está encendida ---
  if (lcd_On) {
    if (!mostrarAlertaBateria ){
      mostrarAlertaBateria = true;
      lcd.clear();
      lcd.display();
      lcd.setCursor(0, 0);
      lcd.print("Bateria");
      lcd.setCursor(0, 1);
      lcd.print("Baja");
      

    } else if (!mostrandoBienvenida) {

      if(PorcentajeBateria < 15){

        lcd.noDisplay();
        lcd_On = false;

      } else if (temperatura < limiteInferior) {
        mostrandoBienvenida = true;
        lcd.clear();
        lcd.display();
        lcd.setCursor(0, 0);
        lcd.print("Atencion!");
        lcd.setCursor(0, 1);
        lcd.print("TempBateria Baja");
      } else if (temperatura > limiteSuperior) {
        mostrandoBienvenida = true;
        lcd.clear();
        lcd.display();
        lcd.setCursor(0, 0);
        lcd.print("Atencion!");
        lcd.setCursor(0, 1);
        lcd.print("TempBateria Alta");
      } else {
        // Mensaje de bienvenida
        mostrandoBienvenida = true;
        lcd.clear();
        lcd.display();
        lcd.setCursor(0, 0);
        lcd.print("Actividad 2");
        lcd.setCursor(0, 1);
        lcd.print("Boya Climatica");
      }
        
    } else if (!mostrandoTempHum){  
      if (temperatura >= limiteInferior && temperatura <= limiteSuperior) {
        // Mostrar Medidas en LCD
        lcd.clear(); // Limpia el mensaje inicial en el LCD
        lcd.setCursor(0, 0);
        lcd.print("T:");
        lcd.print(lecturaTemp, 1);
        lcd.print("C ");
        lcd.print("Hum:");
        lcd.print(humedad, 0);
        lcd.print("% ");

        lcd.setCursor(0, 1);
        lcd.print("Vel:");
        lcd.print(velocidadViento,1);
        lcd.print("kmh ");
        lcd.print(direccionViento);

      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TAnterior:");
        lcd.print(temperaturaAnterior, 1);
        lcd.print("C ");

        lcd.setCursor(0, 1);
        lcd.print("TActual:");
        lcd.print(temperatura, 1);
        lcd.print("C ");

      }

      mostrandoTempHum = true;
      
    } else {

      if (temperatura >= limiteInferior && temperatura <= limiteSuperior) {
        // Mostrar Medidas en LCD
        lcd.clear(); // Limpia el mensaje inicial en el LCD
        lcd.setCursor(0, 0);
        lcd.print("Luz:");
        lcd.print(luzPorcentaje);
        lcd.print("%" );

        lcd.setCursor(0, 1);
        lcd.print("Batt:");
        lcd.print(PorcentajeBateria);
        lcd.print("%");


      } else {
        //Estimacion de consumo Energetico con tiempos de Calefacción y Refrigeracion activos
        lcd.clear();

        if (temperatura < limiteInferior) {
          lcd.setCursor(0, 0);
          lcd.print("t Calef: ");
          lcd.print(tiempoCalefaccion / 1000);
          lcd.print("s");
          lcd.setCursor(0, 1);
          lcd.print("Batt:");
          lcd.print(PorcentajeBateria);
          lcd.print("%");
        } else if (temperatura > limiteSuperior) {
          lcd.setCursor(0, 0);
          lcd.print("t Refrig: ");
          lcd.print(tiempoRefrigeracion / 1000);
          lcd.print("s");
          lcd.setCursor(0, 1);
          lcd.print("Batt:");
          lcd.print(PorcentajeBateria);
          lcd.print("%");     
        }

      }

      mostrandoTempHum = false;

    }
  }
}


//Logica control del sistema segun la temperatura
void controlTemperatura() {

  // --- Lógica de control de Temperatura y carga de batería ---
  if (temperatura < limiteInferior) {
    digitalWrite(CALEFACTOR_PIN, HIGH);  // Calentar si hace frío
    digitalWrite(ENFRIADOR_PIN, LOW);
    carga_On = false;
  } else if (temperatura > limiteSuperior) {
    digitalWrite(CALEFACTOR_PIN, LOW);   // Enfriar si hace calor excesivo
    digitalWrite(ENFRIADOR_PIN, HIGH);
    carga_On = false;
  } else {
    digitalWrite(CALEFACTOR_PIN, LOW);   // Temperatura óptima
    digitalWrite(ENFRIADOR_PIN, LOW);
    carga_On = true;
  }

  //Simulador Calentador/Enfriador
  if (millis() - tiempoAnteriorSimulacion >= retardoTemperatura) {
    tiempoAnteriorSimulacion = millis();

    if (temperatura <= limiteInferior) {
      temperatura += 0.5;
      tiempoCalefaccion += retardoTemperatura;
      Serial.print("Calentando Bateria... Temp: "); Serial.print(temperatura); Serial.println(" °C");
      Serial.println("");
    } else if (temperatura >= limiteSuperior) {
      temperatura -= 0.5;
      tiempoRefrigeracion += retardoTemperatura;
      Serial.print("Enfriando Bateria... Temp: "); Serial.print(temperatura); Serial.println(" °C");
      Serial.println("");
    }

  }
}


// Lógica del 74HC595 y LED de emergencia
void controlIluminacion() {
  ledsEstado = 0;
  
  // --- Control automático de luces de posicionamiento ---
  if ((carga_On == false && luzPorcentaje < 15) || PorcentajeBateria < 15){   //Si la bateria no puede Cargar (Podriamos poner como condicion tambien que la bateria baje de cierta carga)

    analogWrite(LED_POSICION_PIN, 180);  // Oscuro: encender con brillo medio-alto luces LED de Señalización

  } else {
    // Subir/bajar nivel según luz medida
    analogWrite(LED_POSICION_PIN, 0); //Reseteo Luz de Posicionamiento de Emergencia

    for (int i = 0; i < numLEDs; i++) {
      ledsEstado |= (1 << i);
    }

    if (luzPorcentaje < 80 && ledsEstado < 255) {
      ledsEstado = (ledsEstado << 1) | 1;  // Añade un LED
    } else if (luzPorcentaje > 80 && ledsEstado > 0) {
      ledsEstado = ledsEstado >> 1;        // Quita un LED
    } 
  }

  // Enviar nuevo estado al 74HC595
  actualizarLEDs(ledsEstado);
}


void controlBateria(){
    // Control Bateria

  if ((carga_On == false && luzPorcentaje < 15) || PorcentajeBateria < 15) VelocidadGasto += 1; //Se encenderia la luz de posicion de Emergencia

  if (digitalRead(CALEFACTOR_PIN) == HIGH) VelocidadGasto += 4;
  if (digitalRead(ENFRIADOR_PIN) == HIGH) VelocidadGasto += 3;
  if (lcd_On) VelocidadGasto += 10;

  //Gestion Gasto Bateria Luces de Posicionamiento
  if (ledsEstado == 1){
    VelocidadGasto += 1;
  } else if (ledsEstado == 3){
    VelocidadGasto += 2;
  } else if (ledsEstado == 7){
    VelocidadGasto += 3;
  } else if (ledsEstado == 15){
    VelocidadGasto += 4;
  } else if (ledsEstado == 31){
    VelocidadGasto += 5;
  } else if (ledsEstado == 63){
    VelocidadGasto += 6;
  } else if (ledsEstado == 127){
    VelocidadGasto += 7;
  } else if (ledsEstado == 255){
    VelocidadGasto += 8;
  }

  bateria -= VelocidadGasto;

  VelocidadCarga = map(luzPorcentaje, 0, 100, 0, 60); //Velocidad de CargaMax 60u/s en funcion del %Luz (Panel Solar)
  if(carga_On == true){
    bateria += VelocidadCarga;
  }
  
  //ControldeRangos de Bateria
  if(bateria < 0){
    bateria = 0;
  } else if(bateria > 1000000){
    bateria = 1000000;
  }

  PorcentajeBateria = (float)bateria / 10000.0;

}



//------------------------------------FLUJO PRINCIPAL---------------------------------------

void setup() {
  pinMode(CALEFACTOR_PIN, OUTPUT);
  pinMode(ENFRIADOR_PIN, OUTPUT);
  pinMode(LED_POSICION_PIN, OUTPUT);
  pinMode(DS_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(BOTON_PIN, INPUT_PULLUP);  // Botón con resistencia pull-up interna para evitar lecturas erroneas flotantes cuando el boton no esta pulsado 
  pinMode(ANEMOMETRO_PIN, INPUT_PULLUP); // Botón con resistencia pull-up interna para evitar lecturas erroneas flotantes cuando el boton no esta pulsado

  attachInterrupt(digitalPinToInterrupt(ANEMOMETRO_PIN), contarPulsos, FALLING);

  // Inicializa la comunicación serie para monitoreo por consola
  Serial.begin(9600);

  // Inicializa el sensor DHT
  dht.begin();

  // Inicializa la pantalla LCD con 16 columnas y 2 filas
  lcd.begin(16, 2);

  lcd.noDisplay();  // Pantalla apagada al inicio

}

void loop() {

  tiempoActual = millis();

  // Leer sensores y tratamiento de las medidas
  leerSensores();
 
  //Logica de Control de Estado de Ahorro de Bateria y Encendido de la Pantalla
  if (!carga_On || luzPorcentaje < 30 || PorcentajeBateria < 30) { //Sistema no puede cargar la bateria o hay poca luz o Poca Bateria (Supongo carga a traves de paneles solares)

      // Activar pantalla con botón
      if (digitalRead(BOTON_PIN) == LOW) { //Resistencia Internea Pull-Up pone Boton a HIGH por defecto
        contador_botonPulsado = millis();
        tiempoActual = millis();
        lcd_On = true;
        if(PorcentajeBateria < 15){
          mostrarAlertaBateria = false;
        } else{
          mostrarAlertaBateria = true;
        }
      }

      // Apagar pantalla tras 3 segundos sin interacción
      if (lcd_On && millis() - contador_botonPulsado > encendidoTimeout) {
        lcd.noDisplay();
        lcd_On = false;
        mostrandoBienvenida = false;  // Reiniciar para que se muestre de nuevo al encender

      }

  } else {
    //Activar Pantalla y mantenerla siempre encendida
    lcd_On = true;
  }

  //Logica control del sistema segun la temperatura
  controlTemperatura();

  // Lógica del 74HC595 y LED de emergencia
  controlIluminacion();

  // Ejecutar lectura y lógica cada segundo sin bloquear
  if (tiempoActual - tiempoAnterior >= intervaloLectura) {
    tiempoAnterior = tiempoActual;

    //Fórmula: velocidad = pulsos * factor (depende del sensor)
    velocidadViento = pulsos * factor_conversion; 
    pulsos = 0;

    //Control de que se muestra en cada momento por la pantalla LCD
    controlLCD();

    //Almacenar Tem`peratura Anterior para mostrar en LCD
    if (temperatura != temperaturaAnterior) {
      temperaturaAnterior = temperatura;
    }

    // Control Bateria
    controlBateria();
    
    // Mostrar en monitor serie
    Serial.println("Actividad1: Sistema de monitorización de clima");
    Serial.print("Bateria: "); Serial.print(PorcentajeBateria); Serial.print("% ("); Serial.print(bateria); Serial.println(" mA)");
    Serial.print("Valocidad de Gasto: "); Serial.print(VelocidadGasto); Serial.println(" mA/s");
    Serial.print("Valocidad de Carga: "); Serial.print(VelocidadCarga); Serial.println(" mA/s");
    if (temperatura > limiteInferior && temperatura < limiteSuperior) {
      Serial.print("Temperatura Bateria Estable: "); Serial.print(temperatura); Serial.println(" °C");
    }
    Serial.print("Temperatura Exterior: "); Serial.print(lecturaTemp);
    Serial.print(" °C, Hum: "); Serial.print(humedad);
    Serial.print(" %, Luz: "); Serial.print(luzPorcentaje); Serial.println(" %");
    Serial.print("Pulsos: "); Serial.println(pulsos);
    Serial.print("Velocidad del viento: "); Serial.print(velocidadViento); Serial.print(" km/h");
    Serial.print(" Dirección del Viento: "); Serial.println(direccionViento);

        // --- Lógica de control de Temperatura y carga de batería ---

    Serial.print("Carga: "); Serial.println(carga_On ? "Activa" : "Bloqueada");
    Serial.print("Pantalla LCD: "); Serial.println(lcd_On ? "Encendida" : "Apagada");
    Serial.println("");

    VelocidadGasto = 0;
    

  }  //delay(3000); No es una buena Practica.
}


//-----------------Funciones de Interrupción------------------------------------------------
void contarPulsos() {
  unsigned long tiempoAhora = millis();
  if (tiempoAhora - ultimaInterrupcion > reboteMinimo) {
    pulsos++;
    ultimaInterrupcion = tiempoAhora;
  }
}


//------------------Funciones Auxiliares-------------------------------------------------------
void actualizarLEDs(byte estado) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DS_PIN, CLOCK_PIN, MSBFIRST, estado);
  digitalWrite(LATCH_PIN, HIGH);
}
