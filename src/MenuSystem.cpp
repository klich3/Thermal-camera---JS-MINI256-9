/*
█▀ █▄█ █▀▀ █░█ █▀▀ █░█
▄█ ░█░ █▄▄ █▀█ ██▄ ▀▄▀

Author: <Anton Sychev> (anton at sychev dot xyz)
MenuSystem.cpp (c) 2025
Created:  2025-06-21 01:36:27 
Desc: Interactive Menu System Implementation
*/

#include "MenuSystem.h"

// Declarar función global para callback
void globalCameraResponseHandler(const String& response);

/**
 * Callback global para manejar respuestas de la cámara.
 * @param response Respuesta interpretada de la cámara.
 */
void globalCameraResponseHandler(const String& response) {
    Serial.println("📡 " + response);
}

MenuSystem::MenuSystem(CameraController* camera) 
    : _camera(camera), _currentMenu(MAIN_MENU), _waitingForInput(false), _inputHandler(nullptr) {
}

/**
 * Inicializa el sistema de menús y muestra el menú principal.
 */
void MenuSystem::begin() {
    String separator = "";
    for (int i = 0; i < 50; i++) {
        separator += "=";
    }
    
    Serial.println("\n" + separator);
    Serial.println("🎥 JS-MINI256-9 Thermal Camera Controller");
    Serial.println("📡 ESP32 UART Interface");
    Serial.println(separator);
    
    CameraController::setGlobalResponseHandler(globalCameraResponseHandler);
    
    showMainMenu();
}

/**
 * Procesa la entrada del usuario y ejecuta la acción correspondiente.
 * @param input Entrada del usuario.
 */
void MenuSystem::processInput(String input) {
    input.trim();
    input.toUpperCase();
    
    if (_waitingForInput && _inputHandler) {
        _waitingForInput = false;
        (this->*_inputHandler)(input);
        _inputHandler = nullptr;
        return;
    }
    
    if (input == "M" || input == "MENU") {
        returnToMainMenu();
        return;
    }
    
    if (input == "0" || input == "BACK") {
        returnToMainMenu();
        return;
    }
    
    int choice = input.toInt();
    if (choice > 0) {
        executeMenuChoice(choice);
    } else {
        printError("Invalid option. Type 'M' for main menu.");
    }
}

/**
 * Muestra el menú principal.
 */
void MenuSystem::showMainMenu() {
    _currentMenu = MAIN_MENU;
    printMenuHeader("MAIN MENU");
    printMenuItem(1, "Device Information", "Read device model, versions, status");
    printMenuItem(2, "Image Settings", "Brightness, contrast, enhancement");
    printMenuItem(3, "Camera Control", "Shutter, calibration, corrections");
    printMenuItem(4, "Cursor Control", "Show, move, dead pixel management");
    printMenuItem(5, "Color Palettes", "Select thermal color palettes");
    printMenuItem(6, "System Settings", "Save, restore, configuration");
    printSeparator();
    Serial.println("Enter choice (1-6) or 'M' for menu:");
}

void MenuSystem::showInfoMenu() {
    _currentMenu = INFO_MENU;
    printMenuHeader("DEVICE INFORMATION");
    printMenuItem(1, "Read Device Model", "Get device model information");
    printMenuItem(2, "Read FPGA Version", "Get FPGA version");
    printMenuItem(3, "Read Initialization Status", "Get initialization status");
    printMenuItem(0, "Back to Main Menu");
    Serial.println("Enter choice:");
}

void MenuSystem::showImageMenu() {
    _currentMenu = IMAGE_MENU;
    printMenuHeader("IMAGE SETTINGS");
    printMenuItem(1, "Set Brightness", "Adjust image brightness (0-100)");
    printMenuItem(2, "Set Contrast", "Adjust image contrast (0-100)");
    printMenuItem(3, "Read Current Settings", "Display all current values");
    printMenuItem(0, "Back to Main Menu");
    Serial.println("Enter choice:");
}

void MenuSystem::showCameraMenu() {
    _currentMenu = CAMERA_MENU;
    printMenuHeader("CAMERA CONTROL");
    printMenuItem(1, "Manual FFC", "Perform flat field correction");
    printMenuItem(2, "Background Correction", "Perform background correction");
    printMenuItem(0, "Back to Main Menu");
    Serial.println("Enter choice:");
}

void MenuSystem::showCursorMenu() {
    _currentMenu = CURSOR_MENU;
    printMenuHeader("CURSOR CONTROL");
    printMenuItem(1, "Show Cursor", "Display cursor on screen");
    printMenuItem(2, "Hide Cursor", "Hide cursor from screen");
    printMenuItem(3, "Center Cursor", "Move cursor to center");
    printMenuItem(4, "Move Up", "Move cursor up 1 pixel");
    printMenuItem(5, "Move Down", "Move cursor down 1 pixel");
    printMenuItem(6, "Move Left", "Move cursor left 1 pixel");
    printMenuItem(7, "Move Right", "Move cursor right 1 pixel");
    printMenuItem(0, "Back to Main Menu");
    Serial.println("Enter choice:");
}

void MenuSystem::showPaletteMenu() {
    _currentMenu = PALETTE_MENU;
    printMenuHeader("COLOR PALETTES");
    
    const char* paletteNames[] = {
        "White Hot", "Black Hot", "Iron", "Rainbow", "Rain",
        "Ice Fire", "Fusion", "Sepia", "Color1", "Color2",
        "Color3", "Color4", "Color5", "Color6", "Color7"
    };
    
    for (int i = 0; i < 15; i++) {
        printMenuItem(i + 1, paletteNames[i]);
    }
    
    printSeparator();
    printMenuItem(16, "Read Current Palette");
    printMenuItem(0, "Back to Main Menu");
    Serial.println("Enter choice:");
}

void MenuSystem::showSystemMenu() {
    _currentMenu = SYSTEM_MENU;
    printMenuHeader("SYSTEM SETTINGS");
    printMenuItem(1, "Save Configuration", "Save current settings to flash");
    printMenuItem(2, "Restore Factory", "Restore factory default settings");
    printMenuItem(3, "Test Connection", "Test camera communication");
    printMenuItem(0, "Back to Main Menu");
    Serial.println("Enter choice:");
}

void MenuSystem::executeMenuChoice(int choice) {
    switch (_currentMenu) {
        case MAIN_MENU:
            switch (choice) {
                case 1: showInfoMenu(); break;
                case 2: showImageMenu(); break;
                case 3: showCameraMenu(); break;
                case 4: showCursorMenu(); break;
                case 5: showPaletteMenu(); break;
                case 6: showSystemMenu(); break;
                default: printError("Invalid choice");
            }
            break;
            
        case INFO_MENU:
            handleInfoMenuChoice(choice);
            break;
            
        case IMAGE_MENU:
            switch (choice) {
                case 1: setBrightnessInteractive(); break;
                case 2: setContrastInteractive(); break;
                case 3: readCurrentImageSettings(); break;
                case 0: returnToMainMenu(); break;
                default: printError("Invalid choice");
            }
            break;
            
        case CAMERA_MENU:
            switch (choice) {
                case 1: 
                    if (_camera->performManualFFC()) {
                        printSuccess("Manual FFC completed");
                    } else {
                        printError(_camera->getLastError());
                    }
                    break;
                case 2:
                    if (_camera->performBackgroundCorrection()) {
                        printSuccess("Background correction completed");
                    } else {
                        printError(_camera->getLastError());
                    }
                    break;
                case 0: returnToMainMenu(); break;
                default: printError("Invalid choice");
            }
            break;
            
        case CURSOR_MENU:
            switch (choice) {
                case 1: _camera->showCursor(); break;
                case 2: _camera->hideCursor(); break;
                case 3: _camera->centerCursor(); break;
                case 4: _camera->moveCursorUp(); break;
                case 5: _camera->moveCursorDown(); break;
                case 6: _camera->moveCursorLeft(); break;
                case 7: _camera->moveCursorRight(); break;
                case 0: returnToMainMenu(); break;
                default: printError("Invalid choice");
            }
            break;
            
        case PALETTE_MENU:
            if (choice >= 1 && choice <= 15) {
                ColorPalette palette = (ColorPalette)(choice - 1);
                if (_camera->setPalette(palette)) {
                    printSuccess("Palette changed successfully");
                } else {
                    printError(_camera->getLastError());
                }
            } else if (choice == 16) {
                readCurrentPalette();
            } else if (choice == 0) {
                returnToMainMenu();
            } else {
                printError("Invalid choice");
            }
            break;
            
        case SYSTEM_MENU:
            switch (choice) {
                case 1: saveConfiguration(); break;
                case 2: restoreFactory(); break;
                case 3: testConnection(); break;
                case 0: returnToMainMenu(); break;
                default: printError("Invalid choice");
            }
            break;
    }
}

// Acciones específicas
void MenuSystem::setBrightnessInteractive() {
    requestInput("Enter brightness value (0-100):", &MenuSystem::handleBrightnessInput);
}

void MenuSystem::handleBrightnessInput(String input) {
    if (validateNumericInput(input, 0, 100)) {
        uint8_t value = input.toInt();
        if (_camera->setBrightness(value)) {
            printSuccess("Brightness set to " + String(value));
        } else {
            printError(_camera->getLastError());
        }
    }
    showImageMenu();
}

void MenuSystem::setContrastInteractive() {
    requestInput("Enter contrast value (0-100):", &MenuSystem::handleContrastInput);
}

void MenuSystem::handleContrastInput(String input) {
    if (validateNumericInput(input, 0, 100)) {
        uint8_t value = input.toInt();
        if (_camera->setContrast(value)) {
            printSuccess("Contrast set to " + String(value));
        } else {
            printError(_camera->getLastError());
        }
    }
    showImageMenu();
}

void MenuSystem::readCurrentImageSettings() {
    Serial.println("\n📊 Current Image Settings:");
    Serial.println("Brightness: " + String(_camera->getBrightness()) + "/100");
    Serial.println("Contrast: " + String(_camera->getContrast()) + "/100");
    Serial.println("Current Palette: " + String(_camera->getCurrentPalette()));
    Serial.println("");
}

void MenuSystem::readDeviceModel() {
    String model = _camera->getModel();
    if (model.length() > 0) {
        Serial.println("📷 Device Model: " + model);
    } else {
        printError("Failed to read device model");
    }
}

void MenuSystem::readAllInfo() {
    CameraInfo info;
    if (_camera->getDeviceInfo(info)) {
        Serial.println("\n📋 Complete Device Information:");
        Serial.println("Model: " + info.model);
        Serial.println("FPGA Version: " + info.fpgaVersion);
        Serial.println("Software Version: " + info.softwareVersion);
        Serial.println("Status: " + String(info.status));
        Serial.println("");
    } else {
        printError("Failed to read device information");
    }
}

void MenuSystem::readCurrentPalette() {
    ColorPalette palette = _camera->getCurrentPalette();
    Serial.println("🎨 Current Palette: " + String(palette));
}

void MenuSystem::saveConfiguration() {
    if (_camera->saveConfiguration()) {
        printSuccess("Configuration saved to flash memory");
    } else {
        printError(_camera->getLastError());
    }
}

void MenuSystem::restoreFactory() {
    if (_camera->restoreFactory()) {
        printSuccess("Factory settings restored");
    } else {
        printError(_camera->getLastError());
    }
}

void MenuSystem::testConnection() {
    if (_camera->isConnected()) {
        printSuccess("Camera connection OK");
        String model = _camera->getModel();
        Serial.println("📷 Connected to: " + model);
    } else {
        printError("Camera not responding");
    }
}

// Utilidades
void MenuSystem::requestInput(String prompt, void (MenuSystem::*handler)(String)) {
    _waitingForInput = true;
    _inputPrompt = prompt;
    _inputHandler = handler;
    Serial.println(prompt);
}

void MenuSystem::printMenuHeader(String title) {
    String separator = "";
    for (int i = 0; i < title.length() + 8; i++) {
        separator += "=";
    }
    
    Serial.println("\n" + separator);
    Serial.println("=== " + title + " ===");
    Serial.println(separator);
}

void MenuSystem::printSeparator() {
    String separator = "";
    for (int i = 0; i < 30; i++) {
        separator += "-";
    }
    Serial.println(separator);
}

void MenuSystem::printMenuItem(int number, String name, String description) {
    Serial.print(String(number) + " - " + name);
    if (description.length() > 0) {
        Serial.println(" (" + description + ")");
    } else {
        Serial.println();
    }
}

bool MenuSystem::validateNumericInput(String input, int min, int max) {
    int value = input.toInt();
    if (value < min || value > max) {
        printError("Value must be between " + String(min) + " and " + String(max));
        return false;
    }
    return true;
}

void MenuSystem::printError(String error) {
    Serial.println("❌ Error: " + error);
}

void MenuSystem::printSuccess(String message) {
    Serial.println("✅ " + message);
}

void MenuSystem::returnToMainMenu() {
    _waitingForInput = false;
    _inputHandler = nullptr;
    showMainMenu();
}

MenuState MenuSystem::getCurrentMenu() const {
    return _currentMenu;
}

void MenuSystem::update() {
    // Esta función se puede usar para tareas periódicas si es necesario
}

void MenuSystem::handleInfoMenuChoice(int choice) {
    switch (choice) {
        case 1:
            if (_camera->readDeviceModel()) {
                Serial.println("Device model read successfully.");
            } else {
                printError("Failed to read device model.");
            }
            break;
        case 2:
            if (_camera->readFPGA_Version()) {
                Serial.println("FPGA version read successfully.");
            } else {
                printError("Failed to read FPGA version.");
            }
            break;
        case 3:
            if (_camera->readInitializationStatus()) {
                Serial.println("Initialization status read successfully.");
            } else {
                printError("Failed to read initialization status.");
            }
            break;
        case 0:
            returnToMainMenu();
            break;
        default:
            printError("Invalid option.");
            break;
    }
}