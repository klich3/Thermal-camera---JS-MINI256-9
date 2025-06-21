/*
█▀ █▄█ █▀▀ █░█ █▀▀ █░█
▄█ ░█░ █▄▄ █▀█ ██▄ ▀▄▀

Author: <Anton Sychev> (anton at sychev dot xyz)
CameraController.cpp (c) 2025
Created:  2025-06-21 01:36:27 
Desc: JS-MINI256-9 Thermal Camera Controller Implementation - FIXED VERSION
Docs: 
    CameraController camera(&Serial2, 16, 17);
    camera.begin();
    camera.setBrightness(80);
    camera.setPalette(PALETTE_RAINBOW);
*/

#include "CameraController.h"

// Constantes estáticas
const char* CameraController::PALETTE_NAMES[] = {
    "White Hot", "Black Hot", "Iron", "Rainbow", "Rain", 
    "Ice Fire", "Fusion", "Sepia", "Color1", "Color2", 
    "Color3", "Color4", "Color5", "Color6", "Color7"
};

const char* CameraController::SHUTTER_MODE_NAMES[] = {
    "Disabled", "Manual", "Automatic", "Fully Automatic"
};

const char* CameraController::MIRROR_MODE_NAMES[] = {
    "Disabled", "Central", "Horizontal", "Vertical"
};

// Variable estática para callback global
CameraController::ResponseCallback CameraController::_globalCallback = nullptr;

/**
 * Constructor de la clase CameraController.
 * @param serial Puerto serie para comunicación.
 * @param rxPin Pin RX del ESP32.
 * @param txPin Pin TX del ESP32.
 */
CameraController::CameraController(HardwareSerial* serial, uint8_t rxPin, uint8_t txPin) 
    : _serial(serial), _rxPin(rxPin), _txPin(txPin), _debugEnabled(false),
      _responseTimeout(RESPONSE_TIMEOUT), _byteTimeout(BYTE_TIMEOUT),
      _lastByteTime(0) {
    initializeResponse();
}

/**
 * Inicializa la comunicación con la cámara.
 * @return true si la inicialización fue exitosa, false en caso contrario.
 */
bool CameraController::begin() {
    if (!_serial) {
        _lastError = "Serial port not initialized";
        return false;
    }
    
    _serial->begin(UART_BAUDRATE, SERIAL_8N1, _rxPin, _txPin);
    delay(100);
    
    if (_debugEnabled) {
        Serial.println("Camera controller initialized");
        Serial.println("Ready for communication");
    }
    
    return true;
}

/**
 * Procesa los bytes de respuesta recibidos desde la cámara.
 */
void CameraController::update() {
    processResponseBytes();
}

// Funciones privadas de protocolo
uint8_t CameraController::calculateChecksum(uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen) {
    uint16_t sum = device + cls + subcls + rw;
    for (uint8_t i = 0; i < dataLen; i++) {
        sum += data[i];
    }
    return sum & 0xFF;
}

void CameraController::buildCommand(uint8_t *cmdBuffer, uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen) {
    cmdBuffer[0] = HEADER_BYTE;
    cmdBuffer[1] = 0x05; // Longitud fija para comandos básicos
    cmdBuffer[2] = device;
    cmdBuffer[3] = cls;
    cmdBuffer[4] = subcls;
    cmdBuffer[5] = rw;

    for (int i = 0; i < dataLen; i++) {
        cmdBuffer[6 + i] = data[i];
    }

    cmdBuffer[6 + dataLen] = calculateChecksum(device, cls, subcls, rw, data, dataLen);
    cmdBuffer[7 + dataLen] = FOOTER_BYTE;
}

bool CameraController::commandExpectsResponse(uint8_t rw) {
    // Solo los comandos READ (rw=0x01) esperan respuesta
    // Los comandos WRITE/ACTION (rw=0x00) no esperan respuesta
    return (rw == FLAG_READ);
}

bool CameraController::sendCommand(uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen) {
    uint8_t cmdBuffer[16];
    uint8_t totalLen = 8 + dataLen;
    
    buildCommand(cmdBuffer, DEVICE_ADDR, cls, subcls, rw, data, dataLen);
    
    if (_debugEnabled) {
        Serial.print("Sending command: ");
        for (uint8_t i = 0; i < totalLen; i++) {
            Serial.printf("0x%02X ", cmdBuffer[i]);
        }
        Serial.println();
    }
    
    // Solo esperar respuesta si es un comando READ (rw=0x01)
    bool shouldWaitForResponse = commandExpectsResponse(rw);
    
    if (shouldWaitForResponse) {
        initializeResponse();
    }
    
    _serial->write(cmdBuffer, totalLen);
    
    if (shouldWaitForResponse) {
        if (_debugEnabled) {
            Serial.println("Command expects response, waiting...");
        }
        return waitForResponse();
    } else {
        if (_debugEnabled) {
            Serial.println("Write/Action command sent (no response expected)");
        }
        return true; // Comando de escritura/acción enviado correctamente
    }
}

bool CameraController::sendDynamicCommand(uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen, String name) {
    if (_debugEnabled && name != "") {
        Serial.println("Sending: " + name);
    }
    return sendCommand(cls, subcls, rw, data, dataLen);
}

bool CameraController::sendRawCommand(const uint8_t *cmd, size_t len, String name) {
    if (_debugEnabled) {
        if (name != "") {
            Serial.println("Sending: " + name);
        }
        Serial.print("Raw command: ");
        for (size_t i = 0; i < len; i++) {
            Serial.printf("0x%02X ", cmd[i]);
        }
        Serial.println();
    }
    
    // Extraer el flag R/W del comando (posición 5 si es un comando válido)
    bool shouldWaitForResponse = false;
    if (len >= 6 && cmd[0] == HEADER_BYTE) {
        uint8_t rwFlag = cmd[5];
        shouldWaitForResponse = commandExpectsResponse(rwFlag);
    } else {
        // Si no es un comando válido o no podemos determinar el tipo, 
        // asumimos que no espera respuesta para evitar bloqueos
        if (_debugEnabled) {
            Serial.println("Warning: Cannot determine command type from raw data, assuming no response expected");
        }
    }
    
    if (shouldWaitForResponse) {
        initializeResponse();
    }
    
    _serial->write(cmd, len);
    
    if (shouldWaitForResponse) {
        if (_debugEnabled) {
            Serial.println("Raw command expects response, waiting...");
        }
        return waitForResponse();
    } else {
        if (_debugEnabled) {
            Serial.println("Raw write/action command sent (no response expected)");
        }
        return true; // Comando de escritura/acción enviado correctamente
    }
}

void CameraController::initializeResponse() {
    _currentResponse.length = 0;
    _currentResponse.timestamp = millis();
    _currentResponse.complete = false;
    _currentResponse.valid = false;
}


bool CameraController::waitForResponse(unsigned long timeout) {
    unsigned long startTime = millis();
    _lastByteTime = startTime;
    
    if (_debugEnabled) {
        Serial.println("Waiting for response...");
    }
    
    while (millis() - startTime < timeout) {
        if (_serial->available()) {
            while (_serial->available() && _currentResponse.length < MAX_RESPONSE_SIZE) {
                _currentResponse.data[_currentResponse.length] = _serial->read();
                if (_debugEnabled) {
                    Serial.printf("Received byte[%d]: 0x%02X\n", _currentResponse.length, _currentResponse.data[_currentResponse.length]);
                }
                _currentResponse.length++;
                _lastByteTime = millis();
            }
        }
        
        if (_currentResponse.length > 0 && 
            (millis() - _lastByteTime) > _byteTimeout) {
            _currentResponse.complete = true;
            _currentResponse.valid = (_currentResponse.length >= 7);
            
            if (_debugEnabled) {
                Serial.printf("Response complete. Length: %d, Valid: %s\n", 
                             _currentResponse.length, _currentResponse.valid ? "YES" : "NO");
            }
            
            if (_currentResponse.valid) {
                String interpretation = interpretResponse();
                
                if (_debugEnabled) {
                    Serial.println("=== RESPUESTA COMPLETA DEL DISPOSITIVO ===");
                    Serial.print("Raw data (" + String(_currentResponse.length) + " bytes): ");
                    for (size_t i = 0; i < _currentResponse.length; i++) {
                        Serial.printf("0x%02X ", _currentResponse.data[i]);
                    }
                    Serial.println();
                    Serial.println("Interpretación: " + interpretation);
                    Serial.println("==========================================");
                }
                
                // Llamar callback global si existe
                if (_globalCallback) {
                    _globalCallback(interpretation);
                }
            }
            
            return _currentResponse.valid;
        }
        
        delay(1);
    }
    
    if (_debugEnabled) {
        Serial.printf("Response timeout after %lu ms. Received %d bytes.\n", timeout, _currentResponse.length);
        if (_currentResponse.length > 0) {
            Serial.print("Partial data: ");
            for (size_t i = 0; i < _currentResponse.length; i++) {
                Serial.printf("0x%02X ", _currentResponse.data[i]);
            }
            Serial.println();
        }
    }
    
    _lastError = "Response timeout";
    return false;
}


void CameraController::processResponseBytes() {
    if (_serial->available()) {
        while (_serial->available() && _currentResponse.length < MAX_RESPONSE_SIZE) {
            _currentResponse.data[_currentResponse.length] = _serial->read();
            _currentResponse.length++;
            _lastByteTime = millis();
        }
    }
    
    if (_currentResponse.length > 0 && 
        (millis() - _lastByteTime) > _byteTimeout && 
        !_currentResponse.complete) {
        
        _currentResponse.complete = true;
        _currentResponse.valid = (_currentResponse.length >= 7);
        
        if (_currentResponse.valid) {
            String interpretation = interpretResponse();
            
            if (_debugEnabled) {
                Serial.print("Respuesta completa del dispositivo (");
                Serial.print(_currentResponse.length);
                Serial.print(" bytes): ");
                
                for (size_t i = 0; i < _currentResponse.length; i++) {
                    Serial.print("0x");
                    if (_currentResponse.data[i] < 0x10) Serial.print("0");
                    Serial.print(_currentResponse.data[i], HEX);
                    Serial.print(" ");
                }
                Serial.println();
            }
            
            // Usar callback global si está disponible
            if (_globalCallback) {
                _globalCallback(interpretResponse());
            }
        }
        
        initializeResponse();
    }
}

// Información del dispositivo
String CameraController::getModel() {
    if (sendCommand(CLASS_INFO, 0x02, FLAG_READ)) {
        if (_currentResponse.length >= 8) {
            String model = "";
            uint8_t dataLen = _currentResponse.data[6];
            for (int i = 7; i < 7 + dataLen && i < _currentResponse.length - 2; i++) {
                model += (char)_currentResponse.data[i];
            }
            return model;
        }
    }
    return "";
}

String CameraController::getFPGAVersion() {
    if (sendCommand(CLASS_INFO, 0x03, FLAG_READ)) {
        if (_currentResponse.length >= 10) {
            return String(_currentResponse.data[7]) + "." + 
                   String(_currentResponse.data[8]) + "." + 
                   String(_currentResponse.data[9]);
        }
    }
    return "";
}

String CameraController::getSoftwareVersion() {
    if (sendCommand(CLASS_INFO, 0x05, FLAG_READ)) {
        if (_currentResponse.length >= 10) {
            return String(_currentResponse.data[7]) + "." + 
                   String(_currentResponse.data[8]) + "." + 
                   String(_currentResponse.data[9]);
        }
    }
    return "";
}

CameraStatus CameraController::getStatus() {
    if (sendCommand(CLASS_CAMERA, 0x14, FLAG_READ)) {
        if (_currentResponse.length >= 8) {
            return (CameraStatus)_currentResponse.data[7];
        }
    }
    return CAMERA_ERROR;
}

// Control de imagen
bool CameraController::setBrightness(uint8_t value) {
    if (value > 100) {
        _lastError = "Brightness value out of range (0-100)";
        return false;
    }
    uint8_t data[] = {value};
    return sendCommand(CLASS_IMAGE, 0x02, FLAG_WRITE, data, 1);
}

bool CameraController::setContrast(uint8_t value) {
    if (value > 100) {
        _lastError = "Contrast value out of range (0-100)";
        return false;
    }
    uint8_t data[] = {value};
    return sendCommand(CLASS_IMAGE, 0x03, FLAG_WRITE, data, 1);
}

bool CameraController::setDigitalEnhancement(uint8_t value) {
    if (value > 100) {
        _lastError = "Digital enhancement value out of range (0-100)";
        return false;
    }
    uint8_t data[] = {value};
    return sendCommand(CLASS_IMAGE, 0x10, FLAG_WRITE, data, 1);
}

bool CameraController::setStaticNoiseReduction(uint8_t value) {
    if (value > 100) {
        _lastError = "Static noise reduction value out of range (0-100)";
        return false;
    }
    uint8_t data[] = {value};
    return sendCommand(CLASS_IMAGE, 0x15, FLAG_WRITE, data, 1);
}

bool CameraController::setDynamicNoiseReduction(uint8_t value) {
    if (value > 100) {
        _lastError = "Dynamic noise reduction value out of range (0-100)";
        return false;
    }
    uint8_t data[] = {value};
    return sendCommand(CLASS_IMAGE, 0x16, FLAG_WRITE, data, 1);
}

bool CameraController::setPalette(ColorPalette palette) {
    if (palette > PALETTE_COLOR7) {
        _lastError = "Invalid palette value";
        return false;
    }
    uint8_t data[] = {(uint8_t)palette};
    return sendCommand(CLASS_IMAGE, 0x20, FLAG_WRITE, data, 1);
}

// Lectura de valores actuales
uint8_t CameraController::getBrightness() {
    if (sendCommand(CLASS_IMAGE, 0x02, FLAG_READ) && _currentResponse.length >= 8) {
        return _currentResponse.data[7];
    }
    return 0;
}

uint8_t CameraController::getContrast() {
    if (sendCommand(CLASS_IMAGE, 0x03, FLAG_READ) && _currentResponse.length >= 8) {
        return _currentResponse.data[7];
    }
    return 0;
}

ColorPalette CameraController::getCurrentPalette() {
    if (sendCommand(CLASS_IMAGE, 0x20, FLAG_READ) && _currentResponse.length >= 8) {
        uint8_t paletteValue = _currentResponse.data[7]; // Corregido: usar resp[7] como en el código original
        if (paletteValue <= PALETTE_COLOR7) {
            return (ColorPalette)paletteValue;
        }
    }
    return PALETTE_WHITE_HOT;
}

// Control de obturador
bool CameraController::setAutoShutter(AutoShutterMode mode) {
    if (mode > SHUTTER_FULL_AUTO) {
        _lastError = "Invalid shutter mode";
        return false;
    }
    uint8_t data[] = {(uint8_t)mode};
    return sendCommand(CLASS_CAMERA, 0x04, FLAG_WRITE, data, 1);
}

bool CameraController::setShutterInterval(uint16_t minutes) {
    uint8_t data[] = {(uint8_t)(minutes >> 8), (uint8_t)(minutes & 0xFF)};
    return sendCommand(CLASS_CAMERA, 0x05, FLAG_WRITE, data, 2);
}

bool CameraController::performManualFFC() {
    return sendCommand(CLASS_CAMERA, 0x02, FLAG_WRITE);
}

bool CameraController::performBackgroundCorrection() {
    return sendCommand(CLASS_CAMERA, 0x03, FLAG_WRITE);
}

bool CameraController::performVignettingCorrection() {
    uint8_t data[] = {0x02};
    return sendCommand(CLASS_CAMERA, 0x0C, FLAG_WRITE, data, 1);
}

// Control de cursor
bool CameraController::showCursor() {
    uint8_t data[] = {0x0F};
    return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
}

bool CameraController::hideCursor() {
    uint8_t data[] = {0x00};
    return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
}

bool CameraController::centerCursor() {
    uint8_t data[] = {0x06};
    return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
}

bool CameraController::moveCursorUp(uint8_t pixels) {
    if (pixels == 0 || pixels > 15) {
        _lastError = "Pixels out of range (1-15)";
        return false;
    }
    
    if (pixels == 1) {
        uint8_t data[] = {0x02};
        return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
    } else {
        uint8_t data[] = {(uint8_t)(0x20 | (pixels & 0x0F))};
        return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
    }
}

bool CameraController::moveCursorDown(uint8_t pixels) {
    if (pixels == 0 || pixels > 15) {
        _lastError = "Pixels out of range (1-15)";
        return false;
    }
    
    if (pixels == 1) {
        uint8_t data[] = {0x03};
        return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
    } else {
        uint8_t data[] = {(uint8_t)(0x30 | (pixels & 0x0F))};
        return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
    }
}

bool CameraController::moveCursorLeft(uint8_t pixels) {
    if (pixels == 0 || pixels > 15) {
        _lastError = "Pixels out of range (1-15)";
        return false;
    }
    
    if (pixels == 1) {
        uint8_t data[] = {0x04};
        return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
    } else {
        uint8_t data[] = {(uint8_t)(0x40 | (pixels & 0x0F))};
        return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
    }
}

bool CameraController::moveCursorRight(uint8_t pixels) {
    if (pixels == 0 || pixels > 15) {
        _lastError = "Pixels out of range (1-15)";
        return false;
    }
    
    if (pixels == 1) {
        uint8_t data[] = {0x05};
        return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
    } else {
        uint8_t data[] = {(uint8_t)(0x50 | (pixels & 0x0F))};
        return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
    }
}

bool CameraController::addDeadPixel() {
    uint8_t data[] = {0x0D};
    return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
}

bool CameraController::removeDeadPixel() {
    uint8_t data[] = {0x0E};
    return sendCommand(CLASS_IMAGE, 0x1A, FLAG_WRITE, data, 1);
}

// Configuración del sistema
bool CameraController::saveConfiguration() {
    return sendCommand(CLASS_INFO, 0x10, FLAG_WRITE);
}

bool CameraController::restoreFactory() {
    return sendCommand(CLASS_INFO, 0x0F, FLAG_WRITE);
}

// Funciones de utilidad
bool CameraController::isConnected() {
    String model = getModel();
    return (model.length() > 0);
}

String CameraController::getLastError() {
    return _lastError;
}

void CameraController::enableDebug(bool enable) {
    _debugEnabled = enable;
}

void CameraController::setTimeouts(unsigned long responseTimeout, unsigned long byteTimeout) {
    _responseTimeout = responseTimeout;
    _byteTimeout = byteTimeout;
    
    if (_debugEnabled) {
        Serial.println("Timeouts set - Response: " + String(_responseTimeout) + "ms, Byte: " + String(_byteTimeout) + "ms");
    }
}

void CameraController::setGlobalResponseHandler(ResponseCallback callback) {
    _globalCallback = callback;
}

bool CameraController::getDeviceInfo(CameraInfo& info) {
    // Leer modelo
    info.model = getModel();
    if (info.model.length() == 0) {
        _lastError = "Failed to read device model";
        return false;
    }
    
    // Leer versión FPGA
    info.fpgaVersion = getFPGAVersion();
    
    // Leer fecha de compilación FPGA
    if (sendCommand(CLASS_INFO, 0x04, FLAG_READ)) {
        if (_currentResponse.length >= 11) {
            uint32_t fecha = (_currentResponse.data[7] << 24) | 
                            (_currentResponse.data[8] << 16) | 
                            (_currentResponse.data[9] << 8) | 
                            _currentResponse.data[10];
            info.fpgaBuildDate = String(fecha / 10000) + "-" + 
                               String((fecha / 100) % 100) + "-" + 
                               String(fecha % 100);
        }
    }
    
    // Leer versión software
    info.softwareVersion = getSoftwareVersion();
    
    // Leer fecha de compilación software
    if (sendCommand(CLASS_INFO, 0x06, FLAG_READ)) {
        if (_currentResponse.length >= 11) {
            uint32_t fecha = (_currentResponse.data[7] << 24) | 
                            (_currentResponse.data[8] << 16) | 
                            (_currentResponse.data[9] << 8) | 
                            _currentResponse.data[10];
            info.softwareBuildDate = String(fecha / 10000) + "-" + 
                                   String((fecha / 100) % 100) + "-" + 
                                   String(fecha % 100);
        }
    }
    
    // Leer versión de calibración
    if (sendCommand(CLASS_INFO, 0x07, FLAG_READ)) {
        if (_currentResponse.length >= 10) {
            info.calibrationVersion = String(_currentResponse.data[7]) + "." + 
                                     String(_currentResponse.data[8]) + "." + 
                                     String(_currentResponse.data[9]);
        }
    }
    
    // Leer versión ISP
    if (sendCommand(CLASS_INFO, 0x08, FLAG_READ)) {
        if (_currentResponse.length >= 10) {
            info.ispVersion = String(_currentResponse.data[7]) + "." + 
                             String(_currentResponse.data[8]) + "." + 
                             String(_currentResponse.data[9]);
        }
    }
    
    // Leer estado
    info.status = getStatus();
    
    return true;
}


String CameraController::getFPGABuildDate() {
    if (sendCommand(CLASS_INFO, 0x04, FLAG_READ)) {
        if (_currentResponse.length >= 11) {
            uint32_t fecha = (_currentResponse.data[7] << 24) | 
                            (_currentResponse.data[8] << 16) | 
                            (_currentResponse.data[9] << 8) | 
                            _currentResponse.data[10];
            return String(fecha / 10000) + "-" + 
                   String((fecha / 100) % 100) + "-" + 
                   String(fecha % 100);
        }
    }
    return "";
}

String CameraController::getSoftwareBuildDate() {
    if (sendCommand(CLASS_INFO, 0x06, FLAG_READ)) {
        if (_currentResponse.length >= 11) {
            uint32_t fecha = (_currentResponse.data[7] << 24) | 
                            (_currentResponse.data[8] << 16) | 
                            (_currentResponse.data[9] << 8) | 
                            _currentResponse.data[10];
            return String(fecha / 10000) + "-" + 
                   String((fecha / 100) % 100) + "-" + 
                   String(fecha % 100);
        }
    }
    return "";
}


uint8_t CameraController::getDigitalEnhancement() {
    if (sendCommand(CLASS_IMAGE, 0x10, FLAG_READ) && _currentResponse.length >= 8) {
        return _currentResponse.data[7];
    }
    return 0;
}

uint8_t CameraController::getStaticNoiseReduction() {
    if (sendCommand(CLASS_IMAGE, 0x15, FLAG_READ) && _currentResponse.length >= 8) {
        return _currentResponse.data[7];
    }
    return 0;
}

uint8_t CameraController::getDynamicNoiseReduction() {
    if (sendCommand(CLASS_IMAGE, 0x16, FLAG_READ) && _currentResponse.length >= 8) {
        return _currentResponse.data[7];
    }
    return 0;
}

MirrorMode CameraController::getCurrentMirror() {
    if (sendCommand(CLASS_MIRROR, 0x11, FLAG_READ) && _currentResponse.length >= 8) {
        uint8_t mirrorValue = _currentResponse.data[7];
        if (mirrorValue <= MIRROR_VERTICAL) {
            return (MirrorMode)mirrorValue;
        }
    }
    return MIRROR_DISABLED;
}

bool CameraController::setMirror(MirrorMode mode) {
    if (mode > MIRROR_VERTICAL) {
        _lastError = "Invalid mirror mode";
        return false;
    }
    uint8_t data[] = {(uint8_t)mode};
    return sendCommand(CLASS_MIRROR, 0x11, FLAG_WRITE, data, 1);
}

AutoShutterMode CameraController::getAutoShutterMode() {
    if (sendCommand(CLASS_CAMERA, 0x04, FLAG_READ) && _currentResponse.length >= 8) {
        uint8_t shutterValue = _currentResponse.data[7];
        if (shutterValue <= SHUTTER_FULL_AUTO) {
            return (AutoShutterMode)shutterValue;
        }
    }
    return SHUTTER_DISABLED;
}

uint16_t CameraController::getShutterInterval() {
    if (sendCommand(CLASS_CAMERA, 0x05, FLAG_READ) && _currentResponse.length >= 9) {
        return (_currentResponse.data[7] << 8) | _currentResponse.data[8];
    }
    return 0;
}

String CameraController::getCalibrationVersion() {
    if (sendCommand(CLASS_INFO, 0x07, FLAG_READ)) {
        if (_currentResponse.length >= 10) {
            return String(_currentResponse.data[7]) + "." + 
                   String(_currentResponse.data[8]) + "." + 
                   String(_currentResponse.data[9]);
        }
    }
    return "";
}

String CameraController::getISPVersion() {
    if (sendCommand(CLASS_INFO, 0x08, FLAG_READ)) {
        if (_currentResponse.length >= 10) {
            return String(_currentResponse.data[7]) + "." + 
                   String(_currentResponse.data[8]) + "." + 
                   String(_currentResponse.data[9]);
        }
    }
    return "";
}

// INTERPRETACIÓN COMPLETA DE RESPUESTAS - IGUAL QUE EL CÓDIGO ORIGINAL
String CameraController::interpretResponse() {
    if (!_currentResponse.valid || _currentResponse.length < 7) {
        return "Invalid response";
    }
    
    uint8_t* resp = _currentResponse.data;
    uint8_t header = resp[0];      // 0xF0
    uint8_t length = resp[1];      // 0x05
    uint8_t device = resp[2];      // 0x36
    uint8_t classByte = resp[3];   // 0x78
    uint8_t subClassByte = resp[4]; // 0x20
    uint8_t rwFlag = resp[5];      // 0x03
    uint8_t dataLength = resp[6];  // 0x01
    // Datos empiezan en resp[7]
    
    String interpretation = "";
    
    if (_debugEnabled) {
        Serial.println("\n=== INTERPRETACIÓN DE RESPUESTA ===");
        Serial.printf("📋 Estructura: H=0x%02X L=0x%02X D=0x%02X C=0x%02X S=0x%02X R=0x%02X DL=0x%02X\n", 
                      header, length, device, classByte, subClassByte, rwFlag, dataLength);

        // Imprimir respuesta completa en todos los formatos
        Serial.print("🔍 Respuesta completa (hex): ");
        for (size_t i = 0; i < _currentResponse.length; i++) {
            if (resp[i] < 0x10) Serial.print("0");
            Serial.print(resp[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        
        Serial.print("🔍 Respuesta completa (bin): ");
        for (size_t i = 0; i < _currentResponse.length; i++) {
            Serial.print(resp[i], BIN);
            Serial.print(" ");
        }
        Serial.println();

        Serial.print("🔍 Respuesta completa (dec): ");
        for (size_t i = 0; i < _currentResponse.length; i++) {
            Serial.print(resp[i], DEC);
            Serial.print(" ");
        }
        Serial.println();

        Serial.print("🔍 Respuesta completa (oct): ");
        for (size_t i = 0; i < _currentResponse.length; i++) {
            Serial.print(resp[i], OCT);
            Serial.print(" ");
        }
        Serial.println();
    }

    // COMANDOS DE LECTURA DE INFORMACIÓN (Clase 0x74)
    if (classByte == 0x74) {
        switch (subClassByte) {
            case 0x02: // Modelo del módulo (ASCII)
                {
                    String model = "";
                    for (int i = 7; i < 7 + dataLength; i++) {
                        if (i < _currentResponse.length - 2) model += (char)resp[i];
                    }
                    interpretation = "📦 Modelo del módulo: " + model;
                }
                break;
                
            case 0x03: // Versión FPGA
                if (dataLength >= 3) {
                    interpretation = "🔧 Versión FPGA: " + String(resp[7]) + "." + String(resp[8]) + "." + String(resp[9]);
                }
                break;
                
            case 0x04: // Compilación FPGA
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    interpretation = "📅 Compilación FPGA: " + String(fecha / 10000) + "-" + String((fecha / 100) % 100) + "-" + String(fecha % 100);
                }
                break;
                
            case 0x05: // Versión software
                if (dataLength >= 3) {
                    interpretation = "💾 Versión software: " + String(resp[7]) + "." + String(resp[8]) + "." + String(resp[9]);
                }
                break;
                
            case 0x06: // Compilación software
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    interpretation = "📅 Compilación software: " + String(fecha / 10000) + "-" + String((fecha / 100) % 100) + "-" + String(fecha % 100);
                }
                break;
                
            case 0x0B: // Versión calibración cámara
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    interpretation = "📷 Versión calibración cámara: " + String(fecha / 10000) + "-" + String((fecha / 100) % 100) + "-" + String(fecha % 100);
                }
                break;
                
            case 0x0C: // Versión parámetros ISP
                if (dataLength >= 4) {
                    interpretation = "⚙️  Versión parámetros ISP: 0x" + String(resp[7], HEX) + String(resp[8], HEX) + String(resp[9], HEX) + String(resp[10], HEX);
                }
                break;
                
            case 0x10: // Guardar configuración
                interpretation = "💾 Configuración guardada correctamente";
                break;
                
            case 0x0F: // Restaurar fábrica
                interpretation = "🔄 Configuración de fábrica restaurada";
                break;
                
            default:
                interpretation = "❓ Comando clase 0x74, subclase 0x" + String(subClassByte, HEX) + " no interpretado";
                break;
        }
    }
    // COMANDOS DE CONTROL DE CÁMARA (Clase 0x7C)
    else if (classByte == 0x7C) {
        switch (subClassByte) {
            case 0x14: // Estado de inicialización
                if (dataLength >= 1) {
                    switch (resp[7]) {
                        case 0x00:
                            interpretation = "📺 Estado: Inicializando (loading)";
                            break;
                        case 0x01:
                            interpretation = "📺 Estado: Video activo (output)";
                            break;
                        default:
                            interpretation = "📺 Estado desconocido: 0x" + String(resp[7], HEX);
                            break;
                    }
                }
                break;
                
            case 0x02: // Calibración obturador manual
                interpretation = "🎯 Calibración de obturador manual ejecutada";
                break;
                
            case 0x03: // Corrección de fondo manual
                interpretation = "🎨 Corrección de fondo manual ejecutada";
                break;
                
            case 0x04: // Control auto obturador
                if (dataLength >= 1) {
                    const char* modos[] = {"Deshabilitado", "Manual", "Automático", "Totalmente automático"};
                    int modo = resp[7];
                    if (modo <= 3) {
                        if (rwFlag == 0x01) {
                            interpretation = "📷 Obturador automático actual: " + String(modos[modo]) + " (valor: " + String(modo) + ")";
                        } else {
                            interpretation = "📷 Obturador automático configurado: " + String(modos[modo]);
                        }
                    } else {
                        interpretation = "📷 Modo obturador: 0x" + String(resp[7], HEX);
                    }
                }
                break;
                
            case 0x05: // Intervalo obturación automático
                if (dataLength >= 2) {
                    uint16_t minutos = (resp[7] << 8) | resp[8];
                    if (rwFlag == 0x01) {
                        interpretation = "⏱️  Intervalo obturación automático actual: " + String(minutos) + " minutos";
                    } else {
                        interpretation = "⏱️  Intervalo obturación automático configurado: " + String(minutos) + " minutos";
                    }
                } else if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        interpretation = "⏱️  Intervalo obturación automático actual: " + String(resp[7]) + " minutos";
                    } else {
                        interpretation = "⏱️  Intervalo obturación automático configurado: " + String(resp[7]) + " minutos";
                    }
                }
                break;
                
            case 0x0C: // Corrección viñeteado
                interpretation = "🔧 Corrección de viñeteado ejecutada";
                break;
                
            default:
                interpretation = "❓ Comando clase 0x7C, subclase 0x" + String(subClassByte, HEX) + " no interpretado";
                break;
        }
    }
    // COMANDOS DE IMAGEN (Clase 0x78)
    else if (classByte == 0x78) {
        switch (subClassByte) {
            case 0x02: // Brillo
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        interpretation = "☀️  Brillo actual: " + String(resp[7]) + "/100";
                    } else {
                        interpretation = "☀️  Brillo configurado: " + String(resp[7]) + "/100";
                    }
                }
                break;
                
            case 0x03: // Contraste
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        interpretation = "🌗 Contraste actual: " + String(resp[7]) + "/100";
                    } else {
                        interpretation = "🌗 Contraste configurado: " + String(resp[7]) + "/100";
                    }
                }
                break;
                
            case 0x10: // Mejora digital de detalle
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        interpretation = "🔍 Mejora digital detalle actual: " + String(resp[7]) + "/100";
                    } else {
                        interpretation = "🔍 Mejora digital detalle configurado: " + String(resp[7]) + "/100";
                    }
                }
                break;
                
            case 0x15: // Denoising estático
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        interpretation = "🔇 Reducción ruido estático actual: " + String(resp[7]) + "/100";
                    } else {
                        interpretation = "🔇 Reducción ruido estático configurado: " + String(resp[7]) + "/100";
                    }
                }
                break;
                
            case 0x16: // Denoising dinámico
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        interpretation = "🔊 Reducción ruido dinámico actual: " + String(resp[7]) + "/100";
                    } else {
                        interpretation = "🔊 Reducción ruido dinámico configurado: " + String(resp[7]) + "/100";
                    }
                }
                break;
                
            case 0x20: // Paleta de colores
                if (dataLength >= 1) {
                    const char* paletas[] = {
                        "White Hot", "Black Hot", "Iron", "Rainbow", "Rain", 
                        "Ice Fire", "Fusion", "Sepia", "Color1", "Color2", 
                        "Color3", "Color4", "Color5", "Color6", "Color7"
                    };
                    
                    // ANÁLISIS CORRECTO DE LA RESPUESTA DE PALETA:
                    // Según main copy.cpp, el valor real está en resp[6], no en resp[7]
                    int paleta = resp[6];  // USAR resp[6] para paleta
                    
                    if (_debugEnabled) {
                        Serial.printf("🔍 Debug: Valor leído en resp[6] = 0x%02X (%d)\n", paleta, paleta);
                    }

                    if (paleta <= 14) {
                        if (rwFlag == 0x01) {
                            interpretation = "🎨 Paleta actual: " + String(paletas[paleta]) + " (valor: 0x" + String(paleta, HEX) + ")";
                        } else {
                            interpretation = "🎨 Paleta configurada: " + String(paletas[paleta]) + " (valor: 0x" + String(paleta, HEX) + ")";
                        }
                    } else {
                        interpretation = "🎨 Paleta: Modo desconocido 0x" + String(paleta, HEX);
                    }
                }
                break;
                
            case 0x1A: // Comandos de cursor
                if (dataLength >= 1) {
                    uint8_t cursorData = resp[7];
                    
                    switch (cursorData) {
                        case 0x00: interpretation = "👁️‍🗨️ Cursor ocultado"; break;
                        case 0x02: interpretation = "⬆️  Cursor movido arriba"; break;
                        case 0x03: interpretation = "⬇️  Cursor movido abajo"; break;
                        case 0x04: interpretation = "⬅️  Cursor movido izquierda"; break;
                        case 0x05: interpretation = "➡️  Cursor movido derecha"; break;
                        case 0x06: interpretation = "🎯 Cursor centrado"; break;
                        case 0x0D: interpretation = "❌ Píxel defectuoso agregado"; break;
                        case 0x0E: interpretation = "✅ Píxel defectuoso removido"; break;
                        case 0x0F: interpretation = "👁️  Cursor mostrado"; break;
                        default:
                            // Comandos de movimiento múltiple
                            if ((cursorData & 0xF0) == 0x20) {
                                interpretation = "⬆️  Cursor movido " + String(cursorData & 0x0F) + " píxeles arriba";
                            } else if ((cursorData & 0xF0) == 0x30) {
                                interpretation = "⬇️  Cursor movido " + String(cursorData & 0x0F) + " píxeles abajo";
                            } else if ((cursorData & 0xF0) == 0x40) {
                                interpretation = "⬅️  Cursor movido " + String(cursorData & 0x0F) + " píxeles izquierda";
                            } else if ((cursorData & 0xF0) == 0x50) {
                                interpretation = "➡️  Cursor movido " + String(cursorData & 0x0F) + " píxeles derecha";
                            } else {
                                interpretation = "🖱️  Comando cursor: 0x" + String(cursorData, HEX);
                            }
                            break;
                    }
                }
                break;
                
            default:
                interpretation = "❓ Comando clase 0x78, subclase 0x" + String(subClassByte, HEX) + " no interpretado";
                break;
        }
    }
    // COMANDOS DE MIRRORING (Clase 0x70)
    else if (classByte == 0x70) {
        switch (subClassByte) {
            case 0x11: // Mirroring
                if (dataLength >= 1) {
                    const char* modos[] = {"Deshabilitado", "Central", "Horizontal", "Vertical"};
                    int modo = resp[7];
                    if (modo <= 3) {
                        if (rwFlag == 0x01) {
                            interpretation = "🪞 Mirroring actual: " + String(modos[modo]) + " (valor: " + String(modo) + ")";
                        } else {
                            interpretation = "🪞 Mirroring configurado: " + String(modos[modo]);
                        }
                    } else {
                        interpretation = "🪞 Mirroring: Modo " + String(resp[7]);
                    }
                }
                break;
                
            default:
                interpretation = "❓ Comando clase 0x70, subclase 0x" + String(subClassByte, HEX) + " no interpretado";
                break;
        }
    }
    else {
        interpretation = "❓ Clase 0x" + String(classByte, HEX) + " no reconocida";
    }
    
    if (_debugEnabled) {
        Serial.println("====================================\n");
    }
    
    return interpretation;
}

// Métodos para comandos de lectura
bool CameraController::readDeviceModel() {
    uint8_t data[] = {0x02};
    return sendCommand(CLASS_INFO, 0x02, FLAG_READ, data, sizeof(data));
}

bool CameraController::readFPGA_Version() {
    uint8_t data[] = {0x03};
    return sendCommand(CLASS_INFO, 0x03, FLAG_READ, data, sizeof(data));
}

bool CameraController::readInitializationStatus() {
    uint8_t data[] = {0x14};
    return sendCommand(CLASS_CAMERA, 0x14, FLAG_READ, data, sizeof(data));
}

void CameraController::testBuildAndSendCommand(uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t* data, uint8_t dataLen) {
    uint8_t cmdBuffer[16];
    buildCommand(cmdBuffer, DEVICE_ADDR, cls, subcls, rw, data, dataLen);

    Serial.print("Comando construido: ");
    for (int i = 0; i < 8 + dataLen; i++) {
        Serial.printf("0x%02X ", cmdBuffer[i]);
    }
    Serial.println();

    if (sendCommand(cls, subcls, rw, data, dataLen)) {
        Serial.println("✅ Comando enviado correctamente");
    } else {
        Serial.println("❌ Error al enviar el comando");
    }
}