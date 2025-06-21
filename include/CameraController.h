/*
█▀ █▄█ █▀▀ █░█ █▀▀ █░█
▄█ ░█░ █▄▄ █▀█ ██▄ ▀▄▀

Author: <Anton Sychev> (anton at sychev dot xyz)
CameraController.h (c) 2025
Created:  2025-06-21 01:36:27 
Desc: JS-MINI256-9 Thermal Camera Controller Class
*/

#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include <Arduino.h>
#include <HardwareSerial.h>

// Configuración de comunicación
#define UART_BAUDRATE 115200
#define MAX_RESPONSE_SIZE 256
#define RESPONSE_TIMEOUT 150  // 150ms según especificaciones del fabricante
#define BYTE_TIMEOUT 75       // 75ms entre bytes según especificaciones

// Códigos de protocolo
#define HEADER_BYTE 0xF0
#define FOOTER_BYTE 0xFF
#define DEVICE_ADDR 0x36

// Clases de comandos
#define CLASS_INFO 0x74
#define CLASS_CAMERA 0x7C
#define CLASS_IMAGE 0x78
#define CLASS_MIRROR 0x70

// Flags R/W
#define FLAG_WRITE 0x00
#define FLAG_READ 0x01

enum CameraStatus {
    CAMERA_INITIALIZING = 0x00,
    CAMERA_ACTIVE = 0x01,
    CAMERA_ERROR = 0xFF
};

enum ColorPalette {
    PALETTE_WHITE_HOT = 0x00,
    PALETTE_BLACK_HOT = 0x01,
    PALETTE_IRON = 0x02,
    PALETTE_RAINBOW = 0x03,
    PALETTE_RAIN = 0x04,
    PALETTE_ICE_FIRE = 0x05,
    PALETTE_FUSION = 0x06,
    PALETTE_SEPIA = 0x07,
    PALETTE_COLOR1 = 0x08,
    PALETTE_COLOR2 = 0x09,
    PALETTE_COLOR3 = 0x0A,
    PALETTE_COLOR4 = 0x0B,
    PALETTE_COLOR5 = 0x0C,
    PALETTE_COLOR6 = 0x0D,
    PALETTE_COLOR7 = 0x0E
};

enum AutoShutterMode {
    SHUTTER_DISABLED = 0x00,
    SHUTTER_MANUAL = 0x01,
    SHUTTER_AUTO = 0x02,
    SHUTTER_FULL_AUTO = 0x03
};

enum MirrorMode {
    MIRROR_DISABLED = 0x00,
    MIRROR_CENTRAL = 0x01,
    MIRROR_HORIZONTAL = 0x02,
    MIRROR_VERTICAL = 0x03
};

struct CameraInfo {
    String model;
    String fpgaVersion;
    String fpgaBuildDate;
    String softwareVersion;
    String softwareBuildDate;
    String calibrationVersion;
    String ispVersion;
    CameraStatus status;
};

struct Response {
    uint8_t data[MAX_RESPONSE_SIZE];
    size_t length;
    unsigned long timestamp;
    bool complete;
    bool valid;
};

class CameraController {
private:
    HardwareSerial* _serial;
    uint8_t _rxPin;
    uint8_t _txPin;
    Response _currentResponse;
    bool _debugEnabled;
    String _lastError;
    unsigned long _responseTimeout;
    unsigned long _byteTimeout;
    unsigned long _lastByteTime;
    
    // Funciones privadas de protocolo
    uint8_t calculateChecksum(uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen);
    void buildCommand(uint8_t *cmdBuffer, uint8_t device, uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen);
    bool sendCommand(uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data = nullptr, uint8_t dataLen = 0);
    bool commandExpectsResponse(uint8_t rw);
    void initializeResponse();
    bool waitForResponse(unsigned long timeout = RESPONSE_TIMEOUT);
    String interpretResponse();
    void processResponseBytes();
    
    // Interpretación detallada de respuestas
    String interpretInfoResponse(uint8_t subclass, const uint8_t* data, uint8_t dataLen, uint8_t rwFlag);
    String interpretImageResponse(uint8_t subclass, const uint8_t* data, uint8_t dataLen, uint8_t rwFlag);
    String interpretCameraResponse(uint8_t subclass, const uint8_t* data, uint8_t dataLen, uint8_t rwFlag);
    String interpretMirrorResponse(uint8_t subclass, const uint8_t* data, uint8_t dataLen, uint8_t rwFlag);
    
public:
    /**
     * Constructor de la clase CameraController.
     * @param serial Puerto serie para comunicación.
     * @param rxPin Pin RX del ESP32.
     * @param txPin Pin TX del ESP32.
     */
    CameraController(HardwareSerial* serial, uint8_t rxPin, uint8_t txPin);

    /**
     * Inicializa la comunicación con la cámara.
     * @return true si la inicialización fue exitosa, false en caso contrario.
     */
    bool begin();

    /**
     * Habilita o deshabilita el modo de depuración.
     * @param enable true para habilitar, false para deshabilitar.
     */
    void enableDebug(bool enable = true);

    /**
     * Configura los tiempos de espera para las respuestas y los bytes.
     * @param responseTimeout Tiempo de espera para la respuesta en milisegundos.
     * @param byteTimeout Tiempo de espera entre bytes en milisegundos.
     */
    void setTimeouts(unsigned long responseTimeout, unsigned long byteTimeout);

    /**
     * Procesa las respuestas asíncronas de la cámara.
     */
    void update(); // Procesar respuestas asíncronas
    
    /**
     * Obtiene información completa del dispositivo.
     * @param info Estructura CameraInfo donde se almacenará la información.
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool getDeviceInfo(CameraInfo& info);

    /**
     * Obtiene el modelo del dispositivo.
     * @return Modelo del dispositivo como una cadena.
     */
    String getModel();

    /**
     * Obtiene la versión del FPGA.
     * @return Versión del FPGA como una cadena.
     */
    String getFPGAVersion();

    /**
     * Obtiene la fecha de compilación del FPGA.
     * @return Fecha de compilación del FPGA como una cadena.
     */
    String getFPGABuildDate();

    /**
     * Obtiene la versión del software.
     * @return Versión del software como una cadena.
     */
    String getSoftwareVersion();

    /**
     * Obtiene la fecha de compilación del software.
     * @return Fecha de compilación del software como una cadena.
     */
    String getSoftwareBuildDate();

    /**
     * Obtiene la versión de calibración.
     * @return Versión de calibración como una cadena.
     */
    String getCalibrationVersion();

    /**
     * Obtiene la versión del ISP.
     * @return Versión del ISP como una cadena.
     */
    String getISPVersion();

    /**
     * Obtiene el estado actual de la cámara.
     * @return Estado de la cámara como CameraStatus.
     */
    CameraStatus getStatus();

    /**
     * Configura el brillo de la imagen.
     * @param value Valor de brillo (0-100).
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool setBrightness(uint8_t value);

    /**
     * Configura el contraste de la imagen.
     * @param value Valor de contraste (0-100).
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool setContrast(uint8_t value);

    /**
     * Configura la mejora digital de la imagen.
     * @param value Valor de mejora digital (0-100).
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool setDigitalEnhancement(uint8_t value);

    /**
     * Configura la reducción de ruido estático.
     * @param value Valor de reducción de ruido estático (0-100).
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool setStaticNoiseReduction(uint8_t value);

    /**
     * Configura la reducción de ruido dinámico.
     * @param value Valor de reducción de ruido dinámico (0-100).
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool setDynamicNoiseReduction(uint8_t value);

    /**
     * Configura la paleta de colores.
     * @param palette Paleta de colores a configurar.
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool setPalette(ColorPalette palette);

    /**
     * Configura el modo de espejo.
     * @param mode Modo de espejo a configurar.
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool setMirror(MirrorMode mode);
    
    // Lectura de configuración actual
    uint8_t getBrightness();
    uint8_t getContrast();
    uint8_t getDigitalEnhancement();
    uint8_t getStaticNoiseReduction();
    uint8_t getDynamicNoiseReduction();
    ColorPalette getCurrentPalette();
    MirrorMode getCurrentMirror();
    
    // Control de obturador
    bool setAutoShutter(AutoShutterMode mode);
    bool setShutterInterval(uint16_t minutes);
    bool performManualFFC();
    bool performBackgroundCorrection();
    bool performVignettingCorrection();
    AutoShutterMode getAutoShutterMode();
    uint16_t getShutterInterval();
    
    // Control de cursor
    bool showCursor();
    bool hideCursor();
    bool centerCursor();
    bool moveCursorUp(uint8_t pixels = 1);
    bool moveCursorDown(uint8_t pixels = 1);
    bool moveCursorLeft(uint8_t pixels = 1);
    bool moveCursorRight(uint8_t pixels = 1);
    bool addDeadPixel();
    bool removeDeadPixel();
    
    // Configuración del sistema
    bool saveConfiguration();
    bool restoreFactory();
    
    // Comandos dinámicos
    bool sendDynamicCommand(uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t *data, uint8_t dataLen, String name = "");
    bool sendRawCommand(const uint8_t *cmd, size_t len, String name = "");
    
    // Funciones de utilidad
    bool isConnected();
    String getLastError();
    
    // Callbacks - USAR FUNCIÓN GLOBAL ESTÁTICA
    typedef void (*ResponseCallback)(const String& interpretation);
    static void setGlobalResponseHandler(ResponseCallback callback);
    
    // Constantes públicas
    static const char* PALETTE_NAMES[];
    static const char* SHUTTER_MODE_NAMES[];
    static const char* MIRROR_MODE_NAMES[];
    
    // Métodos para comandos de lectura
    bool readDeviceModel();
    bool readFPGA_Version();
    bool readInitializationStatus();
    
    // Método público para pruebas de comandos
    void testBuildAndSendCommand(uint8_t cls, uint8_t subcls, uint8_t rw, const uint8_t* data, uint8_t dataLen);

private:
    static ResponseCallback _globalCallback;
};

#endif