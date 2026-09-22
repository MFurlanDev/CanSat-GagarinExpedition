#include <SPI.h>                 // Comunicación SPI
#include <LoRa.h>                // Librería LoRa
#include <Wire.h>                // Comunicación I2C
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>     // Sensor BMP280
#include <TinyGPSPlus.h>         // GPS

Adafruit_BMP280 bmp;
TinyGPSPlus gps;

// =========================
// GPS
// Cableado:
// GPS TX -> IO4
// GPS RX -> IO15
// GPS VCC -> 3.3V
// GPS GND -> GND
// =========================
static const int RXPin = 4;
static const int TXPin = 15;
static const uint32_t GPSBaud = 9600;
HardwareSerial ss(1);

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Estados
#define OFF 0
#define ON 1

// Pines SPI para LoRa
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 23
#define LORA_IRQ 26

// Sensores MQ
#define mq135 35
#define mq9 34

// Otros pines
const int ledVerde = 2;
const byte pinBuzzer = 12;

// Buzzer
byte salida = 10;
int Buzzeractivo = 0;
int altitudBuzzer = 650;

// Configuración LoRa
const String satelite = "GagarinExpedition";
const long lorahz = 8651E5;

// Prototipos
void escribirenpantalla(int line, String metrica, String valor, String unidad, String command);
static String formatGPSfloat(float value, int decimals);
static String formatGPSDateTime(TinyGPSDate &d, TinyGPSTime &t);
void enviarlora(String satelite, float t, float a, float p, String calidad, String metano, String gpsn, String lat, String lng, String gpsa, String gpsv, String timestamp);
static void smartDelay(unsigned long ms);
void leerGPSRapido();

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("====================================");
  Serial.println("INICIO CANSAT EMISOR LORA + GPS");
  Serial.println("====================================");

  // GPS
  ss.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);
  Serial.println("GPS iniciado a 9600 baudios");

  // I2C explícito para ESP32
  Wire.begin(21, 22);

  // Sensores MQ
  pinMode(mq135, INPUT);
  pinMode(mq9, INPUT);

  // LED y buzzer
  pinMode(ledVerde, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  digitalWrite(ledVerde, OFF);
  analogWrite(pinBuzzer, 0);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while (1);
  }

  display.clearDisplay();
  display.display();

  escribirenpantalla(0, "Sistema", "Iniciando", "", "ClearDisplay");

  // BMP280
  bool status = bmp.begin(0x77);
  if (!status) {
    Serial.println("BMP280 no detectado en 0x77, probando 0x76...");
    status = bmp.begin(0x76);
  }

  if (!status) {
    Serial.println("BMP280 no detectado");
    escribirenpantalla(10, "ERR", "BMP280 KO", "", "DisplayOnly");
  } else {
    Serial.println("BMP280 detectado correctamente");
    escribirenpantalla(10, "BMP280", "OK", "", "DisplayOnly");
  }

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  if (!LoRa.begin(lorahz)) {
    Serial.println("Fallo en inicio de LoRa");
    escribirenpantalla(20, "ERR", "LoRa KO", "", "DisplayOnly");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  Serial.println("LoRa iniciada correctamente");
  escribirenpantalla(20, "LoRa", "OK", "", "DisplayOnly");

  Serial.println("Sistema iniciado");
  Serial.println("Pon el GPS al exterior con cielo abierto");
  Serial.println();
}

void loop() {
  float temperatura = NAN;
  float presion = NAN;
  float altitud = NAN;

  String gas_value_MQ135 = "";
  String gas_value_MQ9 = "";
  String gpsn = "";
  String lat = "";
  String lng = "";
  String gpsa = "";
  String gpsv = "";
  String timestamp = "";

  // Leer GPS constantemente al principio del loop
  leerGPSRapido();

  // BMP280
  temperatura = bmp.readTemperature();
  altitud = bmp.readAltitude(1013.25);
  presion = bmp.readPressure();

  // Buzzer por altitud
  if (!isnan(altitud) && altitud > altitudBuzzer) {
    Buzzeractivo = 1;
  }

  if (!isnan(altitud) && altitud < altitudBuzzer && Buzzeractivo == 1) {
    analogWrite(pinBuzzer, salida);
  } else {
    analogWrite(pinBuzzer, 0);
  }

  // MQ135 y MQ9
  gas_value_MQ135 = String(analogRead(mq135));
  gas_value_MQ9 = String(analogRead(mq9));

  // Seguir alimentando el parser GPS
  smartDelay(1000);

  if (gps.satellites.isValid()) {
    gpsn = String(gps.satellites.value());
  }

  if (gps.location.isValid()) {
    lat = formatGPSfloat(gps.location.lat(), 6);
    lng = formatGPSfloat(gps.location.lng(), 6);
  }

  if (gps.altitude.isValid()) {
    gpsa = formatGPSfloat(gps.altitude.meters(), 3);
  }

  if (gps.speed.isValid()) {
    gpsv = formatGPSfloat(gps.speed.kmph(), 3);
  }

  timestamp = formatGPSDateTime(gps.date, gps.time);

  // =========================
  // OLED
  // =========================
  display.clearDisplay();

  escribirenpantalla(0, "Temp", String(temperatura, 2), " C", "None");
  escribirenpantalla(10, "Alt", String(altitud, 2), " m", "None");
  escribirenpantalla(20, "Pres", String(presion, 0), " Pa", "None");

  if (gps.location.isValid()) {
    escribirenpantalla(30, "Lat", lat, "", "None");
    escribirenpantalla(40, "Lng", lng, "", "None");
  } else if (gps.satellites.isValid()) {
    escribirenpantalla(30, "GPS", gpsn, " sats", "None");
    escribirenpantalla(40, "GPS", "SIN FIX", "", "None");
  } else {
    escribirenpantalla(30, "GPS", "BUSCANDO", "", "None");
    escribirenpantalla(40, "Chars", String(gps.charsProcessed()), "", "None");
  }

  display.display();

  // =========================
  // Debug serie
  // =========================
  Serial.print(satelite);
  Serial.print(",");
  Serial.print(temperatura);
  Serial.print(",");
  Serial.print(altitud);
  Serial.print(",");
  Serial.print(presion);
  Serial.print(",");
  Serial.print(gas_value_MQ135);
  Serial.print(",");
  Serial.print(gas_value_MQ9);
  Serial.print(",");
  Serial.print(gpsn);
  Serial.print(",");
  Serial.print(lat);
  Serial.print(",");
  Serial.print(lng);
  Serial.print(",");
  Serial.print(gpsa);
  Serial.print(",");
  Serial.print(gpsv);
  Serial.print(",");
  Serial.println(timestamp);

  Serial.print("Chars GPS: ");
  Serial.println(gps.charsProcessed());

  if (gps.location.isValid()) {
    Serial.println("ESTADO GPS: FIX OK");
  } else if (gps.satellites.isValid() && gps.satellites.value() > 0) {
    Serial.print("ESTADO GPS: viendo satelites -> ");
    Serial.println(gps.satellites.value());
  } else {
    Serial.println("ESTADO GPS: sin fix / sin satelites");
  }

  Serial.println("--------------------------------------------------");

  // Envío LoRa
  enviarlora(satelite, temperatura, altitud, presion, gas_value_MQ135, gas_value_MQ9, gpsn, lat, lng, gpsa, gpsv, timestamp);

  // Seguir leyendo GPS mientras espera
  smartDelay(1000);
}

// Esta versión de delay mantiene alimentado el parser del GPS
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (ss.available()) {
      gps.encode(ss.read());
    }
  } while (millis() - start < ms);
}

void leerGPSRapido() {
  while (ss.available()) {
    gps.encode(ss.read());
  }
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

void enviarlora(String satelite, float t, float a, float p, String calidad, String metano, String gpsn, String lat, String lng, String gpsa, String gpsv, String timestamp) {
  digitalWrite(ledVerde, ON);

  LoRa.beginPacket();
  LoRa.print(satelite);
  LoRa.print(",");
  LoRa.print(t);
  LoRa.print(",");
  LoRa.print(a);
  LoRa.print(",");
  LoRa.print(p);
  LoRa.print(",");
  LoRa.print(calidad);
  LoRa.print(",");
  LoRa.print(metano);
  LoRa.print(",");
  LoRa.print(gpsn);
  LoRa.print(",");
  LoRa.print(lat);
  LoRa.print(",");
  LoRa.print(lng);
  LoRa.print(",");
  LoRa.print(gpsa);
  LoRa.print(",");
  LoRa.print(gpsv);
  LoRa.print(",");
  LoRa.print(timestamp);
  LoRa.endPacket();

  digitalWrite(ledVerde, OFF);
}

static String formatGPSfloat(float value, int decimals) {
  char buffer[20];
  dtostrf(value, 1, decimals, buffer);
  return String(buffer);
}

static String formatGPSDateTime(TinyGPSDate &d, TinyGPSTime &t) {
  if (d.isValid() && t.isValid()) {
    char sz[24];
    sprintf(sz, "%04d/%02d/%02d %02d:%02d:%02d",
            d.year(), d.month(), d.day(),
            t.hour(), t.minute(), t.second());
    return String(sz);
  } else {
    return "";
  }
}