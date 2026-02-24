#include <Arduino.h>
#include "EmonLib.h"

// Crear una instancia para el sensor
EnergyMonitor emon1;

// ================= Configuración de Hardware =================
const int pinSCT = 34; // Usamos el pin 34 (ADC1), es el más estable en el ESP32
const float voltajeNominal = 127.0; // Voltaje residencial estándar
const float constanteCalibracion = 60.6; // Ajustar según tu resistencia burden

void setup() {
  // Inicializar comunicación serie a alta velocidad
  Serial.begin(115200);
  
  // Darle tiempo al monitor serie para abrir
  delay(1000);
  Serial.println("=== Iniciando Sistema de Monitoreo ===");

  // Configurar el pin y la calibración en EmonLib
  // La constante 60.6 es típica para un SCT-013-000 con R_burden de 33 ohms
  emon1.current(pinSCT, constanteCalibracion); 
}

void loop() {
  // 1. Muestreo de la señal AC
  // Tomamos 1480 muestras continuas para abarcar varios ciclos de la onda de 60Hz
  double Irms = emon1.calcIrms(1480);

  // 2. Filtro de "Ruido Blanco"
  // El ADC del ESP32 no es perfecto y puede leer 0.05A incluso sin nada conectado.
  // Si la lectura es menor a 100mA, la forzamos a 0 para limpiar los datos.
  if (Irms < 0.10) { 
    Irms = 0.0;
  }

  // 3. Cálculos de Potencia
  // Como solo tenemos sensor de corriente, calculamos la Potencia Aparente (S)
  double potenciaAparente = Irms * voltajeNominal;

  // 4. Visualización de los datos puros (Lo que enviaremos después)
  Serial.print("Corriente RMS: ");
  Serial.print(Irms, 3); // 3 decimales
  Serial.print(" A  |  ");
  
  Serial.print("Potencia Aparente: ");
  Serial.print(potenciaAparente, 2); // 2 decimales
  Serial.println(" VA");

  // Esperar 2 segundos antes de la siguiente lectura
  delay(2000);
}
