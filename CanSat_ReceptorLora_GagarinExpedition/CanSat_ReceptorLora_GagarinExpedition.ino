#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Estados
#define OFF 0
#define ON 1

// Pines SPI LoRa
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 23
#define LORA_IRQ 26

// Otros pines
const int ledVerde = 2;
const byte pinBuzzer = 12;

// Buzzer
byte salida = 10;
int Buzzeractivo = 0;
int altitudBuzzer = 650;

// LoRa
const long lorahz = 8651E5;

// Variables recibidas
String satelite = "";
float temperatura = NAN;
float altitud = NAN;
float presion = NAN;
String gas_value_MQ135 = "";
String gas_value_MQ9 = "";
String gpsn = "";
String lat = "";
String lng = "";
String gpsa = "";
String gpsv = "";
String timestamp = "";

unsigned long ultimoPaquete = 0;

// ---------------------------
// Prototipos
// ---------------------------
void escribirenpantalla(int line, String metrica, String valor, String unidad, String command);
bool parseCSV(String data);
String getField(String data, int index);
void mostrarDatos();
void mostrarSinSenal();

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(ledVerde, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  digitalWrite(ledVerde, OFF);
  analogWrite(pinBuzzer, 0);

  // I2C OLED
  Wire.begin(21, 22);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    while (1);
  }

  display.clearDisplay();
  display.display();

  escribirenpantalla(0, "Sistema", "Iniciando", "", "ClearDisplay");

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  if (!LoRa.begin(lorahz)) {
    escribirenpantalla(10, "ERR", "LoRa KO", "", "DisplayOnly");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  escribirenpantalla(10, "LoRa", "OK", "", "DisplayOnly");
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    digitalWrite(ledVerde, ON);

    String recibido = "";
    while (LoRa.available()) {
      recibido += (char)LoRa.read();
    }

    ultimoPaquete = millis();

    // SOLO salida CSV RAW
    Serial.println(recibido);

    bool ok = parseCSV(recibido);

    if (ok) {
      mostrarDatos();
    } else {
      display.clearDisplay();
      escribirenpantalla(0, "ERR", "Trama invalida", "", "None");
      escribirenpantalla(12, "RAW", recibido.substring(0, 18), "", "None");
      display.display();
    }

    digitalWrite(ledVerde, OFF);
  }

  // Buzzer según altitud recibida del emisor
  if (!isnan(altitud) && altitud > altitudBuzzer) {
    Buzzeractivo = 1;
  }

  if (!isnan(altitud) && altitud < altitudBuzzer && Buzzeractivo == 1) {
    analogWrite(pinBuzzer, salida);
  } else {
    analogWrite(pinBuzzer, 0);
  }

  // Si pasan más de 5 s sin paquete, avisar
  if (millis() - ultimoPaquete > 5000) {
    mostrarSinSenal();
  }

  delay(50);
}

bool parseCSV(String data) {
  // Esperamos 12 campos:
  // 0 satelite
  // 1 temperatura
  // 2 altitud
  // 3 presion
  // 4 mq135
  // 5 mq9
  // 6 gpsn
  // 7 lat
  // 8 lng
  // 9 gpsa
  // 10 gpsv
  // 11 timestamp

  String f0  = getField(data, 0);
  String f1  = getField(data, 1);
  String f2  = getField(data, 2);
  String f3  = getField(data, 3);
  String f4  = getField(data, 4);
  String f5  = getField(data, 5);
  String f6  = getField(data, 6);
  String f7  = getField(data, 7);
  String f8  = getField(data, 8);
  String f9  = getField(data, 9);
  String f10 = getField(data, 10);
  String f11 = getField(data, 11);

  if (f0 == "" && f1 == "" && f2 == "" && f3 == "") {
    return false;
  }

  satelite = f0;
  temperatura = (f1.length() > 0) ? f1.toFloat() : NAN;
  altitud     = (f2.length() > 0) ? f2.toFloat() : NAN;
  presion     = (f3.length() > 0) ? f3.toFloat() : NAN;
  gas_value_MQ135 = f4;
  gas_value_MQ9   = f5;
  gpsn = f6;
  lat  = f7;
  lng  = f8;
  gpsa = f9;
  gpsv = f10;
  timestamp = f11;

  return true;
}

String getField(String data, int index) {
  int start = 0;
  int end = -1;
  int currentIndex = 0;

  for (int i = 0; i <= data.length(); i++) {
    if (i == data.length() || data.charAt(i) == ',') {
      end = i;
      if (currentIndex == index) {
        return data.substring(start, end);
      }
      currentIndex++;
      start = i + 1;
    }
  }

  return "";
}

void mostrarDatos() {
  display.clearDisplay();

  escribirenpantalla(0,  "Sat", satelite, "", "None");
  escribirenpantalla(10, "Tmp", String(temperatura, 2), " C", "None");
  escribirenpantalla(20, "Alt", String(altitud, 2), " m", "None");
  escribirenpantalla(30, "Pre", String(presion, 0), " Pa", "None");

  if (lat.length() > 0 && lng.length() > 0) {
    escribirenpantalla(40, "Lat", lat, "", "None");
    escribirenpantalla(50, "Lng", lng, "", "DisplayOnly");
  } else if (gpsn.length() > 0) {
    escribirenpantalla(40, "GPS", gpsn, " sats", "None");
    escribirenpantalla(50, "Fix", "NO", "", "DisplayOnly");
  } else {
    escribirenpantalla(40, "MQ135", gas_value_MQ135, "", "None");
    escribirenpantalla(50, "MQ9", gas_value_MQ9, "", "DisplayOnly");
  }
}

void mostrarSinSenal() {
  static unsigned long ultimoRefresco = 0;

  if (millis() - ultimoRefresco < 1000) return;
  ultimoRefresco = millis();

  display.clearDisplay();
  escribirenpantalla(0, "LoRa", "Esperando", "", "None");
  escribirenpantalla(12, "Ultimo", String((millis() - ultimoPaquete) / 1000), " s", "None");

  if (!isnan(temperatura)) {
    escribirenpantalla(24, "Tmp", String(temperatura, 1), " C", "None");
    escribirenpantalla(36, "Alt", String(altitud, 1), " m", "None");
  }

  escribirenpantalla(48, "Estado", "Sin paquete", "", "DisplayOnly");
}

void escribirenpantalla(int line, String metrica, String valor, String unidad, String command) {
  if (command == "Clear" || command == "ClearDisplay") {
    display.clearDisplay();
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, line);
  display.println(metrica + ": " + valor + unidad);

  if (command == "DisplayOnly" || command == "ClearDisplay") {
    display.display();
  }
}