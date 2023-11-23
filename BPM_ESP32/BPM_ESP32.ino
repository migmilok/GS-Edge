#include "EspMQTTClient.h"
#include "ArduinoJson.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>


// MQTT Configuracoes
EspMQTTClient client{
  "Wokwi-GUEST", //SSID do WiFi
  "",     // Senha do wifi
  "mqtt.tago.io",  // Endereço do servidor
  "Default",       // Usuario
  "fa9f6c56-873d-42c1-b61c-d18019393c65",         // Token do device
  "esp",           // Nome do device
  1883             // Porta de comunicação
};

//Definições Display
#define SCREEN_WIDTH 128         
#define SCREEN_HEIGHT 64         
#define OLED_RESET    -1

//Configuração Display e MPU
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

float pulseFrequency;
float pulseRate;

#define NTP_SERVER     "a.ntp.br"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

#define PULSE_PER_BEAT    1           // Número de pulsos por batimento cardíaco
#define INTERRUPT_PIN     5           // Pino de interrupção
#define SAMPLING_INTERVAL 1000        // Intervalo de amostragem em milissegundos

volatile uint16_t pulse;              // Variável que será incrementada na interrupção
uint16_t count;                       // Variável para armazenar o valor atual de pulse

float heartRate;                      // Frequência cardíaca calculada a partir de count

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;  // Mutex para garantir acesso seguro a pulse

void IRAM_ATTR HeartRateInterrupt() {
  portENTER_CRITICAL_ISR(&mux);  // Entra em uma seção crítica de interrupção
  pulse++;  // Incrementa a variável pulse de maneira segura
  portEXIT_CRITICAL_ISR(&mux);   // Sai da seção crítica de interrupção
}

//Variáveis JSON
char bpm[100];

void setup()
{
  Serial.begin(9600);

  pinMode(INTERRUPT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), HeartRateInterrupt, RISING);  // Configura a interrupção no pino
  Wire.setClock(400000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("BPM: ");
  display.display();
  
  Serial.println("Conectando WiFi");
  while (!client.isWifiConnected()) {
    Serial.print('.'); 
    client.loop(); 
    delay(1000);
  }
  Serial.println("WiFi Conectado");
  Serial.println("Conectando com Servidor MQTT");
  while (!client.isMqttConnected()) {
    Serial.print('.'); 
    client.loop(); 
    delay(1000);
  }
  Serial.println("MQTT Conectado");

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

}
// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{}

void loop() {
  HeartRate();  // Chama a função principal
  printPulseRate(); 
  printLocalTime();
  delay(250);
  sendBpm();
  
}

void HeartRate() {
  static unsigned long startTime;
  if (millis() - startTime < SAMPLING_INTERVAL) return;   // Intervalo de amostragem
  startTime = millis();

  portENTER_CRITICAL(&mux);  // Entra em uma seção crítica
  count = pulse;  // Salva o valor atual de pulse e zera pulse
  pulse = 0;
  portEXIT_CRITICAL(&mux);   // Sai da seção crítica

  // Ajuste na fórmula para mapear a faixa de 0 Hz a 220 Hz para a frequência cardíaca em BPM
  heartRate = map(count, 0, 220, 0, 220);  // Mapeia a contagem para a faixa desejada


  Serial.println("Heart Rate: " + String(heartRate, 2) + " BPM");

  UpdateDisplay();
}

void UpdateDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("BPM: ");
  display.print(heartRate, 1);
  display.display();
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    display.setCursor(0, 40);
    display.println("Connection Err");
    return;
  }

  timeinfo.tm_hour -= 3;
  if (timeinfo.tm_hour < 0) {
    timeinfo.tm_hour += 24;
  }

  display.setCursor(0, 0);
  display.println(&timeinfo, "%H:%M:%S");
  display.setCursor(0, 40);
  display.println(&timeinfo, "%d/%m/%Y");

  display.display();
}

void printPulseRate() {
  display.setCursor(0, 20);
  display.print("BPM: ");
  display.print(heartRate, 1);
  display.display();
}

void sendBpm(){

  StaticJsonDocument<300> BPM;
  BPM["variable"] = "Batimento";
  BPM["value"] = heartRate;

  serializeJson(BPM, bpm);
  client.publish("info/BPM", bpm);
  delay(5000);
  
  client.loop();
}