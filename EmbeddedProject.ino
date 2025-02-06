/*
  ESP32 LED Control with Local CSV + Google Sheets Logging (No Google Calc)
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include "time.h"
#include <SPIFFS.h>  // for local file storage

// -------------------- WiFi Credentials --------------------
const char* ssid     = "Aalu";
const char* password = "robot12!";

// -------------------- Google Apps Script Web App --------------------
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbz6w5DTo7NOeofEVrGnm0GYlC_3kKu7jHzOL4Wcjhv5TXx38yqDcgvWYgwZ64kaQPY8/exec";

// -------------------- GPIO Definitions --------------------
#define NUM_LEDS 4
const uint8_t LED_PINS[NUM_LEDS] = {5, 18, 19, 21};

// -------------------- Global Variables --------------------
bool   ledStates[NUM_LEDS]  = {false, false, false, false};
String ledOnTimes[NUM_LEDS] = {"00:00:00", "00:00:00", "00:00:00", "00:00:00"};

// -------------------- Time (NTP) Settings --------------------
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 5 * 3600; // Adjust for your timezone
const int   daylightOffset_sec = 0;

// -------------------- Local CSV File Path (in SPIFFS) -------------------
const char* CSV_FILE_PATH = "/led_log.csv"; 

// -------------------- WebServer Setup --------------------
WebServer server(80);

// -------------------- Function Declarations --------------------
void handleRoot();
void handleWebButton();
void handleQuery();
void handleStatus();
void toggleLED(uint8_t ledIndex, bool state);
void uploadToGoogleSheet(uint8_t ledIndex, bool state);

// Local CSV helpers
void appendCSVEntry(const String& datetime, uint8_t ledNumber, bool isOn);
void createCSVIfMissing();
String localCalculateOnTime(uint8_t ledNumber,
                            const String& startDate, const String& startTime,
                            const String& endDate,   const String& endTime);
void printEntireCSVtoSerial(const char* reason);

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize file system (SPIFFS)
  if(!SPIFFS.begin(true)){
    Serial.println("[ERROR] SPIFFS Mount Failed!");
  } else {
    Serial.println("[INFO] SPIFFS mounted successfully.");
    // Print the local CSV path
    Serial.printf("[INFO] Using local CSV file at: %s\n", CSV_FILE_PATH);
    createCSVIfMissing();
  }

  // Initialize LED pins
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  // Connect to Wi-Fi
  Serial.println("[INFO] Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[INFO] WiFi connected!");
  Serial.print("[INFO] ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Initialize time via NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("[INFO] NTP initialized.");

  // Define web server routes
  server.on("/", handleRoot);
  server.on("/led", handleWebButton);
  server.on("/query", handleQuery);
  server.on("/status", handleStatus);

  // Start web server
  server.begin();
  Serial.println("[INFO] Web server started.");
}

void loop() {
  server.handleClient();
}

// -----------------------------------------------------------
// toggleLED()
// Toggles a specific LED ON or OFF. Updates ledStates[].
// -----------------------------------------------------------
void toggleLED(uint8_t ledIndex, bool state) {
  if (ledIndex >= NUM_LEDS) {
    Serial.printf("[ERROR] Invalid LED index: %d\n", ledIndex);
    return;
  }
  ledStates[ledIndex] = state;
  digitalWrite(LED_PINS[ledIndex], (state ? HIGH : LOW));
  Serial.printf("[EVENT] LED %d turned %s\n", ledIndex + 1, state ? "ON" : "OFF");
}

// -----------------------------------------------------------
// uploadToGoogleSheet()
// Logs LED toggles to the Google Apps Script Web App
// Also logs to local CSV via appendCSVEntry()
// -----------------------------------------------------------
void uploadToGoogleSheet(uint8_t ledIndex, bool state) {
  // Prepare timestamp
  char datetimeBuff[20];
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[ERROR] Failed to get local time (NTP).");
    return;
  }
  strftime(datetimeBuff, sizeof(datetimeBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);

  // Log to local CSV
  appendCSVEntry(String(datetimeBuff), ledIndex + 1, state);

  // Also upload to Google if WiFi connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] WiFi not connected. Cannot upload to Google Sheet.");
    return;
  }

  // Encode spaces as %20
  char finalDatetime[40];
  int j = 0;
  for (int i = 0; i < strlen(datetimeBuff); i++) {
    if (datetimeBuff[i] == ' ') {
      finalDatetime[j++] = '%';
      finalDatetime[j++] = '2';
      finalDatetime[j++] = '0';
    } else {
      finalDatetime[j++] = datetimeBuff[i];
    }
  }
  finalDatetime[j] = '\0';

  // Construct Google URL
  String url = String(googleScriptURL) +
               "?lednumber=" + String(ledIndex + 1) +
               "&ledstatus=" + (state ? "1" : "0") +
               "&datetime="  + finalDatetime;

  Serial.println("[INFO] Uploading to Google Sheet: " + url);

  HTTPClient http;
  http.setTimeout(5000);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[INFO] HTTP Response Code: 200 || (Data Appended to Sheet)\n");
  } else {
    Serial.printf("[ERROR] HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();

  // If LED is ON, store the HH:MM:SS part
  if (state) {
    ledOnTimes[ledIndex] = String(datetimeBuff).substring(11);
  }
}

// -----------------------------------------------------------
// handleRoot(): Serves the main HTML for LED control & queries.
// -----------------------------------------------------------
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 LED Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css">
  <style>
    body {
      margin: 0; padding: 0; color: #fff; text-align: center;
      background: linear-gradient(-45deg, #1A2980, #26D0CE, #6a11cb, #2575fc);
      background-size: 400% 400%; animation: gradientBG 15s ease infinite;
      font-family: Arial, sans-serif; padding-top: 30px;
    }
    @keyframes gradientBG {
      0% { background-position: 0% 50%; }
      50% { background-position: 100% 50%; }
      100% { background-position: 0% 50%; }
    }
    .container {
      max-width: 800px; margin: auto; background: rgba(0,0,0,0.3);
      padding: 20px; border-radius: 10px;
    }
    .led-row {
      display: flex; flex-wrap: wrap; justify-content: space-around; margin-bottom: 20px;
    }
    .led-col {
      width: 150px; margin: 10px; padding: 10px; background: rgba(255,255,255,0.1);
      border-radius: 5px;
    }
    .btn { margin: 5px 0; width: 80px; }
    .form-group { text-align: left; margin-bottom: 15px; }
    #result {
      text-align: left; background: rgba(0,0,0,0.1); padding: 10px;
      border-radius: 5px; margin-top: 20px; white-space: pre-wrap;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 LED Control</h1>
    
    <div class="led-row">
      <div class="led-col">
        <b>LED 1:</b> <span id="led1Status">)rawliteral" + 
        String(ledStates[0] ? "ON" : "OFF") + R"rawliteral(</span><br>
        <button class="btn btn-success" onclick="toggleLED(1, 'on')">ON</button>
        <button class="btn btn-danger"  onclick="toggleLED(1, 'off')">OFF</button>
      </div>
      <div class="led-col">
        <b>LED 2:</b> <span id="led2Status">)rawliteral" +
        String(ledStates[1] ? "ON" : "OFF") + R"rawliteral(</span><br>
        <button class="btn btn-success" onclick="toggleLED(2, 'on')">ON</button>
        <button class="btn btn-danger"  onclick="toggleLED(2, 'off')">OFF</button>
      </div>
      <div class="led-col">
        <b>LED 3:</b> <span id="led3Status">)rawliteral" +
        String(ledStates[2] ? "ON" : "OFF") + R"rawliteral(</span><br>
        <button class="btn btn-success" onclick="toggleLED(3, 'on')">ON</button>
        <button class="btn btn-danger"  onclick="toggleLED(3, 'off')">OFF</button>
      </div>
      <div class="led-col">
        <b>LED 4:</b> <span id="led4Status">)rawliteral" +
        String(ledStates[3] ? "ON" : "OFF") + R"rawliteral(</span><br>
        <button class="btn btn-success" onclick="toggleLED(4, 'on')">ON</button>
        <button class="btn btn-danger"  onclick="toggleLED(4, 'off')">OFF</button>
      </div>
    </div>

    <h2>Query LED ON Time</h2>
    <form id="queryForm">
      <div class="form-group">
        <label for="lednumber">Select LED:</label>
        <select name="lednumber" id="lednumber" class="form-control" required>
          <option value="1">LED 1</option>
          <option value="2">LED 2</option>
          <option value="3">LED 3</option>
          <option value="4">LED 4</option>
        </select>
      </div>
      <div class="form-group">
        <label for="startDate">Start Date:</label>
        <input type="date" name="startDate" id="startDate" class="form-control" required>
      </div>
      <div class="form-group">
        <label for="startTime">Start Time:</label>
        <input type="time" name="startTime" id="startTime" class="form-control" required>
      </div>
      <div class="form-group">
        <label for="endDate">End Date:</label>
        <input type="date" name="endDate" id="endDate" class="form-control" required>
      </div>
      <div class="form-group">
        <label for="endTime">End Time:</label>
        <input type="time" name="endTime" id="endTime" class="form-control" required>
      </div>
      <button type="submit" class="btn btn-primary">Submit</button>
    </form>

    <div id="result"></div>
  </div>

  <script>
    function toggleLED(ledNumber, state) {
      fetch(`/led?lednumber=${ledNumber}&state=${state}`)
        .then(() => updateLEDStatus())
        .catch(err => console.error('Toggle LED fetch error:', err));
    }

    function updateLEDStatus() {
      fetch('/status')
        .then(resp => {
          if(!resp.ok) throw new Error('Status fetch error');
          return resp.json();
        })
        .then(data => {
          document.getElementById('led1Status').textContent = data.LED1;
          document.getElementById('led2Status').textContent = data.LED2;
          document.getElementById('led3Status').textContent = data.LED3;
          document.getElementById('led4Status').textContent = data.LED4;
        })
        .catch(err => console.error('Status fetch error:', err));
    }

    document.getElementById('queryForm').addEventListener('submit', async (event) => {
      event.preventDefault();
      const formData = new FormData(event.target);
      const params   = new URLSearchParams(formData);
      const fullUrl  = `/query?${params}`;

      try {
        const response = await fetch(fullUrl);
        if(!response.ok) {
          document.getElementById('result').innerHTML =
            "<span style='color:red;'>Error fetching local calculation.</span>";
          return;
        }
        const result = await response.json();
        let htmlOut = "";
        htmlOut += `<strong>Google Query URL:</strong> <a href="${result.queriedURL}" target="_blank">Click Here</a><br>`;
        htmlOut += `<b>LED ${result.ledNumber} ON-time:</b> ${result.hours}h ${result.minutes}m ${result.seconds}s`;
        document.getElementById('result').innerHTML = htmlOut;
      } catch(err) {
        console.error('Query fetch error:', err);
        document.getElementById('result').innerHTML =
          "<span style='color:red;'>Error occurred while local calculation.</span>";
      }
    });

    window.onload = updateLEDStatus;
  </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// -----------------------------------------------------------
// handleWebButton()
// /led?lednumber=X&state=on/off
// Toggles LED, logs to CSV + Google
// -----------------------------------------------------------
void handleWebButton() {
  if (server.hasArg("lednumber") && server.hasArg("state")) {
    uint8_t ledNumber = server.arg("lednumber").toInt();
    String  st        = server.arg("state");
    bool    newState  = (st == "on");

    if (ledNumber < 1 || ledNumber > NUM_LEDS) {
      server.send(400, "text/plain", "Invalid LED number.");
      return;
    }

    toggleLED(ledNumber - 1, newState);
    uploadToGoogleSheet(ledNumber - 1, newState);

    server.send(204, "text/plain", "");
  } else {
    server.send(400, "text/plain", "Missing 'lednumber' or 'state'.");
  }
}

// -----------------------------------------------------------
// handleStatus()
// Returns current ON/OFF states of all LEDs as JSON
// e.g., {"LED1":"ON","LED2":"OFF","LED3":"OFF","LED4":"ON"}
// -----------------------------------------------------------
void handleStatus() {
  String statusJson = "{";
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    statusJson += "\"LED" + String(i+1) + "\":\"" + (ledStates[i] ? "ON" : "OFF") + "\"";
    if (i < NUM_LEDS - 1) statusJson += ",";
  }
  statusJson += "}";

  server.send(200, "application/json", statusJson);
}

// -----------------------------------------------------------
// handleQuery()
// -----------------------------------------------------------
void handleQuery() {
  if (!server.hasArg("lednumber") || !server.hasArg("startDate") ||
      !server.hasArg("startTime") || !server.hasArg("endDate") ||
      !server.hasArg("endTime")) {
    server.send(400, "text/plain", "Missing required parameters");
    return;
  }

  String ledNumberStr = server.arg("lednumber");
  String startDate    = server.arg("startDate");
  String startTime    = server.arg("startTime");
  String endDate      = server.arg("endDate");
  String endTime      = server.arg("endTime");

  uint8_t ledNumber   = ledNumberStr.toInt();
  if (ledNumber < 1 || ledNumber > NUM_LEDS) {
    server.send(400, "text/plain", "Invalid LED number.");
    return;
  }

  // Debug prints
  Serial.println("[QUERY] Time-Range calculation requested");
  Serial.printf("[QUERY] LED = %d, Range: %s %s -> %s %s\n",
                ledNumber, startDate.c_str(), startTime.c_str(), endDate.c_str(), endTime.c_str());

  // Print entire CSV for debugging
  // printEntireCSVtoSerial("handleQuery() called: printing CSV content...");

  // Build the Google Link, but do NOT fetch it
  String googleURL = String(googleScriptURL) +
                     "?lednumber=" + ledNumber +
                     "&startDate=" + startDate +
                     "&startTime=" + startTime +
                     "&endDate="   + endDate +
                     "&endTime="   + endTime;

  // Local calculation
  String localResult = localCalculateOnTime(ledNumber, startDate, startTime, endDate, endTime);

  // parse localResult (JSON) => hours, minutes, seconds
  int hIdx = localResult.indexOf("\"hours\":");
  int mIdx = localResult.indexOf("\"minutes\":");
  int sIdx = localResult.indexOf("\"seconds\":");
  long hours   = 0;
  long minutes = 0;
  long seconds = 0;

  if(hIdx!=-1 && mIdx!=-1 && sIdx!=-1) {
    hours   = localResult.substring(hIdx+8, localResult.indexOf(',', hIdx)).toInt();
    minutes = localResult.substring(mIdx+10, localResult.indexOf(',', mIdx)).toInt();
    int secEnd = localResult.indexOf('}', sIdx);
    seconds = localResult.substring(sIdx+10, secEnd).toInt();
  }

  // Build final JSON
  // e.g.: {"ledNumber":2, "queriedURL":"...", "hours":1, "minutes":30, "seconds":10}
  String finalJSON = "{";
  finalJSON += "\"ledNumber\":" + String(ledNumber) + ",";
  finalJSON += "\"queriedURL\":\"" + googleURL + "\",";
  finalJSON += "\"hours\":" + String(hours) + ",";
  finalJSON += "\"minutes\":" + String(minutes) + ",";
  finalJSON += "\"seconds\":" + String(seconds);
  finalJSON += "}";

  // Send to user
  server.send(200, "application/json", finalJSON);
}

// -----------------------------------------------------------
// createCSVIfMissing()
// If /led_log.csv doesn't exist, create it with a header row
// -----------------------------------------------------------
void createCSVIfMissing() {
  if(!SPIFFS.exists(CSV_FILE_PATH)) {
    File f = SPIFFS.open(CSV_FILE_PATH, FILE_WRITE);
    if(f) {
      f.println("Datetime,LED,Status"); // optional header
      f.close();
      Serial.println("[INFO] Created new /led_log.csv with header.");
    } else {
      Serial.println("[ERROR] Failed to create /led_log.csv");
    }
  } else {
    Serial.println("[INFO] /led_log.csv already exists.");
  }
}

// -----------------------------------------------------------
// appendCSVEntry()
// Appends a line to /led_log.csv
// e.g. "2023-09-06 12:34:56,1,ON"
// -----------------------------------------------------------
void appendCSVEntry(const String& datetime, uint8_t ledNumber, bool isOn) {
  File f = SPIFFS.open(CSV_FILE_PATH, FILE_APPEND);
  if(!f) {
    Serial.println("[ERROR] Cannot open /led_log.csv for append!");
    return;
  }
  String statusStr = isOn ? "ON" : "OFF";
  String line = datetime + "," + String(ledNumber) + "," + statusStr;
  f.println(line);
  f.close();
  //Serial.println("[INFO] Appended local CSV: " + line);
}

// -----------------------------------------------------------
// printEntireCSVtoSerial()
// Utility to print the entire CSV file to Serial
// for debugging each time a user queries
// -----------------------------------------------------------
void printEntireCSVtoSerial(const char* reason) {
  Serial.println("[DEBUG] " + String(reason));
  File f = SPIFFS.open(CSV_FILE_PATH, FILE_READ);
  if(!f) {
    Serial.println("[ERROR] Could not open CSV for reading!");
    return;
  }
  Serial.println("--- CSV BEGIN ---");
  while(f.available()) {
    String line = f.readStringUntil('\n');
    if(line.length() == 0) break;
    Serial.println(line);
  }
  f.close();
  Serial.println("--- CSV END ---");
}

// -----------------------------------------------------------
// localCalculateOnTime()
// - Print each step in serial
// - Summation of ON durations from start->end
// - Return JSON e.g. {"hours":0,"minutes":32,"seconds":6}
// -----------------------------------------------------------
String localCalculateOnTime(uint8_t ledNumber,
                            const String& startDate, const String& startTime,
                            const String& endDate,   const String& endTime) 
{
  // Convert "YYYY-MM-DD" and "HH:MM" to time_t (startEpoch, endEpoch)
  struct tm tmStart = {0}, tmEnd = {0};

  // Parse start date/time
  sscanf(startDate.c_str(), "%d-%d-%d", &tmStart.tm_year, &tmStart.tm_mon, &tmStart.tm_mday);
  sscanf(startTime.c_str(), "%d:%d",    &tmStart.tm_hour, &tmStart.tm_min);
  tmStart.tm_year -= 1900;   
  tmStart.tm_mon  -= 1;      
  time_t startEpoch = mktime(&tmStart);

  // Parse end date/time
  sscanf(endDate.c_str(), "%d-%d-%d", &tmEnd.tm_year, &tmEnd.tm_mon, &tmEnd.tm_mday);
  sscanf(endTime.c_str(), "%d:%d",    &tmEnd.tm_hour, &tmEnd.tm_min);
  tmEnd.tm_year -= 1900;
  tmEnd.tm_mon  -= 1;
  time_t endEpoch = mktime(&tmEnd);

  if (startEpoch == -1 || endEpoch == -1 || endEpoch < startEpoch) {
    //Serial.println(F("[ERROR] Invalid time range in localCalculateOnTime"));
    return "{\"hours\":0,\"minutes\":0,\"seconds\":0}";
  }

  //Serial.printf("[DEBUG] startEpoch=%ld, endEpoch=%ld\n", (long)startEpoch, (long)endEpoch);

  File f = SPIFFS.open(CSV_FILE_PATH, FILE_READ);
  if (!f) {
    Serial.println(F("[ERROR] localCalculateOnTime: cannot open CSV"));
    return "{\"hours\":0,\"minutes\":0,\"seconds\":0}";
  }

  unsigned long totalOnMillis = 0;

  // Skip CSV header (if present)
  String header = f.readStringUntil('\n');
  //Serial.println("[DEBUG] CSV header line (skipped): " + header);

  // Read CSV line by line
  while (true) {
    String line = f.readStringUntil('\n');
    if (line.length() == 0) {
      // No more lines
      break;
    }

    // Print each line for debugging
    //Serial.println("[DEBUG] Read line: " + line);

    int firstComma  = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    if (firstComma == -1 || secondComma == -1) {
      //Serial.println("[DEBUG] Malformed line, skipping: " + line);
      continue;
    }

    // Extract dateTimeStr, ledStr, statStr, then trim
    String dateTimeStr = line.substring(0, firstComma);
    String ledStr      = line.substring(firstComma + 1, secondComma);
    String statStr     = line.substring(secondComma + 1);

    // Remove trailing/newline spaces
    dateTimeStr.trim();
    ledStr.trim();
    statStr.trim();

    // Check LED match
    uint8_t csvLED = ledStr.toInt();
    if (csvLED != ledNumber) {
      // Serial.printf("[DEBUG] LED mismatch (found %d, needed %d), skipping\n", csvLED, ledNumber);
      continue;
    }

    // Parse dateTime => "YYYY-MM-DD HH:MM:SS"
    struct tm thisTm = {0};
    int year, mon, day, hh, mm, ss;
    if (sscanf(dateTimeStr.c_str(), "%d-%d-%d %d:%d:%d",
               &year, &mon, &day, &hh, &mm, &ss) == 6) {
      thisTm.tm_year = year - 1900;
      thisTm.tm_mon  = mon - 1;
      thisTm.tm_mday = day;
      thisTm.tm_hour = hh;
      thisTm.tm_min  = mm;
      thisTm.tm_sec  = ss;
    } else {
      // Serial.println("[DEBUG] Could not parse dateTime, skipping: " + dateTimeStr);
      continue;
    }

    time_t thisEpoch = mktime(&thisTm);

    // Check if in user-specified range
    if (thisEpoch < startEpoch || thisEpoch > endEpoch) {
      // Serial.printf("[DEBUG] Timestamp out of range (epoch=%ld), skipping\n", (long)thisEpoch);
      continue;
    }

    // If line is ON, search for next OFF or use endEpoch
    if (statStr.equalsIgnoreCase("ON")) {
      // Serial.printf("[DEBUG] Found ON at epoch=%ld => %s\n", (long)thisEpoch, line.c_str());
      time_t offTime = endEpoch;  // default if no OFF found

      // We'll read subsequent lines to find OFF
      while (true) {
        if (!f.available()) {
          //Serial.println("[DEBUG] Reached end of file, no more OFF lines");
          break;
        }
        String nextLine = f.readStringUntil('\n');
        if (nextLine.length() == 0) {
          //Serial.println("[DEBUG] Reached empty line / EOF while searching for OFF");
          break;
        }

        int fc = nextLine.indexOf(',');
        int sc = nextLine.indexOf(',', fc + 1);
        if (fc == -1 || sc == -1) {
          //Serial.println("[DEBUG] Malformed nextLine while searching OFF: " + nextLine);
          continue;
        }

        String dt2  = nextLine.substring(0, fc);
        String led2 = nextLine.substring(fc + 1, sc);
        String st2  = nextLine.substring(sc + 1);

        // Trim them
        dt2.trim();
        led2.trim();
        st2.trim();

        // parse dt2 => "YYYY-MM-DD HH:MM:SS"
        struct tm tm2 = {0};
        int y2, m2, d2, hh2, mi2, ss2;
        if (sscanf(dt2.c_str(), "%d-%d-%d %d:%d:%d", &y2, &m2, &d2, &hh2, &mi2, &ss2) == 6) {
          tm2.tm_year = y2 - 1900;
          tm2.tm_mon  = m2 - 1;
          tm2.tm_mday = d2;
          tm2.tm_hour = hh2;
          tm2.tm_min  = mi2;
          tm2.tm_sec  = ss2;
        }
        time_t e2 = mktime(&tm2);

        if (led2.toInt() == ledNumber && st2.equalsIgnoreCase("OFF")
            && e2 >= startEpoch && e2 <= endEpoch) {
          offTime = e2;
          //Serial.printf("[DEBUG] Found matching OFF => epoch=%ld: %s\n", (long)offTime, nextLine.c_str());
          break;
        }
      } // end while searching for OFF

      // Add difference
      if (offTime >= thisEpoch) {
        unsigned long diffMs = (offTime - thisEpoch) * 1000UL;
        unsigned long oldTotal = totalOnMillis; // store previous total for debug
        totalOnMillis += diffMs;
        //Serial.printf("[DEBUG] ON segment => start=%ld, off=%ld, diffMs=%lu\n", (long)thisEpoch, (long)offTime, diffMs);
         //Serial.printf("[DEBUG] ADDED %lu ms to total. Old total=%lu, new total=%lu\n", totalOnMillis);
      }
    } else {
      Serial.printf("[DEBUG] Line says LED was '%s', not 'ON', skipping this line.\n",
                    statStr.c_str());
    }
  } 

  f.close();

  // Convert totalOnMillis to H/M/S
  unsigned long totalSecs = totalOnMillis / 1000UL;
  unsigned long hours   = totalSecs / 3600UL;
  unsigned long minutes = (totalSecs % 3600UL) / 60UL;
  unsigned long seconds = (totalSecs % 60UL);

  Serial.printf("[DEBUG] ON time for selected LED => %lu h, %lu m, %lu s\n",  hours, minutes, seconds);
  Serial.printf("[JSON] RAW-Json from Scripts URL => {'hours':%lu, 'minutes':%lu, 'seconds':%lu}\n",  hours, minutes, seconds);

  // Return final JSON
  char jsonBuf[100];
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"hours\":%lu,\"minutes\":%lu,\"seconds\":%lu}",
           hours, minutes, seconds);
  return String(jsonBuf);
}