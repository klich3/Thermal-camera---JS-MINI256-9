/**
 * Ejemplo básico de uso del ThermalCameraController
 * 
 * Este ejemplo muestra cómo usar las funciones básicas del controlador
 * de cámara térmica sin el sistema de menús.
 * 
 * Conexiones:
 * - ESP32 GPIO16 -> Camera RX
 * - ESP32 GPIO17 -> Camera TX
 * - Camera Power: 5V-16V
 * - Camera GND -> ESP32 GND
 */

#include <Arduino.h>
#include <CameraController.h>

// Configuración de pines
#define RX_PIN 16
#define TX_PIN 17
#define UART_BAUDRATE 115200

// Crear instancias
HardwareSerial cameraSerial(2);
CameraController camera(&cameraSerial, RX_PIN, TX_PIN);

// Callback para respuestas de la cámara
void handleCameraResponse(const String& interpretation) {
    Serial.println("📡 Camera Response: " + interpretation);
}

void setup() {
    // Inicializar Serial
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== Thermal Camera Controller - Basic Usage ===");
    
    // Configurar cámara
    camera.enableDebug(true);
    camera.setTimeouts(150, 75);
    CameraController::setGlobalResponseHandler(handleCameraResponse);
    
    // Inicializar comunicación con la cámara
    if (!camera.begin()) {
        Serial.println("❌ Error al inicializar la cámara");
        Serial.println("Error: " + camera.getLastError());
        while (true) delay(1000);
    }
    
    Serial.println("✅ Cámara inicializada correctamente");
    
    // Pruebas básicas
    testBasicFunctions();
}

void loop() {
    // Procesar respuestas asíncronas
    camera.update();
    
    // Esperar 5 segundos y repetir algunas pruebas
    static unsigned long lastTest = 0;
    if (millis() - lastTest > 5000) {
        testImageSettings();
        lastTest = millis();
    }
}

void testBasicFunctions() {
    Serial.println("\n=== Probando funciones básicas ===");
    
    // Leer información del dispositivo
    String model = camera.getModel();
    if (model.length() > 0) {
        Serial.println("Modelo: " + model);
    }
    
    // Leer estado
    CameraStatus status = camera.getStatus();
    Serial.println("Estado de la cámara: " + String(status));
    
    // Configurar brillo
    if (camera.setBrightness(75)) {
        Serial.println("✅ Brillo configurado a 75");
    } else {
        Serial.println("❌ Error configurando brillo: " + camera.getLastError());
    }
    
    // Configurar contraste
    if (camera.setContrast(60)) {
        Serial.println("✅ Contraste configurado a 60");
    } else {
        Serial.println("❌ Error configurando contraste: " + camera.getLastError());
    }
    
    // Cambiar paleta de colores
    if (camera.setPalette(PALETTE_RAINBOW)) {
        Serial.println("✅ Paleta cambiada a Rainbow");
    } else {
        Serial.println("❌ Error cambiando paleta: " + camera.getLastError());
    }
}

void testImageSettings() {
    Serial.println("\n=== Leyendo configuración actual ===");
    
    uint8_t brightness = camera.getBrightness();
    uint8_t contrast = camera.getContrast();
    ColorPalette palette = camera.getCurrentPalette();
    
    Serial.println("Brillo actual: " + String(brightness));
    Serial.println("Contraste actual: " + String(contrast));
    Serial.println("Paleta actual: " + String(palette));
}
