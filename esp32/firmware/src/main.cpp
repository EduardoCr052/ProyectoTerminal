#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Ticker.h>
#include "EmonLib.h"

// ================= Configuración de Sensor y Variables Globales =================
EnergyMonitor emon1;
const int pinSCT = 34; // Pin ADC1
const float voltajeNominal = 127.0; 
const float constanteCalibracion = 60.6; 

// Variables globales para almacenar la lectura más reciente
double global_Irms = 0.0;
double global_potencia = 0.0;

// ================= Configuración de Red y MQTT =================
Ticker timer;   

const char *ssid = "Conectando...";                      
const char *password = "f5c57d4e24";              
const char* device_name = "Recamara1"; 

byte cont = 0;
byte max_intentos = 50;
int statusLedPin = 2;   
int errorLedPin = 21;   

unsigned int msgTime = 5000; // Tiempo de espera entre mensajes MQTT

const char *mqtt_broker = "s6a1ab0a.ala.us-east-1.emqxsl.com";     
const char *mqtt_topic = "Casa/Recamara";  
const char *mqtt_username = device_name;
const char *mqtt_password = "12345678"; 
const int mqtt_port = 8883;

WiFiClientSecure esp_client;
PubSubClient mqtt_client(esp_client);

const char *ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";

// ================= Funciones de Red =================
void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Conectando a: ");
    Serial.print(ssid);
    Serial.print(" ");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED and cont < max_intentos) {
        cont++;
        digitalWrite(statusLedPin, HIGH);
        delay(500);
        digitalWrite(statusLedPin, LOW);
        Serial.print("."); 
        delay(100); 
    }

    randomSeed(micros());

    if (cont < max_intentos) {
        Serial.println("\n*************************");
        digitalWrite(statusLedPin, HIGH);
        digitalWrite(errorLedPin, LOW);
        Serial.print("Conectado a la red WiFi: ");
        Serial.println(WiFi.SSID());
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        Serial.println("*************************");
    } else {
        Serial.println("\n-------------------------------");
        Serial.println("Error de conexión");
        digitalWrite(statusLedPin, LOW);
        digitalWrite(errorLedPin, HIGH);
        Serial.println("-------------------------------");
    }
}

void connectToMQTT() {
    while (!mqtt_client.connected()) {
        String clientId = String(device_name);
        clientId += "_" + String(random(0xffff), HEX);
        Serial.println("Conectando al broker MQTT como " + String(clientId.c_str()));
        
        if (mqtt_client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Broker MQTT conectado");
            mqtt_client.subscribe(mqtt_topic);
        } else {
            Serial.print("Error al conectar al broker MQTT. Estado: ");
            Serial.print(mqtt_client.state());
            Serial.println(". Intentando de nuevo en 5 segundos...");
            delay(5000);
        }
    }
}

// ================= Función de Envío de Datos =================
void enviarJSON(){
    // Formatea el JSON utilizando los valores reales del sensor
    String json = "{\"id\":\""+ String(device_name) + "\",\"Irms\":"+ String(global_Irms, 3) + ",\"VA\":"+String(global_potencia, 2)+"}";
    
    int str_len = json.length() + 1;            
    char char_array[str_len];
    json.toCharArray(char_array, str_len);      
    mqtt_client.publish(mqtt_topic, char_array);
}

// ================= Setup =================
void setup() {
    Serial.begin(115200);
    pinMode(statusLedPin, OUTPUT);
    pinMode(errorLedPin, OUTPUT);
    
    // Configuración del sensor
    analogReadResolution(10); 
    emon1.current(pinSCT, 22.46);

    // Configuración de red
    setup_wifi();                                   
    esp_client.setCACert(ca_cert);                  
    mqtt_client.setServer(mqtt_broker, mqtt_port);  
    mqtt_client.setKeepAlive(60);                   
    
    // Conexión inicial MQTT y Timer
    if (WiFi.status() == WL_CONNECTED) {
        connectToMQTT();                                
    }
    timer.attach_ms(msgTime, enviarJSON);           
}

// ================= Loop Principal =================
void loop() {
    // 1. Mantener conexión MQTT
    if(WiFi.status() == WL_CONNECTED) {
        if(!mqtt_client.connected()){
            connectToMQTT();
        } else {
            mqtt_client.loop();
        }
    }

    // 2. Muestreo de la señal AC
    double Irms = emon1.calcIrms(1480);

    // 3. Filtro de Ruido Blanco
    if (Irms < 0.10) { 
        Irms = 0.0;
    }

    // 4. Actualizar variables globales (el Ticker las leerá de aquí)
    global_Irms = Irms;
    global_potencia = Irms * voltajeNominal;

    // 5. Monitor Serial
    Serial.print("Corriente RMS: ");
    Serial.print(global_Irms, 3);
    Serial.print(" A  |  Potencia Aparente: ");
    Serial.print(global_potencia, 2);
    Serial.println(" VA");

    delay(2000);
}