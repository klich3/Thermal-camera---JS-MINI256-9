/*
█▀ █▄█ █▀▀ █░█ █▀▀ █░█
▄█ ░█░ █▄▄ █▀█ ██▄ ▀▄▀

Author: <Anton Sychev> (anton at sychev dot xyz)
main.cpp (c) 2025
Created:  2025-06-21 01:36:27 
Desc: Control of camera module
Doc:

0xF0 0x05 0x36 0x78 0x20 0x00 0x03 0xD1 0xFF
 |    |    |    |    |    |    |    |    |
 |    |    |    |    |    |    |    |    +-- Footer (0xFF)
 |    |    |    |    |    |    |    +------- Checksum
 |    |    |    |    |    |    +------------ Data (0x03 = Rainbow)
 |    |    |    |    |    +----------------- R/W Flag (0x00 = write)
 |    |    |    |    +---------------------- Subclass (0x20 = paleta)
 |    |    |    +--------------------------- Class (0x78 = imagen)
 |    |    +-------------------------------- Device (0x36)
 |    +------------------------------------- Length (0x05 = 5 bytes después del length)
 +------------------------------------------ Header (0xF0)

*/

#include <Arduino.h>
#include <HardwareSerial.h>

// ============================================================================
// CONFIGURACIÓN Y CONSTANTES
// ============================================================================

// Configuración UART
HardwareSerial SerialDevice(2); // UART2
#define TX_PIN 17
#define RX_PIN 16
#define UART_BAUDRATE 115200

// Timeouts y constantes
const unsigned long RESPONSE_TIMEOUT = 100;  // 100ms timeout entre bytes
const unsigned long BYTE_TIMEOUT = 50;       // 50ms entre bytes
const size_t MAX_RESPONSE_SIZE = 256;

// ============================================================================
// ESTRUCTURAS Y TIPOS DE DATOS
// ============================================================================

enum MenuState {
    MAIN_MENU,
    READ_MENU,
    WRITE_MENU,
    READWRITE_MENU,
    CURSOR_MENU,
    PARAM_MENU  
};

struct Command {
    const char* name;
    uint8_t data[9];
    uint8_t length;
};

struct Response {
    uint8_t data[MAX_RESPONSE_SIZE];
    size_t length;
    unsigned long timestamp;
    bool complete;
};

struct ParameterCommand {
    const char* name;
    uint8_t device;
    uint8_t cls;
    uint8_t subcls;
    uint8_t minValue;
    uint8_t maxValue;
    uint8_t defaultValue;
    bool isTwoBytes;
    const char* description;
};

struct CursorParameterCommand {
    const char* name;
    uint8_t baseCode; // 0x20=arriba, 0x30=abajo, 0x40=izq, 0x50=der
    const char* direction;
};

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

MenuState currentMenu = MAIN_MENU;
Response currentResponse = {0};

// Variables de estado
bool waitingForParameter = false;
bool waitingForCursorParameter = false;
int selectedParamCommand = -1;
int selectedCursorCommand = -1;

String receivedMessage;
String responseBuffer = "";
unsigned long lastByteTime = 0;

// ============================================================================
// ARRAYS DE COMANDOS
// ============================================================================

// Comandos de lectura
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

// Comandos de escritura
Command writeCommands[] = {
    {"Guardar configuración", {0xF0, 0x05, 0x36, 0x74, 0x10, 0x00, 0x00, 0xBA, 0xFF}, 9},
    {"Restaurar fábrica", {0xF0, 0x05, 0x36, 0x74, 0x0F, 0x00, 0x00, 0xB9, 0xFF}, 9},
    {"Calibración obturador manual (FFC)", {0xF0, 0x05, 0x36, 0x7C, 0x02, 0x00, 0x00, 0xB4, 0xFF}, 9},
    {"Corrección de fondo manual", {0xF0, 0x05, 0x36, 0x7C, 0x03, 0x00, 0x00, 0xB5, 0xFF}, 9},
    {"Corrección viñeteado", {0xF0, 0x05, 0x36, 0x7C, 0x0C, 0x00, 0x02, 0xC0, 0xFF}, 9}
};

// Comandos lectura/escritura predefinidos
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

// Comandos parametrizables
ParameterCommand paramCommands[] = {
    {"Control automático obturador", 0x36, 0x7C, 0x04, 0x00, 0x03, 0x03, false, "0=Deshabilitado, 1=Manual, 2=Auto, 3=Total Auto"},
    {"Intervalo obturación automático", 0x36, 0x7C, 0x05, 0x00, 0xFF, 0x0A, true, "Minutos (2 bytes, default: 10)"},
    {"Brillo de imagen", 0x36, 0x78, 0x02, 0x00, 0x64, 0x32, false, "Rango 0-100, default: 50"},
    {"Contraste de imagen", 0x36, 0x78, 0x03, 0x00, 0x64, 0x32, false, "Rango 0-100, default: 50"},
    {"Mejora digital detalle", 0x36, 0x78, 0x10, 0x00, 0x64, 0x32, false, "Rango 0-100, default: 50"},
    {"Reducción ruido estático", 0x36, 0x78, 0x15, 0x00, 0x64, 0x32, false, "Rango 0-100, default: 50"},
    {"Reducción ruido dinámico", 0x36, 0x78, 0x16, 0x00, 0x64, 0x32, false, "Rango 0-100, default: 50"},
    {"Paleta imagen térmica", 0x36, 0x78, 0x20, 0x00, 0x0E, 0x00, false, "0-14 paletas, default: 0 (White Hot)"},
    {"Reflejo imagen (mirroring)", 0x36, 0x70, 0x11, 0x00, 0x03, 0x00, false, "0=Off, 1=Central, 2=Horizontal, 3=Vertical"}
};

// Comandos de cursor
Command cursorCommands[] = {
    {"Mostrar cursor", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x0F, 0xD7, 0xFF}, 9},
    {"Ocultar cursor", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x00, 0xC8, 0xFF}, 9},
    {"Mover cursor arriba", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x02, 0xCA, 0xFF}, 9},
    {"Mover cursor abajo", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x03, 0xCB, 0xFF}, 9},
    {"Mover cursor izquierda", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x04, 0xCC, 0xFF}, 9},
    {"Mover cursor derecha", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x05, 0xCD, 0xFF}, 9},
    {"Cursor al centro", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x06, 0xCE, 0xFF}, 9},
    {"Agregar píxel defectuoso", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x0D, 0xD5, 0xFF}, 9},
    {"Quitar píxel defectuoso", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x0E, 0xD6, 0xFF}, 9},
    
    // Comandos de movimiento múltiple
    {"Mover 2 píxeles arriba", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x22, 0xEA, 0xFF}, 9},
    {"Mover 5 píxeles arriba", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x25, 0xED, 0xFF}, 9},
    {"Mover 10 píxeles arriba", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x2A, 0xF2, 0xFF}, 9},
    {"Mover 2 píxeles abajo", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x32, 0xFA, 0xFF}, 9},
    {"Mover 5 píxeles abajo", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x35, 0xFD, 0xFF}, 9},
    {"Mover 10 píxeles abajo", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x3A, 0x02, 0xFF}, 9},
    {"Mover 2 píxeles izquierda", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x42, 0x0A, 0xFF}, 9},
    {"Mover 5 píxeles izquierda", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x45, 0x0D, 0xFF}, 9},
    {"Mover 10 píxeles izquierda", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x4A, 0x12, 0xFF}, 9},
    {"Mover 2 píxeles derecha", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x52, 0x1A, 0xFF}, 9},
    {"Mover 5 píxeles derecha", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x55, 0x1D, 0xFF}, 9},
    {"Mover 10 píxeles derecha", {0xF0, 0x05, 0x36, 0x78, 0x1A, 0x00, 0x5A, 0x22, 0xFF}, 9}
};

// Comandos parametrizados de cursor
CursorParameterCommand cursorParamCommands[] = {
    {"Mover N píxeles arriba", 0x20, "arriba"},
    {"Mover N píxeles abajo", 0x30, "abajo"},
    {"Mover N píxeles izquierda", 0x40, "izquierda"},
    {"Mover N píxeles derecha", 0x50, "derecha"}
};

// Tamaños de arrays
const int readCommandsSize = sizeof(readCommands) / sizeof(readCommands[0]);
const int writeCommandsSize = sizeof(writeCommands) / sizeof(writeCommands[0]);
const int readWriteCommandsSize = sizeof(readWriteCommands) / sizeof(readWriteCommands[0]);
const int cursorCommandsSize = sizeof(cursorCommands) / sizeof(cursorCommands[0]);
const int paramCommandsSize = sizeof(paramCommands) / sizeof(paramCommands[0]);
const int cursorParamCommandsSize = sizeof(cursorParamCommands) / sizeof(cursorParamCommands[0]);

// ============================================================================
// DECLARACIONES DE FUNCIONES
// ============================================================================

// Funciones de protocolo
uint8_t calcCHK(uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen);
void buildCommand(uint8_t *cmdBuffer, uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen);
void sendCommand(const uint8_t *cmd, size_t len, String name = "");
void sendDynamicCommand(uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen, String name = "");

// Funciones de respuesta
void inicializarRespuesta();
void mostrarRespuestaCompleta();
void interpretarRespuesta(uint8_t* resp, size_t size);
void procesarBytesRespuesta();

// Funciones de menú
void mostrarMenu();
void mostrarComandosLectura();
void mostrarComandosEscritura();
void mostrarComandosLecturaEscritura();
void mostrarComandosCursor();
void mostrarComandosParametros();

// Funciones de procesamiento
void procesarComando(String comando);
void ejecutarComandoNumerico(int numeroComando);
void procesarParametro(String input);
void procesarParametroCursor(String input);

// Funciones de parámetros
void solicitarParametro(int commandIndex);
void leerParametroActual(int commandIndex);
void solicitarParametroCursor(int commandIndex);

void mostrarPaletas();
void seleccionarPaleta();
void procesarSeleccionPaleta(String input);

bool waitingForPalette = false;

// ============================================================================
// IMPLEMENTACIÓN DE FUNCIONES DE PROTOCOLO
// ============================================================================

uint8_t calcCHK(uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen) {
    uint16_t sum = device + cls + subcls + rw;
    for (uint8_t i = 0; i < dataLen; i++) {
        sum += data[i];
    }
    return sum & 0xFF;
}

void buildCommand(uint8_t *cmdBuffer, uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen) {
    cmdBuffer[0] = 0xF0;                    // Header
    cmdBuffer[1] = 0x05 + dataLen;          // Length (bytes después del length field)
    cmdBuffer[2] = device;                  // Device Address
    cmdBuffer[3] = cls;                     // Class Address
    cmdBuffer[4] = subcls;                  // Subclass Address
    cmdBuffer[5] = rw;                      // R/W Flag
    
    // Copiar datos directamente después de RW (SIN byte de longitud de datos)
    for (int i = 0; i < dataLen; i++) {
        cmdBuffer[6 + i] = data[i];
    }
    
    // Calcular y agregar checksum
    cmdBuffer[6 + dataLen] = calcCHK(device, cls, subcls, rw, data, dataLen);
    cmdBuffer[7 + dataLen] = 0xFF;          // Footer
}

void sendCommand(const uint8_t *cmd, size_t len, String name) {
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
    
    inicializarRespuesta();
    SerialDevice.write(cmd, len);
    Serial.println("Comando enviado por UART2");
}

void sendDynamicCommand(uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen, String name) {
    uint8_t cmdBuffer[16];  // Buffer suficiente
    buildCommand(cmdBuffer, device, cls, subcls, rw, data, dataLen);
    
    uint8_t totalLen = 8 + dataLen;  // CORREGIR: Header(1) + Length(1) + Device(1) + Cls(1) + Subcls(1) + RW(1) + Data(dataLen) + CHK(1) + Footer(1)
    
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
    
    inicializarRespuesta();
    SerialDevice.write(cmdBuffer, totalLen); 
    Serial.println("Comando enviado por UART2");
}

// ============================================================================
// PALETAS DE COLORES
// ============================================================================

void mostrarPaletas() {
    Serial.println("\n=== SELECCIONAR PALETA DE COLORES ===");
    Serial.println("1  - White Hot (Blanco caliente) - Default");
    Serial.println("2  - Black Hot (Negro caliente)");
    Serial.println("3  - Iron (Estilo hierro fundido)");
    Serial.println("4  - Rainbow (Arco iris)");
    Serial.println("5  - Rain (Gradientes azul-verde)");
    Serial.println("6  - Ice Fire (Hielo-fuego)");
    Serial.println("7  - Fusion (Fusión de color)");
    Serial.println("8  - Sepia (Tono marrón clásico)");
    Serial.println("9  - Color1 (Paleta de usuario 1)");
    Serial.println("10 - Color2 (Paleta de usuario 2)");
    Serial.println("11 - Color3 (Paleta de usuario 3)");
    Serial.println("12 - Color4 (Paleta de usuario 4)");
    Serial.println("13 - Color5 (Paleta de usuario 5)");
    Serial.println("14 - Color6 (Paleta de usuario 6)");
    Serial.println("15 - Color7 (Paleta de usuario 7)");
    Serial.println("0  - Volver al menú principal");
    Serial.println("\nIngrese número de paleta (1-15):");
}

void seleccionarPaleta() {
    waitingForPalette = true;
    mostrarPaletas();
}

void procesarSeleccionPaleta(String input) {
    input.trim();
    int paleta = input.toInt();
    
    if (paleta == 0) {
        // Volver al menú principal
        waitingForPalette = false;
        mostrarMenu();
        return;
    }
    
    if (paleta < 1 || paleta > 15) {
        Serial.println("Error: Seleccione un número entre 1 y 15");
        Serial.println("Ingrese número de paleta (1-15) o 0 para volver:");
        return;
    }
    
    // Convertir número de menú (1-15) a valor hex (0x00-0x0E)
    uint8_t valorHex = paleta - 1;
    
    // Nombres de paletas para mostrar confirmación
    const char* nombresPaletas[] = {
        "White Hot", "Black Hot", "Iron", "Rainbow", "Rain", 
        "Ice Fire", "Fusion", "Sepia", "Color1", "Color2", 
        "Color3", "Color4", "Color5", "Color6", "Color7"
    };
    
    uint8_t data[1] = {valorHex};
    String commandName = "Paleta: " + String(nombresPaletas[valorHex]) + " (0x" + String(valorHex, HEX) + ")";
    
    sendDynamicCommand(0x36, 0x78, 0x20, 0x00, data, 1, commandName);
    
    waitingForPalette = false;
    
    Serial.println("\n¿Desea seleccionar otra paleta? (S/N)");
    Serial.println("O presione 'M' para volver al menú principal");
}

// ============================================================================
// IMPLEMENTACIÓN DE FUNCIONES DE RESPUESTA
// ============================================================================

void inicializarRespuesta() {
    currentResponse.length = 0;
    currentResponse.timestamp = millis();
    currentResponse.complete = false;
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
    
    interpretarRespuesta(currentResponse.data, currentResponse.length);
    inicializarRespuesta();
}

void interpretarRespuesta(uint8_t* resp, size_t size) {
    if (size < 7) return;

    // ESTRUCTURA CORRECTA DE RESPUESTA
    uint8_t header = resp[0];      // 0xF0
    uint8_t length = resp[1];      // 0x05
    uint8_t device = resp[2];      // 0x36
    uint8_t classByte = resp[3];   // 0x78
    uint8_t subClassByte = resp[4]; // 0x20
    uint8_t rwFlag = resp[5];      // 0x03
    uint8_t dataLength = resp[6];  // 0x01
    // Datos empiezan en resp[7]
    // Para paleta: resp[7] = valor de paleta
    
    Serial.println("\n=== INTERPRETACIÓN DE RESPUESTA ===");
    Serial.printf("📋 Estructura: H=0x%02X L=0x%02X D=0x%02X C=0x%02X S=0x%02X R=0x%02X DL=0x%02X\n", 
                  header, length, device, classByte, subClassByte, rwFlag, dataLength);


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
                
            case 0x03: // Versión FPGA
                if (dataLength >= 3) {
                    Serial.printf("🔧 Versión FPGA: %d.%d.%d\n", resp[7], resp[8], resp[9]);
                }
                break;
                
            case 0x04: // Compilación FPGA
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    Serial.printf("📅 Compilación FPGA: %04lu-%02lu-%02lu\n", 
                                fecha / 10000, (fecha / 100) % 100, fecha % 100);
                }
                break;
                
            case 0x05: // Versión software
                if (dataLength >= 3) {
                    Serial.printf("💾 Versión software: %d.%d.%d\n", resp[7], resp[8], resp[9]);
                }
                break;
                
            case 0x06: // Compilación software
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    Serial.printf("📅 Compilación software: %04lu-%02lu-%02lu\n", 
                                fecha / 10000, (fecha / 100) % 100, fecha % 100);
                }
                break;
                
            case 0x0B: // Versión calibración cámara
                if (dataLength >= 4) {
                    uint32_t fecha = (resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | resp[10];
                    Serial.printf("📷 Versión calibración cámara: %04lu-%02lu-%02lu\n", 
                                fecha / 10000, (fecha / 100) % 100, fecha % 100);
                }
                break;
                
            case 0x0C: // Versión parámetros ISP
                if (dataLength >= 4) {
                    Serial.printf("⚙️  Versión parámetros ISP: 0x%02X%02X%02X%02X\n", 
                                resp[7], resp[8], resp[9], resp[10]);
                }
                break;
                
            case 0x10: // Guardar configuración
                Serial.println("💾 Configuración guardada correctamente");
                break;
                
            case 0x0F: // Restaurar fábrica
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
                
            case 0x02: // Calibración obturador manual
                Serial.println("🎯 Calibración de obturador manual ejecutada");
                break;
                
            case 0x03: // Corrección de fondo manual
                Serial.println("🎨 Corrección de fondo manual ejecutada");
                break;
                
            case 0x04: // Control auto obturador
                if (dataLength >= 1) {
                    const char* modos[] = {"Deshabilitado", "Manual", "Automático", "Totalmente automático"};
                    int modo = resp[7];
                    if (modo <= 3) {
                        if (rwFlag == 0x01) {
                            Serial.printf("📷 Obturador automático actual: %s (valor: %d)\n", modos[modo], modo);
                        } else {
                            Serial.printf("📷 Obturador automático configurado: %s\n", modos[modo]);
                        }
                    } else {
                        Serial.printf("📷 Modo obturador: 0x%02X\n", resp[7]);
                    }
                }
                break;
                
            case 0x05: // Intervalo obturación automático
                if (dataLength >= 2) {
                    uint16_t minutos = (resp[7] << 8) | resp[8];
                    if (rwFlag == 0x01) {
                        Serial.printf("⏱️  Intervalo obturación automático actual: %d minutos\n", minutos);
                    } else {
                        Serial.printf("⏱️  Intervalo obturación automático configurado: %d minutos\n", minutos);
                    }
                } else if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        Serial.printf("⏱️  Intervalo obturación automático actual: %d minutos\n", resp[7]);
                    } else {
                        Serial.printf("⏱️  Intervalo obturación automático configurado: %d minutos\n", resp[7]);
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
                    if (rwFlag == 0x01) {
                        Serial.printf("☀️  Brillo actual: %d/100\n", resp[7]);
                    } else {
                        Serial.printf("☀️  Brillo configurado: %d/100\n", resp[7]);
                    }
                }
                break;
                
            case 0x03: // Contraste
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        Serial.printf("🌗 Contraste actual: %d/100\n", resp[7]);
                    } else {
                        Serial.printf("🌗 Contraste configurado: %d/100\n", resp[7]);
                    }
                }
                break;
                
            case 0x10: // Mejora digital de detalle
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        Serial.printf("🔍 Mejora digital detalle actual: %d/100\n", resp[7]);
                    } else {
                        Serial.printf("🔍 Mejora digital detalle configurado: %d/100\n", resp[7]);
                    }
                }
                break;
                
            case 0x15: // Denoising estático
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        Serial.printf("🔇 Reducción ruido estático actual: %d/100\n", resp[7]);
                    } else {
                        Serial.printf("🔇 Reducción ruido estático configurado: %d/100\n", resp[7]);
                    }
                }
                break;
                
            case 0x16: // Denoising dinámico
                if (dataLength >= 1) {
                    if (rwFlag == 0x01) {
                        Serial.printf("🔊 Reducción ruido dinámico actual: %d/100\n", resp[7]);
                    } else {
                        Serial.printf("🔊 Reducción ruido dinámico configurado: %d/100\n", resp[7]);
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
                    
                    // ANÁLISIS DE LA RESPUESTA CORRECTA:
                    // 0xF0 0x05 0x36 0x78 0x20 0x03 0x01 0xD2 0xFF
                    //  |    |    |    |    |    |    |    |    |
                    //  |    |    |    |    |    |    |    |    +-- Footer (0xFF)
                    //  |    |    |    |    |    |    |    +------- Checksum (0xD2)
                    //  |    |    |    |    |    |    +------------ DATA (0x01 = Black Hot)
                    //  |    |    |    |    |    +----------------- R/W Flag (0x03)
                    //  |    |    |    |    +---------------------- Subclass (0x20)
                    //  |    |    |    +--------------------------- Class (0x78)
                    //  |    |    +-------------------------------- Device (0x36)
                    //  |    +------------------------------------- Length (0x05)
                    //  +------------------------------------------ Header (0xF0)
                    
                    // El problema es que dataLength = 0x01, pero el dato real está en posición 6
                    int paleta = resp[6];  // CAMBIAR: resp[6] contiene el valor real (0x01)
                    Serial.printf("🔍 Debug: Valor leído en resp[6] = 0x%02X (%d)\n", paleta, paleta);

                    if (paleta <= 14) {
                        if (rwFlag == 0x01) {
                            Serial.printf("🎨 Paleta actual: %s (valor: 0x%02X)\n", paletas[paleta], paleta);
                        } else {
                            Serial.printf("🎨 Paleta configurada: %s (valor: 0x%02X)\n", paletas[paleta], paleta);
                        }
                    } else {
                        Serial.printf("🎨 Paleta: Modo desconocido 0x%02X\n", paleta);
                    }
                }
                break;
                
            case 0x1A: // Comandos de cursor
                if (dataLength >= 1) {
                    uint8_t cursorData = resp[7];
                    
                    switch (cursorData) {
                        case 0x00: Serial.println("👁️‍🗨️ Cursor ocultado"); break;
                        case 0x02: Serial.println("⬆️  Cursor movido arriba"); break;
                        case 0x03: Serial.println("⬇️  Cursor movido abajo"); break;
                        case 0x04: Serial.println("⬅️  Cursor movido izquierda"); break;
                        case 0x05: Serial.println("➡️  Cursor movido derecha"); break;
                        case 0x06: Serial.println("🎯 Cursor centrado"); break;
                        case 0x0D: Serial.println("❌ Píxel defectuoso agregado"); break;
                        case 0x0E: Serial.println("✅ Píxel defectuoso removido"); break;
                        case 0x0F: Serial.println("👁️  Cursor mostrado"); break;
                        default:
                            // Comandos de movimiento múltiple
                            if ((cursorData & 0xF0) == 0x20) {
                                Serial.printf("⬆️  Cursor movido %d píxeles arriba\n", cursorData & 0x0F);
                            } else if ((cursorData & 0xF0) == 0x30) {
                                Serial.printf("⬇️  Cursor movido %d píxeles abajo\n", cursorData & 0x0F);
                            } else if ((cursorData & 0xF0) == 0x40) {
                                Serial.printf("⬅️  Cursor movido %d píxeles izquierda\n", cursorData & 0x0F);
                            } else if ((cursorData & 0xF0) == 0x50) {
                                Serial.printf("➡️  Cursor movido %d píxeles derecha\n", cursorData & 0x0F);
                            } else {
                                Serial.printf("🖱️  Comando cursor: 0x%02X\n", cursorData);
                            }
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
                    const char* modos[] = {"Deshabilitado", "Central", "Horizontal", "Vertical"};
                    int modo = resp[7];
                    if (modo <= 3) {
                        if (rwFlag == 0x01) {
                            Serial.printf("🪞 Mirroring actual: %s (valor: %d)\n", modos[modo], modo);
                        } else {
                            Serial.printf("🪞 Mirroring configurado: %s\n", modos[modo]);
                        }
                    } else {
                        Serial.printf("🪞 Mirroring: Modo %d\n", resp[7]);
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

void procesarBytesRespuesta() {
    static unsigned long lastByteTime = 0;
    
    if (SerialDevice.available()) {
        while (SerialDevice.available() && currentResponse.length < MAX_RESPONSE_SIZE) {
            currentResponse.data[currentResponse.length] = SerialDevice.read();
            currentResponse.length++;
            lastByteTime = millis();
        }
    }
    
    if (currentResponse.length > 0 && 
        (millis() - lastByteTime) > BYTE_TIMEOUT && 
        !currentResponse.complete) {
        
        currentResponse.complete = true;
        mostrarRespuestaCompleta();
    }
}

// ============================================================================
// IMPLEMENTACIÓN DE FUNCIONES DE MENÚ
// ============================================================================

void mostrarMenu() {
    currentMenu = MAIN_MENU;
    Serial.println("\n===== MENU PRINCIPAL =====");
    Serial.println("R - Comandos de Lectura");
    Serial.println("W - Comandos de Escritura");
    Serial.println("X - Comandos Lectura/Escritura (fijos)");
    Serial.println("P - Comandos con Parámetros");
    Serial.println("C - Comandos de Cursor");
    Serial.println("PAL - Seleccionar Paleta de Colores");  // NUEVA LÍNEA
    Serial.println("D - Comando dinámico (ejemplo)");
    Serial.println("M - Mostrar este menú");
    Serial.println("========================");
    Serial.println("Ingrese su opción:");
}

void mostrarComandosLectura() {
    currentMenu = READ_MENU;
    Serial.println("\n=== COMANDOS DE LECTURA ===");
    for (int i = 0; i < readCommandsSize; i++) {
        Serial.printf("%d - %s\n", i + 1, readCommands[i].name);
    }
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

void mostrarComandosEscritura() {
    currentMenu = WRITE_MENU;
    Serial.println("\n=== COMANDOS DE ESCRITURA ===");
    for (int i = 0; i < writeCommandsSize; i++) {
        Serial.printf("%d - %s\n", i + 1, writeCommands[i].name);
    }
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

void mostrarComandosLecturaEscritura() {
    currentMenu = READWRITE_MENU;
    Serial.println("\n=== COMANDOS LECTURA/ESCRITURA ===");
    for (int i = 0; i < readWriteCommandsSize; i++) {
        Serial.printf("%d - %s\n", i + 1, readWriteCommands[i].name);
    }
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

void mostrarComandosCursor() {
    currentMenu = CURSOR_MENU;
    Serial.println("\n=== COMANDOS DE CURSOR ===");
    
    Serial.println("--- Comandos básicos ---");
    for (int i = 0; i < 9; i++) {
        Serial.printf("%d - %s\n", i + 1, cursorCommands[i].name);
    }
    
    Serial.println("\n--- Comandos de movimiento múltiple ---");
    for (int i = 9; i < cursorCommandsSize; i++) {
        Serial.printf("%d - %s\n", i + 1, cursorCommands[i].name);
    }
    
    Serial.println("\n--- Comandos parametrizados ---");
    for (int i = 0; i < cursorParamCommandsSize; i++) {
        Serial.printf("%d - %s (%s)\n", cursorCommandsSize + i + 1, 
                     cursorParamCommands[i].name, cursorParamCommands[i].direction);
    }
    
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

void mostrarComandosParametros() {
    currentMenu = PARAM_MENU;  
    Serial.println("\n=== COMANDOS CON PARÁMETROS ===");
    Serial.println("--- Escribir valores ---");
    for (int i = 0; i < paramCommandsSize; i++) {
        Serial.printf("%d - %s (%s)\n", i + 1, paramCommands[i].name, paramCommands[i].description);
    }
    Serial.println("\n--- Leer valores actuales ---");
    for (int i = 0; i < paramCommandsSize; i++) {
        Serial.printf("%d - Leer %s\n", i + 10 + 1, paramCommands[i].name);
    }
    Serial.println("0 - Volver al menú principal");
    Serial.println("Ingrese número del comando:");
}

// ============================================================================
// IMPLEMENTACIÓN DE FUNCIONES DE PROCESAMIENTO
// ============================================================================

void ejecutarComandoNumerico(int numeroComando) {
    int index = numeroComando - 1;
    
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
            
        case PARAM_MENU:
            if (index >= 0 && index < paramCommandsSize) {
                // ESCRIBIR PARÁMETRO - DIRECTAMENTE SOLICITAR EL VALOR
                solicitarParametro(index);
            } else if (numeroComando >= 11 && numeroComando <= (10 + paramCommandsSize)) {
                // LEER PARÁMETRO - Opciones 11-19 para leer (numeroComando 11-19)
                int readIndex = numeroComando - 11;
                leerParametroActual(readIndex);
            } else {
                Serial.println("Número de comando inválido");
            }
            break;
            
        case CURSOR_MENU:
            if (index >= 0 && index < cursorCommandsSize) {
                sendCommand(cursorCommands[index].data, cursorCommands[index].length, cursorCommands[index].name);
            } else if (index >= cursorCommandsSize && index < (cursorCommandsSize + cursorParamCommandsSize)) {
                int paramIndex = index - cursorCommandsSize;
                solicitarParametroCursor(paramIndex);
            } else {
                Serial.println("Número de comando inválido");
            }
            break;
            
        default:
            Serial.println("Error: menú no reconocido");
            break;
    }
}


void procesarComando(String comando) {
    comando.trim();
    comando.toUpperCase();
    
    // Procesar selección de paleta pendiente
    if (waitingForPalette) {
        // Permitir comandos especiales durante selección de paleta
        if (comando == "S" || comando == "SI") {
            mostrarPaletas();
            return;
        } else if (comando == "N" || comando == "NO" || comando == "M") {
            waitingForPalette = false;
            mostrarMenu();
            return;
        } else {
            procesarSeleccionPaleta(comando);
            return;
        }
    }
    
    // Procesar parámetros pendientes
    if (waitingForCursorParameter) {
        procesarParametroCursor(comando);
        return;
    }
    
    if (waitingForParameter) {
        procesarParametro(comando);
        return;
    }
    
    // Procesar comandos de menú
    if (comando == "R") {
        mostrarComandosLectura();
    }
    else if (comando == "W") {
        mostrarComandosEscritura();
    }
    else if (comando == "X") {
        mostrarComandosLecturaEscritura();
    }
    else if (comando == "P") {
        mostrarComandosParametros();
    }
    else if (comando == "C") {
        mostrarComandosCursor();
    }
    else if (comando == "PAL" || comando == "PALETA") {  // NUEVO COMANDO
        seleccionarPaleta();
    }
    else if (comando == "D") {
        Serial.println("Ejemplo: Enviando brillo 50 con checksum automático");
        uint8_t data[] = {50};
        sendDynamicCommand(0x36, 0x78, 0x02, 0x00, data, 1, "Brillo 50 (dinámico)");
    }
    else if (comando == "M") {
        mostrarMenu();
    }
    else if (comando.toInt() > 0) {
        int numeroComando = comando.toInt();
        ejecutarComandoNumerico(numeroComando); 
    }
    else if (comando == "0") {
        mostrarMenu();
        waitingForParameter = false;
        waitingForCursorParameter = false;
        waitingForPalette = false;  // AGREGAR ESTA LÍNEA
        selectedParamCommand = -1;
        selectedCursorCommand = -1;
    }
    else {
        Serial.println("Opción no válida. Escriba 'M' para ver el menú.");
    }
}

// ============================================================================
// IMPLEMENTACIÓN DE FUNCIONES DE PARÁMETROS
// ============================================================================

void solicitarParametro(int commandIndex) {
    if (commandIndex < 0 || commandIndex >= paramCommandsSize) {
        Serial.println("Comando inválido");
        return;
    }
    
    selectedParamCommand = commandIndex;
    waitingForParameter = true;
    
    ParameterCommand &cmd = paramCommands[commandIndex];
    
    Serial.println("\n=== CONFIGURAR PARÁMETRO ===");
    Serial.println("Comando: " + String(cmd.name));
    Serial.println("Descripción: " + String(cmd.description));
    
    if (cmd.isTwoBytes) {
        Serial.printf("Rango válido: 0-65535 (2 bytes)\n");
        Serial.printf("Valor por defecto: %d\n", cmd.defaultValue);
        Serial.println("Ingrese valor (0-65535) o 'D' para usar default:");
    } else {
        Serial.printf("Rango válido: %d-%d\n", cmd.minValue, cmd.maxValue);
        Serial.printf("Valor por defecto: %d\n", cmd.defaultValue);
        Serial.println("Ingrese valor o 'D' para usar default:");
    }
}

void procesarParametro(String input) {
    if (selectedParamCommand < 0 || selectedParamCommand >= paramCommandsSize) {
        Serial.println("Error: comando no válido");
        waitingForParameter = false;
        return;
    }
    
    ParameterCommand &cmd = paramCommands[selectedParamCommand];
    input.trim();
    input.toUpperCase();
    
    uint16_t valor;
    
    if (input == "D" || input == "DEFAULT") {
        valor = cmd.defaultValue;
        Serial.println("Usando valor por defecto: " + String(valor));
    } else {
        valor = input.toInt();
        
        if (cmd.isTwoBytes) {
            if (valor > 65535) {
                Serial.println("Error: Valor fuera de rango (0-65535)");
                waitingForParameter = false;
                return;
            }
        } else {
            if (valor < cmd.minValue || valor > cmd.maxValue) {
                Serial.printf("Error: Valor fuera de rango (%d-%d)\n", cmd.minValue, cmd.maxValue);
                waitingForParameter = false;
                return;
            }
        }
    }
    
    if (cmd.isTwoBytes) {
        uint8_t data[2];
        data[0] = (valor >> 8) & 0xFF;
        data[1] = valor & 0xFF;
        sendDynamicCommand(cmd.device, cmd.cls, cmd.subcls, 0x00, data, 2, 
                          String(cmd.name) + " = " + String(valor));
    } else {
        uint8_t data[1];
        data[0] = valor & 0xFF;
        sendDynamicCommand(cmd.device, cmd.cls, cmd.subcls, 0x00, data, 1, 
                          String(cmd.name) + " = " + String(valor));
    }
    
    waitingForParameter = false;
    selectedParamCommand = -1;
}

void leerParametroActual(int commandIndex) {
    if (commandIndex < 0 || commandIndex >= paramCommandsSize) {
        Serial.println("Comando inválido");
        return;
    }
    
    ParameterCommand &cmd = paramCommands[commandIndex];
    uint8_t data[] = {0x00};
    sendDynamicCommand(cmd.device, cmd.cls, cmd.subcls, 0x01, data, 1, 
                      "Leer " + String(cmd.name));
}

void solicitarParametroCursor(int commandIndex) {
    if (commandIndex < 0 || commandIndex >= cursorParamCommandsSize) {
        Serial.println("Comando inválido");
        return;
    }
    
    selectedCursorCommand = commandIndex;
    waitingForCursorParameter = true;
    
    CursorParameterCommand &cmd = cursorParamCommands[commandIndex];
    
    Serial.println("\n=== MOVER CURSOR PERSONALIZADO ===");
    Serial.println("Comando: " + String(cmd.name));
    Serial.println("Dirección: " + String(cmd.direction));
    Serial.println("Rango válido: 1-15 píxeles");
    Serial.println("Ingrese número de píxeles (1-15):");
}

void procesarParametroCursor(String input) {
    if (selectedCursorCommand < 0 || selectedCursorCommand >= cursorParamCommandsSize) {
        Serial.println("Error: comando no válido");
        waitingForCursorParameter = false;
        return;
    }
    
    CursorParameterCommand &cmd = cursorParamCommands[selectedCursorCommand];
    input.trim();
    
    int pixeles = input.toInt();
    
    if (pixeles < 1 || pixeles > 15) {
        Serial.println("Error: Número de píxeles fuera de rango (1-15)");
        waitingForCursorParameter = false;
        selectedCursorCommand = -1;
        return;
    }
    
    uint8_t dataValue = cmd.baseCode | (pixeles & 0x0F);
    uint8_t data[1] = {dataValue};
    
    String commandName = "Mover " + String(pixeles) + " píxeles " + String(cmd.direction);
    sendDynamicCommand(0x36, 0x78, 0x1A, 0x00, data, 1, commandName);
    
    waitingForCursorParameter = false;
    selectedCursorCommand = -1;
}

// ============================================================================
// FUNCIONES PRINCIPALES
// ============================================================================

void setup() {
    Serial.begin(UART_BAUDRATE);
    SerialDevice.begin(UART_BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN);
    
    delay(500);
    Serial.println("--> ESP32 Camera Terminal UART");
    Serial.println("UART Principal: Menú y control");
    Serial.println("UART2 (GPIO16/17): Comunicación con dispositivo");
    mostrarMenu();
}

void loop() {
    // Procesar comandos del menú
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
    
    // Procesar respuestas del dispositivo
    procesarBytesRespuesta();
}