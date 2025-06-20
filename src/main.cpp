#include <Arduino.h>
#include <HardwareSerial.h>

// Configuración del segundo UART
HardwareSerial SerialDevice(2); // UART2
#define TX_PIN 17 //tx
#define RX_PIN 16 //rx

String responseBuffer = "";
unsigned long lastByteTime = 0;
const unsigned long RESPONSE_TIMEOUT = 100; // 100ms timeout entre bytes


// Estructura para comandos
struct Command {
    const char* name;
    uint8_t data[9];
    uint8_t length;
};

struct Response {
    uint8_t data[256];
    size_t length;
    unsigned long timestamp;
    bool complete;
};

Response currentResponse = {0};


// Variable para controlar el estado del menú
enum MenuState {
    MAIN_MENU,
    READ_MENU,
    WRITE_MENU,
    READWRITE_MENU,
    CURSOR_MENU
};

MenuState currentMenu = MAIN_MENU;

Command readCommands[] = {
    {"Leer modelo del módulo", {0xF0, 0x05, 0x36, 0x74, 0x02, 0x01, 0x00, 0xAD, 0xFF}, 9},
    {"Leer versión del FPGA", {0xF0, 0x05, 0x36, 0x74, 0x03, 0x01, 0x00, 0xAE, 0xFF}, 9},
    {"Leer compilación del FPGA", {0xF0, 0x05, 0x36, 0x74, 0x04, 0x01, 0x00, 0xAF, 0xFF}, 9},
    {"Leer versión del software", {0xF0, 0x05, 0x36, 0x74, 0x05, 0x01, 0x00, 0xB0, 0xFF}, 9},
    {"Leer compilación del software", {0xF0, 0x05, 0x36, 0x74, 0x06, 0x01, 0x00, 0xB1, 0xFF}, 9},
    {"Leer versión calibración cámara", {0xF0, 0x05, 0x36, 0x74, 0x0B, 0x01, 0x00, 0xB6, 0xFF}, 9},
    {"Leer versión parámetros ISP", {0xF0, 0x05, 0x36, 0x74, 0x0C, 0x01, 0x00, 0xB7, 0xFF}, 9},
    {"Leer estado de inicialización", {0xF0, 0x05, 0x36, 0x7C, 0x14, 0x01, 0x00, 0xC7, 0xFF}, 9}
};

Command writeCommands[] = {
    {"Guardar configuración", {0xF0, 0x05, 0x36, 0x74, 0x10, 0x00, 0x00, 0xBA, 0xFF}, 9},
    {"Restaurar fábrica", {0xF0, 0x05, 0x36, 0x74, 0x0F, 0x00, 0x00, 0xB9, 0xFF}, 9},
    {"Calibración obturador manual (FFC)", {0xF0, 0x05, 0x36, 0x7C, 0x02, 0x00, 0x00, 0xB4, 0xFF}, 9},
    {"Corrección de fondo manual", {0xF0, 0x05, 0x36, 0x7C, 0x03, 0x00, 0x00, 0xB5, 0xFF}, 9},
    {"Corrección viñeteado", {0xF0, 0x05, 0x36, 0x7C, 0x0C, 0x00, 0x02, 0xC0, 0xFF}, 9}
};

Command readWriteCommands[] = {
    {"Brillo (100)", {0xF0, 0x05, 0x36, 0x78, 0x02, 0x00, 0x64, 0x14, 0xFF}, 9},
    {"Contraste (65)", {0xF0, 0x05, 0x36, 0x78, 0x03, 0x00, 0x41, 0xF2, 0xFF}, 9},
    {"Mejora digital detalle (50)", {0xF0, 0x05, 0x36, 0x78, 0x10, 0x00, 0x32, 0xF0, 0xFF}, 9},
    {"Denoising estático (50)", {0xF0, 0x05, 0x36, 0x78, 0x15, 0x00, 0x32, 0xF5, 0xFF}, 9},
    {"Denoising dinámico (50)", {0xF0, 0x05, 0x36, 0x78, 0x16, 0x00, 0x32, 0xF6, 0xFF}, 9},
    {"Paleta Rainbow", {0xF0, 0x05, 0x36, 0x78, 0x20, 0x00, 0x03, 0xD1, 0xFF}, 9},
    {"Control auto obturador", {0xF0, 0x05, 0x36, 0x7C, 0x04, 0x00, 0x03, 0xBF, 0xFF}, 9},
    {"Mirroring central", {0xF0, 0x05, 0x36, 0x70, 0x11, 0x00, 0x01, 0xB8, 0xFF}, 9}
};

Command cursorCommands[] = {
    {"Mostrar cursor", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x0F, 0xD7, 0xFF}, 9},
    {"Mover cursor arriba", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x02, 0xCA, 0xFF}, 9},
    {"Mover cursor abajo", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x03, 0xCB, 0xFF}, 9},
    {"Mover cursor izquierda", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x04, 0xCC, 0xFF}, 9},
    {"Mover cursor derecha", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x05, 0xCD, 0xFF}, 9},
    {"Cursor al centro", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x06, 0xCE, 0xFF}, 9},
    {"Agregar píxel defectuoso", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x0D, 0xD5, 0xFF}, 9},
    {"Quitar píxel defectuoso", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x0E, 0xD6, 0xFF}, 9}
};

// Obtener tamaños de arrays
const int readCommandsSize = sizeof(readCommands) / sizeof(readCommands[0]);
const int writeCommandsSize = sizeof(writeCommands) / sizeof(writeCommands[0]);
const int readWriteCommandsSize = sizeof(readWriteCommands) / sizeof(readWriteCommands[0]);
const int cursorCommandsSize = sizeof(cursorCommands) / sizeof(cursorCommands[0]);

String receivedMessage;

// Función para calcular checksum
uint8_t calcCHK(uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen) {
    uint16_t sum = device + cls + subcls + rw;
    for (uint8_t i = 0; i < dataLen; i++) {
        sum += data[i];
    }
    return sum & 0xFF; // Últimos 8 bits
}


// Función para construir comando completo con checksum automático
void buildCommand(uint8_t *cmdBuffer, uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen) {
    cmdBuffer[0] = 0xF0;  // Header
    cmdBuffer[1] = 0x05 + dataLen;  // Length (5 bytes fijos + datos)
    cmdBuffer[2] = device;  // Device Address
    cmdBuffer[3] = cls;     // Class Address
    cmdBuffer[4] = subcls;  // Subclass Address
    cmdBuffer[5] = rw;      // R/W Flag
    cmdBuffer[6] = dataLen; // Data Length
    
    // Copiar datos
    for (int i = 0; i < dataLen; i++) {
        cmdBuffer[7 + i] = data[i];
    }
    
    // Calcular y agregar checksum
    cmdBuffer[7 + dataLen] = calcCHK(device, cls, subcls, rw, data, dataLen);
    cmdBuffer[8 + dataLen] = 0xFF;  // Footer
}


void inicializarRespuesta() {
    currentResponse.length = 0;
    currentResponse.timestamp = millis();
    currentResponse.complete = false;
}


void interpretarRespuesta(uint8_t* resp, size_t size) {
    if (size < 7) return;

    uint8_t classByte = resp[3];
    uint8_t subClassByte = resp[4];
    uint8_t dataLength = resp[6];

    Serial.println("\n=== INTERPRETACIÓN DE RESPUESTA ===");

    // COMANDOS DE LECTURA DE INFORMACIÓN (Clase 0x74)
    if (classByte == 0x74) {
        switch (subClassByte) {
            case 0x02: // Modelo del módulo (ASCII)
                {
                    String model = "";
                    for (int i = 7; i < 7 + dataLength; i++) {
                        if (i < size - 2) model += (char)resp[i];
                    }
                    Serial.println("📦 Modelo del módulo: " + model);
                }
                break;
                
            case 0x03: // Versión FPGA (3 bytes: X.Y.Z)
                if (dataLength >= 3) {
                    Serial.printf("🔧 Versión FPGA: %d.%d.%d\n", resp[7], resp[8], resp[9]);
                }
                break;
                
            case 0x04: // Compilación FPGA (4 bytes: fecha YYYYMMDD)
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    Serial.printf("📅 Compilación FPGA: %04lu-%02lu-%02lu\n", 
                                fecha / 10000, (fecha / 100) % 100, fecha % 100);
                }
                break;
                
            case 0x05: // Versión software (3 bytes: X.Y.Z)
                if (dataLength >= 3) {
                    Serial.printf("💾 Versión software: %d.%d.%d\n", resp[7], resp[8], resp[9]);
                }
                break;
                
            case 0x06: // Compilación software (4 bytes: fecha YYYYMMDD)
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    Serial.printf("📅 Compilación software: %04lu-%02lu-%02lu\n", 
                                fecha / 10000, (fecha / 100) % 100, fecha % 100);
                }
                break;
                
            case 0x0B: // Versión calibración proceso cámara (4 bytes: fecha)
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    Serial.printf("📷 Versión calibración cámara: %04lu-%02lu-%02lu\n", 
                                fecha / 10000, (fecha / 100) % 100, fecha % 100);
                }
                break;
                
            case 0x0C: // Versión parámetros ISP (4 bytes válidos)
                if (dataLength >= 4) {
                    Serial.printf("⚙️  Versión parámetros ISP: 0x%02X%02X%02X%02X\n", 
                                resp[7], resp[8], resp[9], resp[10]);
                }
                break;
                
            case 0x10: // Guardar configuración (respuesta)
                Serial.println("💾 Configuración guardada correctamente");
                break;
                
            case 0x0F: // Restaurar fábrica (respuesta)
                Serial.println("🔄 Configuración de fábrica restaurada");
                break;
                
            default:
                Serial.printf("❓ Comando clase 0x74, subclase 0x%02X no interpretado\n", subClassByte);
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
                            Serial.println("📺 Estado: Inicializando (loading)");
                            break;
                        case 0x01:
                            Serial.println("📺 Estado: Video activo (output)");
                            break;
                        default:
                            Serial.printf("📺 Estado desconocido: 0x%02X\n", resp[7]);
                            break;
                    }
                }
                break;
                
            case 0x02: // Calibración obturador manual (FFC)
                Serial.println("🎯 Calibración de obturador manual ejecutada");
                break;
                
            case 0x03: // Corrección de fondo manual
                Serial.println("🎨 Corrección de fondo manual ejecutada");
                break;
                
            case 0x04: // Control auto obturador
                if (dataLength >= 1) {
                    switch (resp[7]) {
                        case 0x00:
                            Serial.println("📷 Obturador automático: Deshabilitado");
                            break;
                        case 0x01:
                            Serial.println("📷 Obturador automático: Manual");
                            break;
                        case 0x02:
                            Serial.println("📷 Obturador automático: Automático");
                            break;
                        case 0x03:
                            Serial.println("📷 Obturador automático: Totalmente automático");
                            break;
                        default:
                            Serial.printf("📷 Modo obturador: 0x%02X\n", resp[7]);
                            break;
                    }
                }
                break;
                
            case 0x0C: // Corrección viñeteado
                Serial.println("🔧 Corrección de viñeteado ejecutada");
                break;
                
            default:
                Serial.printf("❓ Comando clase 0x7C, subclase 0x%02X no interpretado\n", subClassByte);
                break;
        }
    }
    // COMANDOS DE IMAGEN (Clase 0x78)
    else if (classByte == 0x78) {
        switch (subClassByte) {
            case 0x02: // Brillo
                if (dataLength >= 1) {
                    Serial.printf("☀️  Brillo configurado: %d\n", resp[7]);
                }
                break;
                
            case 0x03: // Contraste
                if (dataLength >= 1) {
                    Serial.printf("🌗 Contraste configurado: %d\n", resp[7]);
                }
                break;
                
            case 0x10: // Mejora digital de detalle
                if (dataLength >= 1) {
                    Serial.printf("🔍 Mejora digital detalle: %d\n", resp[7]);
                }
                break;
                
            case 0x15: // Denoising estático
                if (dataLength >= 1) {
                    Serial.printf("🔇 Denoising estático: %d\n", resp[7]);
                }
                break;
                
            case 0x16: // Denoising dinámico
                if (dataLength >= 1) {
                    Serial.printf("🔊 Denoising dinámico: %d\n", resp[7]);
                }
                break;
                
            case 0x20: // Paleta de colores
                if (dataLength >= 1) {
                    switch (resp[7]) {
                        case 0x00:
                            Serial.println("🎨 Paleta: Blanco y negro");
                            break;
                        case 0x01:
                            Serial.println("🎨 Paleta: Iron");
                            break;
                        case 0x02:
                            Serial.println("🎨 Paleta: Cool");
                            break;
                        case 0x03:
                            Serial.println("🎨 Paleta: Rainbow");
                            break;
                        default:
                            Serial.printf("🎨 Paleta: Modo %d\n", resp[7]);
                            break;
                    }
                }
                break;
                
            case 0x1A: // Comandos de cursor
                if (dataLength >= 1) {
                    switch (resp[7]) {
                        case 0x02:
                            Serial.println("⬆️  Cursor movido arriba");
                            break;
                        case 0x03:
                            Serial.println("⬇️  Cursor movido abajo");
                            break;
                        case 0x04:
                            Serial.println("⬅️  Cursor movido izquierda");
                            break;
                        case 0x05:
                            Serial.println("➡️  Cursor movido derecha");
                            break;
                        case 0x06:
                            Serial.println("🎯 Cursor centrado");
                            break;
                        case 0x0D:
                            Serial.println("❌ Píxel defectuoso agregado");
                            break;
                        case 0x0E:
                            Serial.println("✅ Píxel defectuoso removido");
                            break;
                        case 0x0F:
                            Serial.println("👁️  Cursor mostrado");
                            break;
                        default:
                            Serial.printf("🖱️  Comando cursor: 0x%02X\n", resp[7]);
                            break;
                    }
                }
                break;
                
            default:
                Serial.printf("❓ Comando clase 0x78, subclase 0x%02X no interpretado\n", subClassByte);
                break;
        }
    }
    // COMANDOS DE MIRRORING (Clase 0x70)
    else if (classByte == 0x70) {
        switch (subClassByte) {
            case 0x11: // Mirroring
                if (dataLength >= 1) {
                    switch (resp[7]) {
                        case 0x00:
                            Serial.println("🪞 Mirroring: Deshabilitado");
                            break;
                        case 0x01:
                            Serial.println("🪞 Mirroring: Central");
                            break;
                        case 0x02:
                            Serial.println("🪞 Mirroring: Horizontal");
                            break;
                        case 0x03:
                            Serial.println("🪞 Mirroring: Vertical");
                            break;
                        default:
                            Serial.printf("🪞 Mirroring: Modo %d\n", resp[7]);
                            break;
                    }
                }
                break;
                
            default:
                Serial.printf("❓ Comando clase 0x70, subclase 0x%02X no interpretado\n", subClassByte);
                break;
        }
    }
    else {
        Serial.printf("❓ Clase 0x%02X no reconocida\n", classByte);
    }
    
    Serial.println("====================================\n");
}

void mostrarRespuestaCompleta() {
    Serial.print("Respuesta completa del dispositivo (");
    Serial.print(currentResponse.length);
    Serial.print(" bytes): ");
    
    for (size_t i = 0; i < currentResponse.length; i++) {
        Serial.print("0x");
        if (currentResponse.data[i] < 0x10) Serial.print("0");
        Serial.print(currentResponse.data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Interpretar la respuesta en formato human readable
    interpretarRespuesta(currentResponse.data, currentResponse.length);
    
    // Reinicializar para la próxima respuesta
    inicializarRespuesta();
}

// Función mejorada para enviar comandos dinámicos
void sendDynamicCommand(uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen, String name = "") {
    uint8_t cmdBuffer[16];  // Buffer suficiente para comando completo
    buildCommand(cmdBuffer, device, cls, subcls, rw, data, dataLen);
    
    uint8_t totalLen = 9 + dataLen;  // 9 bytes fijos + datos
    
    if (name != "") {
        Serial.println("Enviando: " + name);
    }
    Serial.print("Comando dinámico: ");
    for (size_t i = 0; i < totalLen; i++) {
        Serial.print("0x");
        if (cmdBuffer[i] < 0x10) Serial.print("0");
        Serial.print(cmdBuffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Inicializar captura de respuesta
    inicializarRespuesta();
    
    // Enviar comando por UART2
    SerialDevice.write(cmdBuffer, totalLen);
    Serial.println("Comando enviado por UART2");
}


void sendCommand(const uint8_t *cmd, size_t len, String name = "") {
    if (name != "") {
        Serial.println("Enviando: " + name);
    }
    Serial.print("Comando: ");
    for (size_t i = 0; i < len; i++) {
        Serial.print("0x");
        if (cmd[i] < 0x10) Serial.print("0");
        Serial.print(cmd[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Inicializar captura de respuesta
    inicializarRespuesta();
    
    // Enviar comando por UART2 al dispositivo
    SerialDevice.write(cmd, len);
    Serial.println("Comando enviado por UART2");
}


// Función para crear comando de lectura
void sendReadCommand(uint8_t cls, uint8_t subcls, String name = "") {
    uint8_t data[] = {0x00};
    sendDynamicCommand(0x36, cls, subcls, 0x01, data, 1, name);
}

// Función para crear comando de escritura
void sendWriteCommand(uint8_t cls, uint8_t subcls, uint8_t value, String name = "") {
    uint8_t data[] = {value};
    sendDynamicCommand(0x36, cls, subcls, 0x00, data, 1, name);
}

// Función para crear comando de escritura con múltiples bytes
void sendWriteCommandMulti(uint8_t cls, uint8_t subcls, const uint8_t *data, uint8_t dataLen, String name = "") {
    sendDynamicCommand(0x36, cls, subcls, 0x00, data, dataLen, name);
}


void leerRespuestaDispositivo() {
    if (SerialDevice.available()) {
        Serial.print("Respuesta del dispositivo: ");
        while (SerialDevice.available()) {
            uint8_t byte = SerialDevice.read();
            Serial.print("0x");
            if (byte < 0x10) Serial.print("0");
            Serial.print(byte, HEX);
            Serial.print(" ");

            //---
            // Agregar al buffer como hex string
            if (byte < 0x10) responseBuffer += "0";
            responseBuffer += String(byte, HEX);
            responseBuffer += " ";
            lastByteTime = millis();
        }
        Serial.println();
    }

    // Si han pasado más de 100ms sin nuevos bytes, mostrar la respuesta completa
    if (responseBuffer.length() > 0 && (millis() - lastByteTime) > RESPONSE_TIMEOUT) {
        Serial.println("Respuesta completa del dispositivo: " + responseBuffer);
        responseBuffer = "";
    }
}

void ejecutarComandoNumerico(int numeroComando) {
    int index = numeroComando - 1; // Convertir a índice 0-based
    
    switch (currentMenu) {
        case READ_MENU:
            if (index >= 0 && index < readCommandsSize) {
                sendCommand(readCommands[index].data, readCommands[index].length, readCommands[index].name);
            } else {
                Serial.println("Número de comando inválido");
            }
            break;
            
        case WRITE_MENU:
            if (index >= 0 && index < writeCommandsSize) {
                sendCommand(writeCommands[index].data, writeCommands[index].length, writeCommands[index].name);
            } else {
                Serial.println("Número de comando inválido");
            }
            break;
            
        case READWRITE_MENU:
            if (index >= 0 && index < readWriteCommandsSize) {
                sendCommand(readWriteCommands[index].data, readWriteCommands[index].length, readWriteCommands[index].name);
            } else {
                Serial.println("Número de comando inválido");
            }
            break;
            
        case CURSOR_MENU:
            if (index >= 0 && index < cursorCommandsSize) {
                sendCommand(cursorCommands[index].data, cursorCommands[index].length, cursorCommands[index].name);
            } else {
                Serial.println("Número de comando inválido");
            }
            break;
            
        default:
            Serial.println("Error: menú no reconocido");
            break;
    }
}

void mostrarMenu() {
    currentMenu = MAIN_MENU;
    Serial.println("\n===== MENU PRINCIPAL =====");
    Serial.println("R - Comandos de Lectura");
    Serial.println("W - Comandos de Escritura");
    Serial.println("X - Comandos Lectura/Escritura");
    Serial.println("C - Comandos de Cursor");
    Serial.println("M - Mostrar este menú");
    Serial.println("========================");
    Serial.println("Ingrese su opción:");
}

void mostrarComandosLectura() {
    currentMenu = READ_MENU;
    Serial.println("\n=== COMANDOS DE LECTURA ===");
    for (int i = 0; i < readCommandsSize; i++) {
        Serial.print(i + 1);
        Serial.print(" - ");
        Serial.println(readCommands[i].name);
    }
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

void mostrarComandosEscritura() {
    currentMenu = WRITE_MENU;
    Serial.println("\n=== COMANDOS DE ESCRITURA ===");
    for (int i = 0; i < writeCommandsSize; i++) {
        Serial.print(i + 1);
        Serial.print(" - ");
        Serial.println(writeCommands[i].name);
    }
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

void mostrarComandosLecturaEscritura() {
    currentMenu = READWRITE_MENU;
    Serial.println("\n=== COMANDOS LECTURA/ESCRITURA ===");
    for (int i = 0; i < readWriteCommandsSize; i++) {
        Serial.print(i + 1);
        Serial.print(" - ");
        Serial.println(readWriteCommands[i].name);
    }
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

void mostrarComandosCursor() {
    currentMenu = CURSOR_MENU;
    Serial.println("\n=== COMANDOS DE CURSOR ===");
    for (int i = 0; i < cursorCommandsSize; i++) {
        Serial.print(i + 1);
        Serial.print(" - ");
        Serial.println(cursorCommands[i].name);
    }
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

// Agregar opción para comando personalizado
void procesarComando(String comando) {
    comando.trim();
    comando.toUpperCase();
    
    if (comando == "R") {
        mostrarComandosLectura();
    }
    else if (comando == "W") {
        mostrarComandosEscritura();
    }
    else if (comando == "X") {
        mostrarComandosLecturaEscritura();
    }
    else if (comando == "C") {
        mostrarComandosCursor();
    }
    else if (comando == "D") {
        // Comando dinámico de ejemplo
        Serial.println("Ejemplo: Enviando brillo 50 con checksum automático");
        sendWriteCommand(0x78, 0x02, 50, "Brillo 50 (dinámico)");
    }
    else if (comando == "M") {
        mostrarMenu();
    }
    else if (comando.toInt() > 0) {
        ejecutarComandoNumerico(comando.toInt());
    }
    else if (comando == "0") {
        mostrarMenu();
    }
    else {
        Serial.println("Opción no válida. Escriba 'M' para ver el menú.");
    }
}


void procesarBytesRespuesta() {
    static unsigned long lastByteTime = 0;
    const unsigned long BYTE_TIMEOUT = 50; // 50ms entre bytes
    
    if (SerialDevice.available()) {
        while (SerialDevice.available() && currentResponse.length < 256) {
            currentResponse.data[currentResponse.length] = SerialDevice.read();
            currentResponse.length++;
            lastByteTime = millis();
        }
    }
    
    // Marcar como completa si no hay más bytes por un tiempo
    if (currentResponse.length > 0 && 
        (millis() - lastByteTime) > BYTE_TIMEOUT && 
        !currentResponse.complete) {
        
        currentResponse.complete = true;
        mostrarRespuestaCompleta();
    }
}

// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------

void setup()
{
    // Inicializar UART principal para el menú
    Serial.begin(115200);
    
    // Inicializar UART2 para comunicación con dispositivo
    SerialDevice.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    
    delay(500);
    Serial.println("--> ESP32 Camera Terminal UART");
    Serial.println("UART Principal: Menú y control");
    Serial.println("UART2 (GPIO16/17): Comunicación con dispositivo");
    mostrarMenu();
}

// ----------------------------------------------------------------------------
// Main control loop
// ----------------------------------------------------------------------------

void loop()
{  
    // Procesar comandos del menú (UART principal)
    if (Serial.available() > 0) {
        char receivedChar = Serial.read();

        if (receivedChar == '\n') {
            if (receivedMessage.length() > 0) {
                procesarComando(receivedMessage);
            }
            receivedMessage = "";  
        } else if (receivedChar != '\r') {
            receivedMessage += receivedChar;
        }
    }
    
    // Leer respuestas del dispositivo (UART2)
    procesarBytesRespuesta();
}