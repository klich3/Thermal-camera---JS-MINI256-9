/*
█▀ █▄█ █▀▀ █░█ █▀▀ █░█
▄█ ░█░ █▄▄ █▀█ ██▄ ▀▄▀

Author: <Anton Sychev> (anton at sychev dot xyz)
main.cpp (c) 2025
Created:  2025-06-21 01:36:27 
Desc: Main application using modular camera controller
*/

#include <Arduino.h>
#include "CameraController.h"
#include "MenuSystem.h"

// ============================================================================
// CONFIGURACIÓN GLOBAL
// ============================================================================

#define RX_PIN 16
#define TX_PIN 17
#define UART_BAUDRATE 115200

// ============================================================================
// OBJETOS GLOBALES
// ============================================================================

HardwareSerial cameraSerial(2);
CameraController camera(&cameraSerial, RX_PIN, TX_PIN);
MenuSystem menu(&camera);

String inputBuffer = "";

// ============================================================================
// CALLBACK GLOBAL PARA RESPUESTAS
// ============================================================================

void handleCameraResponse(const String& interpretation) {
    Serial.println("📡 " + interpretation);
}

// ============================================================================
// FUNCIONES PRINCIPALES
// ============================================================================

//void testCommand(); 

void setup() {
    // Inicializar comunicación serie principal
    Serial.begin(UART_BAUDRATE);
    while (!Serial) {
        delay(10);
    }
    
    delay(1000);
    Serial.println("Starting ESP32 Camera Controller...");
    
    // Debug de configuración
    Serial.println("=== Configuration Debug ===");
    Serial.println("RX Pin: " + String(RX_PIN));
    Serial.println("TX Pin: " + String(TX_PIN));
    Serial.println("Baudrate: " + String(UART_BAUDRATE));
    Serial.println("============================");
    
    // Inicializar controlador de cámara
    camera.enableDebug(true);
    camera.setTimeouts(150, 75);  // Timeouts según especificaciones del fabricante
    
    // Configurar callback global para respuestas
    CameraController::setGlobalResponseHandler(handleCameraResponse);
    
    if (!camera.begin()) {
        Serial.println("❌ Failed to initialize camera controller");
        Serial.println("Error: " + camera.getLastError());
        
        Serial.println("\n=== Diagnostic Information ===");
        Serial.println("Please check connections and restart");
        
        while (true) {
            delay(500);
        }
    }
    
    // Inicializar sistema de menú
    menu.begin();
    
    Serial.println("✅ System ready! Type commands or use menu navigation.");

      delay(10);

    //testCommand();
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
    
    // IMPORTANTE: Actualizar procesamiento de respuestas asíncronas
    camera.update();
    
    // Actualizar sistema de menú
    menu.update();
}

/*
void testCommand() {
    Serial.println("=== Test Command: Change Palette to Rainbow ===");
    
    // Comando conocido: Cambiar paleta a Rainbow
    uint8_t data[] = {0x03}; // Código para la paleta Rainbow
    camera.testBuildAndSendCommand(CLASS_IMAGE, 0x20, FLAG_WRITE, data, sizeof(data));
}
*/