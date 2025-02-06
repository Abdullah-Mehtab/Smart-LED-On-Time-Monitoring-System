# Embedded System: Google Sheet LED Detection & ON Time Calculation

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A project that logs LED statuses (ON/OFF) to Google Sheets using an ESP32 and calculates the total ON time within a user-specified range via a web interface.

## 📝 Overview
This project integrates an ESP32 microcontroller with Google Sheets to log the status of four LEDs and calculate their total ON time. It combines hardware (ESP32, LEDs) and software (Google Apps Script, web interface) to demonstrate IoT capabilities, including:
- Real-time LED control via a web interface.
- Automated logging to Google Sheets with timestamps.
- ON time calculation for any LED within custom time ranges.

## 🚀 Features
- **Multi-LED Control**: Toggle 4 LEDs via GPIO pins 5, 18, 19, and 21.
- **Google Sheets Integration**: Log status changes with timestamps.
- **ON Time Calculation**: Query total ON duration for any LED in hours, minutes, and seconds.
- **Web Interface**: Responsive UI for controlling LEDs and submitting queries.
- **Error Handling**: Robust checks for Wi-Fi, HTTP requests, and data validation.
- **Local Backup**: Log data to SPIFFS (local CSV) if cloud upload fails.

## 🛠 Hardware Requirements
| Component          | Quantity |
|---------------------|----------|
| ESP32 Development Board | 1        |
| LEDs (Any Color)    | 4        |
| 220Ω Resistors      | 4        |
| Breadboard          | 1        |
| Jumper Wires        | 10+      |
| Micro-USB Cable     | 1        |

## 💻 Software Requirements
- **Arduino IDE** (with ESP32 Board Support)
- **Libraries**: `WiFi.h`, `HTTPClient.h`, `WebServer.h`, `SPIFFS.h`, `ArduinoJson.h`
- **Google Account** (for Sheets and Apps Script)
- **Web Browser** (for interface access)

---

## 🔧 Setup Instructions

### 1. Google Sheets & Apps Script Setup
1. **Create a Google Sheet**:
   - Name: `EmbeddedProject`.
   - Columns: `Datetime`, `LED`, `Status`.
   
2. **Deploy the Google Apps Script**:
   - Open the Sheet → **Extensions → Apps Script**.
   - Replace the default code with [Code.gs](Code.gs).
   - Deploy as a **Web App**:
     - Execute as: **Me**.
     - Access: **Anyone**.
   - Copy the generated web app URL for the ESP32 code.

### 2. Hardware Setup
1. **Circuit Connections**:
   - Connect LEDs to ESP32 GPIO pins **5, 18, 19, 21** with 220Ω resistors.
   - Ensure all cathodes are grounded.
   
   ![Circuit Diagram](media/image8.png)

### 3. Software Configuration
1. **ESP32 Code**:
   - Open [EmbeddedProject.ino](EmbeddedProject.ino) in Arduino IDE.
   - Replace placeholders:
     - `ssid`: Your Wi-Fi SSID.
     - `password`: Your Wi-Fi password.
     - `googleScriptURL`: Your deployed Google Apps Script URL.
   - Install required libraries via **Tools → Manage Libraries**.
   - Upload the code to the ESP32.

---

## 🖥 Usage
1. **Access the Web Interface**:
   - Connect to the ESP32’s IP (printed in Serial Monitor).
   - Interface supports both desktop and mobile browsers.

2. **Control LEDs**:
   - Click **ON/OFF** buttons to toggle LEDs. Status updates in real-time.

3. **Query ON Time**:
   - Select LED, start/end date, and time.
   - Submit to view total ON duration and a Google Sheets query link.

![Web Interface](media/image9.png)

---

## 📂 Code Structure
| File               | Description                                                                 |
|---------------------|-----------------------------------------------------------------------------|
| `EmbeddedProject.ino` | ESP32 code for LED control, logging, and web server.                        |
| `Code.gs`            | Google Apps Script for Google Sheets data processing and ON time calculation. |

## 🔄 Workflow
1. **LED Toggle** → Log to Google Sheets/SPIFFS.
2. **User Query** → Fetch data from Google Sheets → Calculate ON time → Display results.

---

## 🚨 Troubleshooting
| Issue                          | Solution                                   |
|--------------------------------|-------------------------------------------|
| Wi-Fi Connection Failed        | Check SSID/password in code.              |
| Google Sheets Access Denied    | Ensure the Apps Script web app is public. |
| SPIFFS Mount Error             | Format SPIFFS in Arduino IDE.             |
| HTTP Request Timeout           | Verify Google Script URL and parameters.  |

---

## 📜 License
This project is open-source and available under the MIT License. See [LICENSE](LICENSE) for details.

---

## 🙏 Acknowledgments
- [Google Apps Script Documentation](https://developers.google.com/apps-script)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [Bootstrap](https://getbootstrap.com/) for the web interface styling.
