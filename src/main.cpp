#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL6180X.h>
#include <SPI.h>
#include "HX711.h"
#include <WiFi.h>          // Incluye la biblioteca WiFi para el ESP32
#include <HTTPClient.h>    //Include de la librería HTTPClient para hacer las llamadas a GOOGLE


// Definir pin del botón
const int botonPin = 12;

// HX711 cableado del sensor
const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN = 5;

// Crear objeto del sensor de distancia
Adafruit_VL6180X sensor = Adafruit_VL6180X();

// Crear objeto del sensor celula de carga
HX711 balanza;

// Configura los parámetros de la red Wi-Fi
const char* ssid = "poxtius";         // Reemplaza con el nombre de tu red Wi-Fi
const char* password = "Zgrrmrtnz1976"; // Reemplaza con la contraseña de tu red Wi-Fi

// Google script ID and required credentials
String GOOGLE_SCRIPT_ID = "AKfycbyI4j8uDDer_WD5in-fPzWIBh0NjGXoSpp9xGlXGE4t1ln6mjd--M-dQJrLrJ47A8TlBQ";

// Variables de medición
int LED = 2;
int distanciaReferencia = -1;   // Distancia de referencia (inicialmente no definida)
int distanciaActual = 0;        // Distancia medida actualmente
int ultimaDistanciaTomada = 0;
bool referenciaTomada = false;  // Bandera para saber si ya se tomó la referencia
bool inicioPrueba = false;

//Hace la media de lecturas del sensor para evitar que nos de falsas lecturas.
int leerMediaSensor() {
    int suma = 0;
    const int numLecturas = 10;

    for (int i = 0; i < numLecturas; i++) {
        int lectura = sensor.readRange();  // Lee el valor del sensor
        suma += lectura;
        delay(10);  // Pequeño retraso entre lecturas, ajusta según necesites
    }

    return suma / numLecturas;  // Calcula la media
}

//Función para automatizar el led y decidir que significa
void encender_led(int veces, int tiempo){
  for(int i = 0; i< veces; i++){
  digitalWrite(LED, HIGH);
  delay(tiempo);
  digitalWrite(LED, LOW);
  delay(tiempo);
  }
  
}

//Función para que nos de el peso cuando haya un cambio de distancia.
long pesar ()
{
 if (balanza.is_ready()) {
    long reading = balanza.get_units(10);
    //Serial.print("HX711 reading: ");
    //Serial.println(reading);
    return (reading);
  } else {
    //Serial.println("HX711 not found.");
    return(-1);
  }
}

//Funcion que manda los datos a la hoja de Excell
void mandar_datos(float dist, float peso){

	String distString = String(dist, 2);  //Convertimos las variable float a String
	String pesoString = String(peso, 2);	//Convertimos las variable float a String
	/*Generamos la url que necesitamos para mandar los datos*/
	String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + "dist=" + distString + "&fuerza=" + pesoString;
	/*Imprimimos esa url para ver si es la que necesitamos*/
	//Serial.println(urlFinal);
	/*Creamos el objeto HTTPClient que será donde metamos la url para mandar*/
	HTTPClient http;
	http.begin(urlFinal.c_str());
	http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
	int httpCode = http.GET(); 
	//Serial.print("HTTP Status Code: ");
  //Serial.println(httpCode);
  if (httpCode == 200){
  }
    //---------------------------------------------------------------------
    //getting response from google sheet
    String payload;
    if (httpCode > 0) {
        payload = http.getString();
        //Serial.println("Payload: "+payload);
	}
	/*Acabamos el mensaje*/
	http.end(); 
  encender_led(1, 500);

}

//Función de configuración del hardware y software del programa
void setup() {
  Serial.begin(115200);
  pinMode(botonPin, INPUT_PULLUP);  // Configurar el pin del botón como entrada con pull-up interno
  pinMode(LED, OUTPUT);

  // Iniciar sensor de distancia
  if (!sensor.begin()) {
    Serial.println("Error al iniciar el sensor VL6180X. Verifica la conexión.");
    while (1);
  } else {
    Serial.println("Sensor VL6180X iniciado correctamente.");
  }
  //Iniciar la balanza la ajustas y la taras
  balanza.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  delay(250);
  balanza.set_scale(-1050);
  balanza.tare(10);

  //Iniciamos la wifi para que el ESP32 se conecte

  WiFi.begin(ssid, password); // Inicia la conexión a la red Wi-Fi

  // Espera a que se realice la conexión
  while (WiFi.status() != WL_CONNECTED) {
   	delay(500);
   	Serial.print(".");
  }

  encender_led(5, 500); //Encender el LED 5 veces significa que el programa ha iniciado correctamente
}

//Bloque principal del programa
void loop() {
  // Verificar si el botón ha sido pulsado
  if (digitalRead(botonPin) == LOW) { // LOW significa que el botón está presionado
    if (!referenciaTomada && !inicioPrueba) {  // Si aún no se ha tomado la referencia
      distanciaReferencia = leerMediaSensor(); // Leer y almacenar la distancia como referencia
      ultimaDistanciaTomada = distanciaReferencia;
      //Serial.print("Distancia de referencia tomada: ");
      //Serial.print(distanciaReferencia);
      //Serial.println(" mm");
      referenciaTomada = true; // Activar la bandera de referencia tomada
      encender_led(4,750); // Encender el LED durante 4 veces significa que empieza la prueba
    }
    else
    {
      inicioPrueba = false;
      referenciaTomada = false;
      //Serial.println("Prueba acabada");
      encender_led(2, 1000); //Encender el LED durante 2 veces significa que la prueba acaba.
    }
  }

  // Si la referencia ya fue tomada, monitorizar la distancia
  if (referenciaTomada) {
    distanciaActual = leerMediaSensor(); // Leer la distancia actual
    if (distanciaActual >= ultimaDistanciaTomada + 1) { // Si la distancia ha aumentado en al menos 1 mm
      int diferencia = distanciaActual - distanciaReferencia; // Calcular la diferencia
      //Serial.print("Diferencia: ");
      //Serial.print(diferencia);
      //Serial.println(" mm");
      long peso = pesar();
      //Serial.print("Peso: ");
      //Serial.println(peso);
      mandar_datos(diferencia, peso);
      // Actualizar la referencia para detectar el próximo incremento
      ultimaDistanciaTomada = distanciaActual; 
    }
  }
  
  delay(100); // Tiempo de espera para evitar lecturas muy rápidas
}
