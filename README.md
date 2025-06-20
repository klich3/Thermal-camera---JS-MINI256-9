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
- **Color Palettes:** 15 available palettes including White Hot, Black Hot, Iron, Rainbow, Rain, Ice Fire, Fusion, Sepia, and 7 custom color palettes
- **Digital Enhancement:** Adjustable detail enhancement (0-100)
- **Noise Reduction:** Static and dynamic denoising (0-100 each)
- **Image Controls:** Brightness (0-100), contrast adjustment (0-100)
- **Mirror Functions:** Disabled, Central, Horizontal, Vertical mirroring

### Power & Interface
- **Supply Voltage:** 5V-16V DC
- **Power Consumption:** ≤0.8W
- **Video Output:** CVBS (Composite Video)
- **Control Interface:** UART (TTL level, 115200 baud)
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

## ESP32 Controller Features

### Hardware Configuration
- **ESP32 DevKit** or compatible board
- **UART2 Interface** (GPIO16 RX, GPIO17 TX)
- **Baudrate:** 115200 bps, 8N1
- **Primary UART:** Menu control and monitoring
- **Secondary UART2:** Camera communication

### Interactive Menu System
The controller provides a comprehensive menu-driven interface accessible via Serial Monitor:

#### Main Menu Options
- **R** - Read Commands (Camera information and status)
- **W** - Write Commands (Configuration and calibration)
- **X** - Read/Write Commands (Preset configurations)
- **P** - Parametric Commands (Adjustable values)
- **C** - Cursor Commands (Cursor control and dead pixel management)
- **L** - Color Palette Commands (15 thermal palettes)
- **D** - Dynamic Command Example
- **M** - Show main menu

### Available Commands

#### Read Commands (Class 0x74)
1. **Module Model** - Read device model information
2. **FPGA Version** - Read FPGA firmware version
3. **FPGA Build Date** - Read FPGA compilation date
4. **Software Version** - Read software version
5. **Software Build Date** - Read software compilation date
6. **Camera Calibration Version** - Read calibration data version
7. **ISP Parameters Version** - Read image processing parameters version
8. **Initialization Status** - Read device initialization state

#### Write Commands (Class 0x74, 0x7C)
1. **Save Configuration** - Store current settings to flash
2. **Factory Reset** - Restore factory default settings
3. **Manual FFC (Flat Field Correction)** - Execute manual shutter calibration
4. **Manual Background Correction** - Execute background correction
5. **Vignetting Correction** - Execute vignetting correction

#### Image Control Commands (Class 0x78)
1. **Brightness Control** (0-100) - Adjust image brightness
2. **Contrast Control** (0-100) - Adjust image contrast
3. **Digital Detail Enhancement** (0-100) - Enhance image details
4. **Static Noise Reduction** (0-100) - Reduce static noise
5. **Dynamic Noise Reduction** (0-100) - Reduce dynamic noise
6. **Thermal Palette Selection** (0-14) - Choose color palette
7. **Image Mirroring** (0-3) - Set image orientation

#### Automatic Shutter Control (Class 0x7C)
1. **Auto Shutter Mode** (0-3):
   - 0: Disabled
   - 1: Manual
   - 2: Automatic
   - 3: Fully Automatic
2. **Auto Shutter Interval** (0-65535 minutes) - Set calibration interval

#### Color Palettes (15 Available)
1. **White Hot** - Default white hot palette
2. **Black Hot** - Inverted black hot palette
3. **Iron** - Iron/metal style palette
4. **Rainbow** - Full spectrum rainbow
5. **Rain** - Blue-green gradient palette
6. **Ice Fire** - Cold to hot gradient
7. **Fusion** - Multi-color fusion palette
8. **Sepia** - Classic brown tone palette
9. **Color1-7** - Seven customizable user palettes

#### Cursor Control Commands (Class 0x78, Subclass 0x1A)
- **Show/Hide Cursor** - Toggle cursor visibility
- **Basic Movement** - Single pixel movement (up, down, left, right)
- **Multi-pixel Movement** - Move 1-15 pixels in any direction
- **Center Cursor** - Move cursor to center position
- **Dead Pixel Management** - Add/remove defective pixels

### Protocol Implementation

#### Command Structure
```
[Header] [Length] [Device] [Class] [Subclass] [R/W] [Data...] [Checksum] [Footer]
  0xF0     0x05     0x36     0x7x      0xxx     0x0x    ...      CHK      0xFF
```

#### Response Interpretation
The controller automatically interprets all camera responses with human-readable format:
- **Device Information** - Model, versions, build dates
- **Status Information** - Initialization state, current settings
- **Parameter Values** - Current brightness, contrast, palette, etc.
- **Action Confirmations** - Calibration completion, settings saved

#### Automatic Features
- **Checksum Calculation** - All commands include automatic checksum
- **Response Parsing** - Intelligent response interpretation
- **Parameter Validation** - Input range checking
- **Timeout Handling** - 50ms byte timeout, 100ms response timeout

### Usage Examples

#### Quick Start
1. Connect ESP32 to computer via USB
2. Connect camera UART to GPIO16(RX)/GPIO17(TX)
3. Open Serial Monitor at 115200 baud
4. Type **'M'** to show main menu
5. Navigate menus using number keys

#### Change Thermal Palette
```
M          # Show main menu
L          # Enter palette menu
4          # Select Rainbow palette
```

#### Adjust Image Settings
```
M          # Show main menu
P          # Enter parametric menu
3          # Select brightness control
75         # Set brightness to 75%
```

#### Read Camera Information
```
M          # Show main menu
R          # Enter read menu
1          # Read module model
```

### Advanced Features

#### Dynamic Command Generation
The controller includes dynamic command generation for parametric values:
- Automatic checksum calculation
- Parameter validation
- Multi-byte parameter support
- Real-time command building

#### Response Analysis
- Hex dump of raw responses
- Structured field interpretation
- Human-readable status messages
- Error detection and reporting

## Applications
- FPV (First Person View) systems
- Drone thermal imaging
- Security surveillance
- Search and rescue operations
- Industrial inspection
- Wildlife observation
- Remote thermal monitoring
- Automated thermal analysis

## Technical Notes

### Communication Protocol
- **Baud Rate:** 115200 bps
- **Data Format:** 8 data bits, no parity, 1 stop bit
- **Flow Control:** None
- **Command Timeout:** 100ms
- **Byte Timeout:** 50ms

### Memory Usage
- **Maximum Response Size:** 256 bytes
- **Command Buffer:** 16 bytes
- **Menu States:** 7 different menu contexts
- **Command Arrays:** Pre-compiled for efficiency

### Error Handling
- Input validation for all parameters
- Range checking for numeric values
- Timeout detection for responses
- Malformed command detection
- Automatic menu recovery

## Troubleshooting

### Common Issues
1. **No Response from Camera**
   - Check UART connections (RX/TX crossed)
   - Verify power supply (5V-16V)
   - Confirm baud rate (115200)

2. **Invalid Commands**
   - Use menu system for validated commands
   - Check parameter ranges
   - Verify checksum calculation

3. **Timeout Errors**
   - Increase timeout values if needed
   - Check cable connections
   - Verify camera power status

### Development Notes
- All commands are pre-validated with correct checksums
- Response parsing handles variable-length data
- Menu system prevents invalid command entry
- Automatic parameter range validation
