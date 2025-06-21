/*
█▀ █▄█ █▀▀ █░█ █▀▀ █░█
▄█ ░█░ █▄▄ █▀█ ██▄ ▀▄▀

Author: <Anton Sychev> (anton at sychev dot xyz)
MenuSystem.h (c) 2025
Created:  2025-06-21 01:36:27 
Desc: Interactive Menu System for Camera Control
*/

#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>
#include "CameraController.h"

enum MenuState {
    MAIN_MENU,
    INFO_MENU,
    IMAGE_MENU,
    CAMERA_MENU,
    CURSOR_MENU,
    PALETTE_MENU,
    SYSTEM_MENU
};

class MenuSystem {
private:
    CameraController* _camera;
    MenuState _currentMenu;
    bool _waitingForInput;
    String _inputPrompt;
    void (MenuSystem::*_inputHandler)(String);
    
    // Funciones de menú
    /**
     * Muestra el menú principal.
     */
    void showMainMenu();

    /**
     * Muestra el menú de información del dispositivo.
     */
    void showInfoMenu();

    /**
     * Muestra el menú de configuración de imagen.
     */
    void showImageMenu();

    /**
     * Muestra el menú de control de la cámara.
     */
    void showCameraMenu();

    /**
     * Muestra el menú de control del cursor.
     */
    void showCursorMenu();

    /**
     * Muestra el menú de selección de paletas de colores.
     */
    void showPaletteMenu();

    /**
     * Muestra el menú de configuración del sistema.
     */
    void showSystemMenu();
    
    // Funciones de ejecución
    /**
     * Ejecuta la acción correspondiente a la opción seleccionada en el menú actual.
     * @param choice Número de la opción seleccionada.
     */
    void executeMenuChoice(int choice);
    
    // Acciones de imagen
    /**
     * Permite configurar el brillo de la imagen de forma interactiva.
     */
    void setBrightnessInteractive();

    /**
     * Permite configurar el contraste de la imagen de forma interactiva.
     */
    void setContrastInteractive();

    /**
     * Lee y muestra los valores actuales de configuración de la imagen.
     */
    void readCurrentImageSettings();
    
    // Acciones de información
    /**
     * Lee y muestra el modelo del dispositivo.
     */
    void readDeviceModel();

    /**
     * Lee y muestra toda la información del dispositivo.
     */
    void readAllInfo();
    
    // Acciones de paleta
    /**
     * Lee y muestra la paleta de colores actual.
     */
    void readCurrentPalette();
    
    // Acciones de sistema
    /**
     * Guarda la configuración actual en la memoria del dispositivo.
     */
    void saveConfiguration();

    /**
     * Restaura la configuración de fábrica del dispositivo.
     */
    void restoreFactory();

    /**
     * Prueba la conexión con la cámara.
     */
    void testConnection();
    
    // Funciones de entrada
    /**
     * Maneja la entrada del usuario para configurar el brillo.
     * @param input Entrada del usuario.
     */
    void handleBrightnessInput(String input);

    /**
     * Maneja la entrada del usuario para configurar el contraste.
     * @param input Entrada del usuario.
     */
    void handleContrastInput(String input);

    /**
     * Maneja la entrada del usuario para seleccionar una paleta de colores.
     * @param input Entrada del usuario.
     */
    void handlePaletteInput(String input);
    
    // Manejo de opciones del menú de información
    /**
     * Maneja las opciones seleccionadas en el menú de información.
     * @param choice Número de la opción seleccionada.
     */
    void handleInfoMenuChoice(int choice);
    
    // Utilidades
    /**
     * Solicita una entrada al usuario con un mensaje específico y asigna un manejador para procesar la entrada.
     * @param prompt Mensaje que se muestra al usuario.
     * @param handler Puntero a la función que manejará la entrada del usuario.
     */
    void requestInput(String prompt, void (MenuSystem::*handler)(String));

    /**
     * Imprime el encabezado de un menú con un título específico.
     * @param title Título del menú.
     */
    void printMenuHeader(String title);

    /**
     * Imprime un elemento del menú con su número, nombre y descripción opcional.
     * @param number Número del elemento.
     * @param name Nombre del elemento.
     * @param description Descripción opcional del elemento.
     */
    void printMenuItem(int number, String name, String description = "");

    /**
     * Imprime un separador visual para los menús.
     */
    void printSeparator();

    /**
     * Valida si una entrada numérica está dentro de un rango específico.
     * @param input Entrada del usuario.
     * @param min Valor mínimo permitido.
     * @param max Valor máximo permitido.
     * @return true si la entrada es válida, false en caso contrario.
     */
    bool validateNumericInput(String input, int min, int max);

    /**
     * Imprime un mensaje de error.
     * @param error Mensaje de error.
     */
    void printError(String error);

    /**
     * Imprime un mensaje de éxito.
     * @param message Mensaje de éxito.
     */
    void printSuccess(String message);
    
public:
    MenuSystem(CameraController* camera);
    void begin();
    void processInput(String input);
    void update();
    MenuState getCurrentMenu() const;
    void returnToMainMenu();
};

#endif