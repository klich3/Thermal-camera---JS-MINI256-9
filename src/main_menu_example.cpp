/**
 * Ejemplo de uso del ThermalCameraController con sistema de menús
 * 
 * Este archivo reemplaza a main.cpp para demostrar el uso
 * del sistema de menús interactivo.
 * 
 * Para compilar: pio run -e menu_example
 */

#include <Arduino.h>
#include "CameraController.h"
#include "MenuSystem.h"

// Configuración de pines
#define RX_PIN 16
#define TX_PIN 17
#define UART_BAUDRATE 115200

// Crear instancias
HardwareSerial cameraSerial(2);
CameraController camera(&cameraSerial, RX_PIN, TX_PIN);
MenuSystem menu(&camera);

String inputBuffer = "";

// Callback para respuestas de la cámara
void handleCameraResponse(const String& interpretation) {
    Serial.println("📡 " + interpretation);
}

void setup() {
    // Inicializar Serial
    Serial.begin(UART_BAUDRATE);
    while (!Serial) {
        delay(10);
    }
    
    delay(1000);
    Serial.println("=== Thermal Camera Controller - Menu Interface ===");
    
    // Debug de configuración
    Serial.println("=== Configuration Debug ===");
    Serial.println("RX Pin: " + String(RX_PIN));
    Serial.println("TX Pin: " + String(TX_PIN));
    Serial.println("Baudrate: " + String(UART_BAUDRATE));
    Serial.println("============================");
    
    // Configurar cámara
    camera.enableDebug(true);
    camera.setTimeouts(150, 75);
    CameraController::setGlobalResponseHandler(handleCameraResponse);
    
    // Inicializar comunicación con la cámara
    if (!camera.begin()) {
        Serial.println("❌ Error al inicializar la cámara");
        Serial.println("Error: " + camera.getLastError());
        
        Serial.println("\n=== Información de diagnóstico ===");
        Serial.println("Por favor, verifica las conexiones y reinicia");
        
        while (true) {
            delay(500);
        }
    }
    
    // Inicializar sistema de menú
    menu.begin();
    
    Serial.println("✅ Sistema listo! Usa los comandos o navega por los menús.");
}

void loop() {
    // Procesar entrada del usuario
    if (Serial.available()) {
        char receivedChar = Serial.read();
        
        if (receivedChar == '\n') {
            if (inputBuffer.length() > 0) {
                menu.processInput(inputBuffer);
            }
            inputBuffer = "";
        } else if (receivedChar != '\r') {
            inputBuffer += receivedChar;
        }
    }
    
    // Actualizar procesamiento de respuestas asíncronas
    camera.update();
    
    // Actualizar sistema de menú
    menu.update();
}
