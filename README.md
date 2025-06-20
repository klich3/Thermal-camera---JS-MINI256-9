# FPV Thermal Camera Controller

ESP32-based FPV thermal camera controller for JS-MINI256-9 cameras.

**Camera Model:** JS-MINI256-9  
**Control Interface:** UART Communication Protocol

## Camera Specifications

### Detector
- **Type:** Vanadium Oxide Uncooled Infrared Focal Plane Detector
- **Resolution:** 256×192 pixels
- **Frame Rate:** 25fps
- **Pixel Pitch:** 12 μm
- **NETD:** <40mK (@25°C, F/1.0)

### Optical Specifications
- **Spectral Range:** 8~14 μm (Long Wave Infrared)
- **Available Lenses:**
  - 4.0mm F1.0 / 6.8mm F1.0
  - 9.7mm F1.0

### Field of View (FOV)
- **4mm lens:** 45° (H) × 33° (V)
- **6.8mm lens:** 26° (H) × 20° (V) 
- **9.7mm lens:** 18.1° (H) × 13.6° (V)

### Image Processing
- **Color Palettes:** White heat, Black heat, Fusion, Iron, Red, Fusion
- **Digital Enhancement:** Adjustable detail enhancement
- **Noise Reduction:** Static and dynamic denoising
- **Image Controls:** Brightness, contrast adjustment

### Power & Interface
- **Supply Voltage:** 5V-16V DC
- **Power Consumption:** ≤0.8W
- **Video Output:** CVBS (Composite Video)
- **Control Interface:** UART (TTL level)
- **Secondary Output:** PAL standard

### Physical Specifications
- **Dimensions:** 20mm × 20mm
- **Weight:** ≤23g
- **Operating Temperature:** -40°C ~ +80°C
- **Storage Temperature:** -45°C ~ 85°C

### Connector Pinout
1. **POWER** - DC power input (5V-16V)
2. **GND** - Ground
3. **CVBS** - Composite video output
4. **UART TXD** - UART transmit data
5. **UART RXD** - UART receive data

## Features
- Ultra-compact design for FPV applications
- Wide voltage input range
- Low power consumption
- Multiple lens options
- Real-time thermal imaging
- UART command control
- Multiple color palette modes
- Digital image enhancement

## Applications
- FPV (First Person View) systems
- Drone thermal imaging
- Security surveillance
- Search and rescue operations
- Industrial inspection
- Wildlife observation

## UART Control Protocol
This ESP32 controller implements a complete UART command interface for:
- Reading camera information and status
- Adjusting image parameters (brightness, contrast, palette)
- Controlling camera functions (FFC, background correction)
- Cursor control and bad pixel management

See the source code for complete command reference and implementation details.