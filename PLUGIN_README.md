# ThermalCameraController Plugin

Plugin para controlar cámaras térmicas JS-MINI256-9 con ESP32 via UART.

## Instalación

### Opción 1: Usando PlatformIO Library Manager
```bash
pio lib install "ThermalCameraController"
```

### Opción 2: Desde GitHub
Añade al archivo `platformio.ini`:
```ini
lib_deps = 
    https://github.com/klich3/Thermal-camera---JS-MINI256-9.git
```

### Opción 3: Local
Copia la carpeta del plugin a tu directorio `lib/` del proyecto.

## Uso Rápido

### Ejemplo Básico
```cpp
#include <CameraController.h>

HardwareSerial cameraSerial(2);
CameraController camera(&cameraSerial, 16, 17);

void setup() {
    Serial.begin(115200);
    camera.begin();
    
    // Configurar brillo
    camera.setBrightness(75);
    
    // Cambiar paleta
    camera.setPalette(PALETTE_RAINBOW);
}

void loop() {
    camera.update(); // Procesar respuestas asíncronas
}
```

### Ejemplo con Menú
```cpp
#include <CameraController.h>
#include <MenuSystem.h>

HardwareSerial cameraSerial(2);
CameraController camera(&cameraSerial, 16, 17);
MenuSystem menu(&camera);

void setup() {
    Serial.begin(115200);
    camera.begin();
    menu.begin();
}

void loop() {
    // Procesar entrada del usuario desde Serial
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        menu.processInput(input);
    }
    
    camera.update();
    menu.update();
}
```

## API Reference

### CameraController

#### Constructor
```cpp
CameraController(HardwareSerial* serial, uint8_t rxPin, uint8_t txPin)
```

#### Métodos Principales
- `bool begin()` - Inicializa la comunicación
- `void enableDebug(bool enable)` - Habilita/deshabilita debug
- `void update()` - Procesa respuestas asíncronas
- `bool setBrightness(uint8_t value)` - Configura brillo (0-100)
- `bool setContrast(uint8_t value)` - Configura contraste (0-100)
- `bool setPalette(ColorPalette palette)` - Cambia paleta de colores
- `String getModel()` - Obtiene modelo del dispositivo
- `CameraStatus getStatus()` - Obtiene estado de la cámara

### MenuSystem

#### Constructor
```cpp
MenuSystem(CameraController* camera)
```

#### Métodos Principales
- `void begin()` - Inicializa el sistema de menús
- `void processInput(String input)` - Procesa entrada del usuario
- `void update()` - Actualiza el sistema de menús
- `MenuState getCurrentMenu()` - Obtiene el menú actual

## Tests

Ejecutar tests unitarios:
```bash
pio test -e test
```

## Ejemplos

Los ejemplos se encuentran en la carpeta `examples/`:
- `BasicUsage` - Uso básico sin menús
- `MenuInterface` - Uso con sistema de menús interactivo

## Licencia

MIT License - Ver archivo LICENSE para más detalles.
