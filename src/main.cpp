#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Ticker.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiManager wifiManager;
Ticker ticker;

WiFiClientSecure clientSecure;

String hora = "00:00";  
String fecha = "01.JAN";  // Variable para la fecha inicial en formato DD-MMM
String tempZaragoza = "00"; 
int precioEthereumEUR = 0;  
bool puntosEncendidos = true;  
unsigned long ultimoTiempoSync = 0;  
unsigned long ultimoTiempoParpadeo = 0;  
const unsigned long intervaloSync = 10000;  
const unsigned long intervaloParpadeo = 1000;  

String sincronizarHora();
String obtenerTemperaturaZaragoza();
int obtenerPrecioEthereumEUR();

const char* meses[] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};

void tick() {
  int state = digitalRead(LED_BUILTIN);  
  digitalWrite(LED_BUILTIN, !state);     
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  ticker.attach(0.6, tick);

  Serial.begin(115200);
  Wire.begin(14, 12);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Fallo al iniciar la pantalla OLED"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 0);
  display.println(F("Conectando WiFi..."));
  display.display();

  wifiManager.autoConnect("ESP8266_OLED");
  ticker.detach();
  digitalWrite(LED_BUILTIN, HIGH);

  display.clearDisplay();
  display.setCursor(3, 0);
  display.println(F("WiFi Conectado!"));
  display.display();
  delay(2000);

  clientSecure.setInsecure();

  // Sincronizar hora, fecha, temperatura y precio de Ethereum
  hora = sincronizarHora();
  tempZaragoza = obtenerTemperaturaZaragoza();
  precioEthereumEUR = obtenerPrecioEthereumEUR();
}

String sincronizarHora() {
  HTTPClient http;
  http.begin(clientSecure, "https://worldtimeapi.org/api/timezone/Europe/Madrid");
  int httpCode = http.GET();
  String nuevaHora = hora;
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Respuesta de WorldTimeAPI: " + payload);
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      nuevaHora = doc["datetime"].as<String>().substring(11, 16);  // Hora en formato "HH:MM"
      String day = doc["datetime"].as<String>().substring(8, 10);
      int month = doc["datetime"].as<String>().substring(5, 7).toInt();
      fecha = day + "." + String(meses[month - 1]);  // Formato "DD-MMM"
      Serial.println("Hora sincronizada: " + nuevaHora);
      Serial.println("Fecha sincronizada: " + fecha);
    } else {
      Serial.println("Error al parsear JSON de WorldTimeAPI");
    }
  } else {
    Serial.println("Error al obtener la hora.");
  }
  http.end();
  return nuevaHora;
}

String obtenerTemperaturaZaragoza() {
  HTTPClient http;
  http.begin(clientSecure, "https://wttr.in/Zaragoza?format=%t");
  int httpCode = http.GET();
  String temperatura = "N/A";
  if (httpCode == HTTP_CODE_OK) {
    temperatura = http.getString();
    temperatura.replace("°C", "");  // Elimina el símbolo de grado y 'C'
    Serial.println("Temperatura obtenida: " + temperatura);
  } else {
    Serial.println("Error al obtener la temperatura.");
  }
  http.end();
  return temperatura;
}

int obtenerPrecioEthereumEUR() {
  HTTPClient http;
  http.begin(clientSecure, "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=eur");
  int httpCode = http.GET();
  int precio = precioEthereumEUR;
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Respuesta de CoinGecko: " + payload);
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      precio = doc["ethereum"]["eur"].as<float>();
      Serial.print("Precio de Ethereum obtenido: ");
      Serial.println(precio);
    } else {
      Serial.println("Error al parsear JSON de CoinGecko");
    }
  } else {
    Serial.println("Error al obtener el precio de Ethereum.");
  }
  http.end();
  return static_cast<int>(precio);
}

void loop() {
  if (millis() - ultimoTiempoSync >= intervaloSync) {
    hora = sincronizarHora();
    tempZaragoza = obtenerTemperaturaZaragoza();
    precioEthereumEUR = obtenerPrecioEthereumEUR();
    ultimoTiempoSync = millis();
  }

  if (millis() - ultimoTiempoParpadeo >= intervaloParpadeo) {
    puntosEncendidos = !puntosEncendidos;
    ultimoTiempoParpadeo = millis();
  }

  display.clearDisplay();
  
  // Muestra el precio de Ethereum en la parte superior
  display.setTextSize(2);
  display.setCursor(20, 0);  // Ajuste para que el precio esté más hacia la izquierda
  display.print(precioEthereumEUR);
  display.setCursor(70, 0);
  display.setTextSize(1);
  display.print("ETH/EUR");

  // Muestra la fecha en el formato "DD-MMM" en la posición (3, 19)
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(fecha);

  // Muestra la temperatura en la esquina superior derecha, un poco más abajo
  display.setTextSize(2);
  display.setCursor(90, 20);  // Posición ajustada hacia abajo
  display.print(tempZaragoza);

  // Muestra la hora en la parte inferior izquierda en tamaño 3
  display.setTextSize(3);
  display.setCursor(0, 43);
  if (puntosEncendidos) {
    display.print(hora);
  } else {
    display.print(hora.substring(0, 2) + " " + hora.substring(3, 5));
  }

  display.display();
  delay(100);
}
