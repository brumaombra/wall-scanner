#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <PS2MouseHandler.h>

#define MOUSE_DATA 18
#define MOUSE_CLOCK 5
#define TXPIN 21
#define RXPIN 19
#define REDLED 23
#define GREENLED 22
#define LOWBATLED 2
#define upperLED 4 // 17 if I want the lower LED to light up when I need to go up
#define centralLED 16
#define lowerLED 17 // 4 if I want the upper LED to light up when I need to go down
#define BUTTON 15
#define REDCH 0
#define YELLCH 1
#define BEEPER 12
#define BEEPCH 2

// Global variables
PS2MouseHandler mouse(MOUSE_CLOCK, MOUSE_DATA, PS2_MOUSE_REMOTE); // Mouse instance
unsigned int stato = 0; // Switch state
enum scanStatus { READY = 0, TUNING = 1, SCANNING = 2, ENDED = 3 }; // Scan states
scanStatus currentScanStatus = READY; // Scan state
unsigned long timerCounter = 0;
float delta = 0;
float Fi0 = 29;
int elapsedTime = 0;
unsigned int i = 0; // For averaging
const float soglia = 0.5; // Threshold for LED sensitivity
unsigned long prevMillis = millis();
int XVal = 0, YVal = 0;
int Xprec = 0, Yprec = 0;
float Xcm = 0, Ycm = 0;
byte NCM = 3; // Number of cm every how many measurements
bool normalizeValues = false; // To understand if normalize values
bool displayValuesOnMap = false; // If display values on heatmap
bool OKXY = true; // To understand if I have already measured in a certain
bool TXval, RXval, LastTX, LastRX; // PIN values
const char accessPointSSID[] = "Wall-scanner"; // Access point SSID
char csvString[10000] = ""; // String to save recorded data
AsyncWebServer server(80); // Web server
AsyncWebSocket ws("/ws"); // WebSocket
int connectedClients = 0; // Number of connected clients
Preferences preferences; // Preferences (To save the configuration)
bool devMode = true; // Development mode

// PWM for LEDs
void LedPWM() {
    if (delta > Fi0 + soglia) {
        ledcWrite(REDCH, 10 * (delta - (Fi0 + soglia)));
        ledcWrite(YELLCH, 0);
    }
    if (delta < Fi0 - soglia) {
        ledcWrite(YELLCH, 30 * (Fi0 - soglia - delta));
        ledcWrite(REDCH, 0);
    }
    if ((delta > Fi0 - soglia) && (delta < Fi0 + soglia)) {
        ledcWrite(REDCH, 0);
        ledcWrite(YELLCH, 0);
    }
}

// Add reference value to CSV
void addReferenceValueToCsv() {
    sprintf(csvString, "%.1f;", Fi0);
}

// Create CSV string
void writeCsv(int X, int Y, float mag) {
    char tempBuffer[30];
    sprintf(tempBuffer, "%d,%d,%.1f;", X, Y, mag);
    strcat(csvString, tempBuffer);
    if (devMode) Serial.println(tempBuffer); // Print current measurement
}

// Manage LEDs for scan direction
void LEDUpDown(float Ycm, int Yprec) {
    if (Ycm - int((Ycm / NCM) * NCM > float(NCM) / 3) && (Ycm - int(Ycm / NCM) * NCM < float(NCM) * 2 / 3)) { // Turn on central LED
        digitalWrite(centralLED, HIGH);
        digitalWrite(upperLED, LOW);
        digitalWrite(lowerLED, LOW);
    }
    if (Ycm - int(Ycm / NCM) * NCM > float(NCM) * 2 / 3) { // Turn on lower LED
        digitalWrite(centralLED, LOW);
        digitalWrite(upperLED, LOW);
        digitalWrite(lowerLED, HIGH);
    }
    if (Ycm - int(Ycm / NCM) * NCM < float(NCM) / 3) { // Turn on upper LED
        digitalWrite(centralLED, LOW);
        digitalWrite(upperLED, HIGH);
        digitalWrite(lowerLED, LOW);
    }
}

// Setup LittleFS
bool setupLittleFS() {
	if (!LittleFS.begin()) { // Check if LittleFS is mounted
		if (devMode) Serial.println("Error during LittleFS configuration");
		return false;
	} else {
		return true;
	}
}

// Read saved configuration
bool readConfig() {
    preferences.begin("config", false);
    NCM = preferences.getInt("resolution", NCM); // Scan resolution
    normalizeValues = preferences.getBool("normalize", normalizeValues); // If display values
    displayValuesOnMap = preferences.getBool("displayValues", displayValuesOnMap); // If display values on heatmap
    preferences.end();
    if (devMode) Serial.println("Configuration loaded correctly");
    return true; // All OK
}

// Save configuration
bool writeConfig() {
    preferences.begin("config", false);
    preferences.putInt("resolution", NCM); // Scan resolution
    preferences.putBool("normalize", normalizeValues); // If display values
    preferences.putBool("displayValues", displayValuesOnMap); // If display values on heatmap
    preferences.end();
    if (devMode) Serial.println("Configuration saved correctly");
    return true; // All OK
}

// Send message via WebSocket
void sendSocketMessage() {
    if (connectedClients <= 0) return; // If no clients connected, do nothing
    if (devMode) Serial.println("Send message via WebSocket");
    JsonDocument doc;
    doc["status"] = currentScanStatus; // Reading state
    doc["data"] = csvString; // Complete CSV string
    size_t jsonLength = measureJson(doc) + 1; // Size of JSON document
    char json[jsonLength];
    serializeJson(doc, json, sizeof(json));
    ws.textAll(json); // Send the message
}

// Send message via WebSocket to front-end with polling
void pollingSocketClient(const int frequence) {
    if (millis() - prevMillis < frequence) return; // Only every tot time, otherwise exit
    prevMillis = millis(); // Update prevMillis
    sendSocketMessage(); // Send message via WebSocket
}

// Handle WebSocket response
void processSocketMessage(const char* message, size_t length) {
    // Read the message
}

// WebSocket event
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT: // Connection event
            connectedClients++; // Increment number of connected clients
            if (devMode) Serial.println("WebSocket connected");
            break;
        case WS_EVT_DISCONNECT: // Disconnection event
            connectedClients--; // Decrement number of connected clients
            if (devMode) Serial.println("WebSocket disconnected");
            break;
        case WS_EVT_DATA:
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0; // Add end of string character
                processSocketMessage((const char*)data, len); // Handle WebSocket response
            }
            break;
    }
}

// Start the web server
bool setupServer() {
    server.serveStatic("/home", LittleFS, "/").setDefaultFile("index.html"); // Serve web page
    server.serveStatic("/js", LittleFS, "/js"); // Serve web page
    server.serveStatic("/css", LittleFS, "/css"); // Serve web page
    server.serveStatic("/webfonts", LittleFS, "/webfonts"); // Serve web page
    server.onNotFound([](AsyncWebServerRequest *request) { // Error handling
		request->send(404); // Page not found
	});

    // Get settings
	server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["resolution"] = NCM; // Scan resolution
        doc["normalize"] = normalizeValues; // If display values
        doc["displayValues"] = displayValuesOnMap; // If display values on heatmap
        size_t jsonLength = measureJson(doc) + 1; // Size of JSON document
		char json[jsonLength];
		serializeJson(doc, json, sizeof(json));
		request->send(200, "application/json", json); // Send response
    });

    // Save settings
	server.on("/setSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
        String resolution = request->getParam("resolution")->value();
        NCM = resolution.toInt(); // Overwrite resolution
        normalizeValues = request->getParam("normalize")->value() == "true";
        displayValuesOnMap = request->getParam("displayValues")->value() == "true";
        writeConfig(); // Save configuration
        JsonDocument doc;
        doc["resolution"] = NCM; // Send updated variable to front-end
        doc["normalize"] = normalizeValues; // Send updated variable to front-end
        doc["displayValues"] = displayValuesOnMap; // Send updated variable to front-end
        size_t jsonLength = measureJson(doc) + 1; // Size of JSON document
		char json[jsonLength];
		serializeJson(doc, json, sizeof(json));
        request->send(200, "application/json", json); // Send response
    });

    // Add WebSocket event
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Start access point and server
    if (!WiFi.softAP(accessPointSSID)) // Configure ESP as access point
        return false; // Error
    server.begin(); // Start web server
    return true; // All OK
}

// Set pins
void setupPin() {
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(TXPIN, INPUT);
    pinMode(RXPIN, INPUT);
    pinMode(REDLED, OUTPUT);
    pinMode(GREENLED, OUTPUT);
    pinMode(LOWBATLED, OUTPUT);
    pinMode(centralLED, OUTPUT);
    pinMode(upperLED, OUTPUT);
    pinMode(lowerLED, OUTPUT);
    ledcSetup(REDCH, 1000, 8);
    ledcAttachPin(REDLED, REDCH);
    ledcSetup(YELLCH, 1000, 8);
    ledcAttachPin(GREENLED, YELLCH);
    ledcSetup(BEEPCH, 2000, 4); // 4 bit resolution, from 0 to 15
    ledcAttachPin(BEEPER, BEEPCH);
}

// Setup mouse
bool setupMouse() {
    if (mouse.initialise() != 0) { // Mouse error
        if (devMode) Serial.println("Mouse error");
        return false;
    } else {
        return true;
    }
}

// Turn on/off all LEDs
void turnOnOffAllLed(const bool on) {
    ledcWrite(REDCH, on ? 255 : 0); // Red LED
    ledcWrite(YELLCH, on ? 255 : 0); // Blue LED
    digitalWrite(LOWBATLED, on ? HIGH : LOW); // Low Battery LED
    digitalWrite(upperLED, on ? HIGH : LOW); // Upper LED
    digitalWrite(lowerLED, on ? HIGH : LOW); // Lower LED
    digitalWrite(centralLED, on ? HIGH : LOW); // Central LED
}

// Test all LEDs
void testAllLedSequence() {
    const int singleLedDelay = 200; // Delay between single LED on
    const int allLedDelay = 1000; // Delay between all LEDs on
    turnOnOffAllLed(false); // Turn off all LEDs
    ledcWrite(REDCH, 255); // Turn on red LED
    delay(singleLedDelay);
    ledcWrite(REDCH, 0); // Turn off red LED
    ledcWrite(YELLCH, 255); // Turn on blue LED
    delay(singleLedDelay);
    ledcWrite(YELLCH, 0); // Turn off blue LED
    digitalWrite(LOWBATLED, HIGH); // Turn on LOWBAT LED
    delay(singleLedDelay);
    digitalWrite(LOWBATLED, LOW); // Turn off LOWBAT LED
    delay(singleLedDelay);
    digitalWrite(upperLED, HIGH); // Turn on upper LED
    delay(singleLedDelay);
    digitalWrite(upperLED, LOW); // Turn off upper LED
    digitalWrite(centralLED, HIGH); // Turn on lower LED
    delay(singleLedDelay);
    digitalWrite(centralLED, LOW); // Turn off lower LED
    digitalWrite(lowerLED, HIGH); // Turn on central LED
    delay(singleLedDelay);
    digitalWrite(lowerLED, LOW); // Turn off central LED
    delay(singleLedDelay);
    turnOnOffAllLed(true); // Turn on all LEDs
    delay(singleLedDelay);
    turnOnOffAllLed(false); // Turn off all LEDs
}

// Initial beeper sequence
void testBeeper() {
    const int beepDelay = 50; // Delay between beeper on and off
    int j, freq = 500;
    for(j = 1; j < 6; j++) {
        ledcSetup(BEEPCH, freq * j, 4); // 4 bit resolution, from 0 to 15
        ledcAttachPin(BEEPER, BEEPCH);
        ledcWrite(BEEPCH, 7); // Turn on beeper
        delay(beepDelay);
        ledcWrite(BEEPCH, 0); // Turn off beeper
        delay(beepDelay);
    }

    delay(6 * beepDelay);
    ledcWrite(BEEPCH, 7); // Turn on beeper
    delay(6 * beepDelay);
    ledcWrite(BEEPCH, 0); // Turn off beeper
    delay(beepDelay);
}

// Single beep
void beep() {
    const int beepDelay = 50; // Delay between beeper on and off
    int freq = 3000;
    ledcSetup(BEEPCH, freq, 4); // 4 bit resolution, from 0 to 15
    ledcAttachPin(BEEPER, BEEPCH);
    ledcWrite(BEEPCH, 7); // Turn on beeper
    delay(beepDelay);
    ledcWrite(BEEPCH, 0); // Turn off beeper
    delay(beepDelay);
}

// Success blinking green LED (Total delay 1200ms)
void blinkingLedSequence(const bool success) {
    turnOnOffAllLed(false); // Turn off all LEDs
    for (int counter = 0; counter < 3; counter++) {
        digitalWrite(success ? centralLED : upperLED, LOW); // Turn off LED
        delay(200);
        digitalWrite(success ? centralLED : upperLED, HIGH); // Turn on LED
        delay(200);
    }
    digitalWrite(success ? centralLED : upperLED, LOW); // Turn off LED
}

// Blinking two red LEDs (Total delay 1200ms)
void blikingErrorSequence(const bool keepOn) {
    turnOnOffAllLed(false); // Turn off all LEDs
    for (int counter = 0; counter < 3; counter++) {
        digitalWrite(upperLED, LOW); // Turn off LED
        digitalWrite(lowerLED, LOW); // Turn off LED
        delay(200);
        digitalWrite(upperLED, HIGH); // Turn on LED
        digitalWrite(lowerLED, HIGH); // Turn on LED
        delay(200);
    }
    if (!keepOn) turnOnOffAllLed(false); // Turn off LEDs if necessary
}

// Setup
void setup() {
    if (devMode) Serial.begin(115200); // Initialize serial
    setupPin(); // Setup pins
    testAllLedSequence(); // Test all LEDs
    testBeeper(); // Test beeper
    readConfig(); // Read saved configuration
    const bool initialTest = setupLittleFS() && setupMouse() && setupServer(); // Setup critical functions
    if (initialTest) { // Setup OK
        blinkingLedSequence(true); // If all ok blink green
        if (devMode) Serial.println("Setup OK");
        if (devMode) Serial.println("Ready for new scan, press the button to start calibration...");
        turnOnOffAllLed(true); // Turn on all LEDs
    } else { // Setup KO
        blikingErrorSequence(true); // If error blink red and leave LEDs on
        if (devMode) Serial.println("An error occurred during setup, execution stopped");
        while (true) delay(1000); // Stop the program
    }
}

// Reset variables used in loop
void resetVariabiliLoop() {
    LastTX = digitalRead(TXPIN); // Read TX value
    LastRX = digitalRead(RXPIN); // Read RX value
    i = timerCounter = 0; // Reset variables
    prevMillis = millis(); // Reset millis
}

// Go to state 5
void navToStato5() {
    resetVariabiliLoop(); // Prepare variables for next state
    currentScanStatus = ENDED; // Scan ended
    sendSocketMessage(); // Send message via WebSocket
    if (devMode) Serial.println("Scan finished!");
    blinkingLedSequence(true); // Blink green LED + delay to avoid double button press
    stato = 5; // Go to final state
}

// Initial state
void stato0() {
    pollingSocketClient(3000); // Send status every 3 seconds
    
    // If button not pressed do nothing
    if (digitalRead(BUTTON)) return;
    beep(); // Beep once
    resetVariabiliLoop(); // Prepare variables for next state
    XVal = 0; // Reset coordinates
    YVal = 0; // Reset coordinates
    turnOnOffAllLed(false); // Turn off all LEDs
    currentScanStatus = TUNING; // Calibration started
    if (devMode) Serial.println("Button pressed! Starting coil tuning...");
    delay(1000); // Delay to avoid double button press
    stato = 1; // Go to state 1
}

// Tune the coil
void stato1() {
    TXval = digitalRead(TXPIN); // Read TX value
    RXval = digitalRead(RXPIN); // Read RX value
    if (!LastTX && TXval) // Rising edge of TX
        timerCounter = ESP.getCycleCount(); // Start timer
    if (!RXval && LastRX && timerCounter != 0) { // Falling edge of RX
        elapsedTime = ESP.getCycleCount() - timerCounter; // Stop timer
        delta = delta + elapsedTime / 240;
        timerCounter = 0; // Reset timer
        if (i > 600) {
            delta = delta / 601; // Calculate average
            LedPWM(); // Manage magnetism LED
            if (devMode) Serial.println(delta, 1);
            sprintf(csvString, "%.1f", delta); // Fill CSV variable with single value
            pollingSocketClient(200); // Send data
            i = 0; // Reset counter
            delta = 0; // Reset delta
        } else {
            i++; // Increment counter
        }
    }

    LastTX = TXval; // Update TX value
    LastRX = RXval; // Update RX value

    if (digitalRead(BUTTON)) return; // If button not pressed exit
    beep(); // Beep once;
    if (devMode) Serial.print("Reference value: ");
    resetVariabiliLoop(); // Prepare variables for next state
    delay(1000); // Delay to avoid double button press
    stato = 2; // Go to next state
}

// Tune the delay
void stato2() {
    TXval = digitalRead(TXPIN); // Read TX value
    RXval = digitalRead(RXPIN); // Read RX value
    if (!LastTX && TXval) // Rising edge of TX
        timerCounter = ESP.getCycleCount();
    if (!RXval && LastRX && timerCounter != 0) { // Falling edge of RX
        elapsedTime = ESP.getCycleCount() - timerCounter;
        Fi0 = Fi0 + elapsedTime / 240;
        timerCounter = 0; // Reset timer
        if (i > 5000) {
            Fi0 = Fi0 / 5001;
            if (devMode) Serial.println(Fi0, 1); // Print reference value
            addReferenceValueToCsv(); // Add reference value to CSV
            sendSocketMessage(); // Send message via WebSocket
            blinkingLedSequence(true); // Blink green LED
            delay(1800); // Additional delay to reach 3000ms
            beep(); // Beep once;
            i = 0; // Reset counter
            currentScanStatus = SCANNING; // Set SCANNING status
            sendSocketMessage(); // Send message via WebSocket
            if (devMode) Serial.println("Scan started...");
            stato = 3; // Go to next state
        } else {
            i++; // Increment counter
        }
    }

    LastTX = TXval; // Update TX value
    LastRX = RXval; // Update RX value
}

// Check if I moved
void stato3() {
    if (millis() - prevMillis < 200) return; // If not passed 200 ms, do nothing
    prevMillis = millis(); // Update prevMillis
    LedPWM(); // Manage magnetism LED
    mouse.get_data(); // Read data from mouse
    XVal += mouse.x_movement(); // Get x movement
    YVal += mouse.y_movement(); // Get y movement
    Xcm = XVal * 0.01151; // Convert to cm
    Ycm = YVal * 0.01151; // Convert to cm
    LEDUpDown(Ycm, Yprec); // Manage LED based on mouse movement
    if (Xprec != int(Xcm) / NCM | Yprec != int(Ycm) / NCM) {
        Xprec = int(Xcm) / NCM;
        Yprec = int(Ycm) / NCM;
        OKXY = true;
    }
    if ((Xcm - int(Xcm / NCM) * NCM > float(NCM) / 3) && (Xcm - int(Xcm / NCM) * NCM < float(NCM) * 2 / 3) && OKXY) {
        OKXY = false;
        stato = 4; // Go to next state
    }

    // Check button press
    if (!digitalRead(BUTTON)){
        beep(); // Beep once;
        navToStato5(); // Go to state 5
    }
}

// Measure magnetism and go back to measure
void stato4() {
    TXval = digitalRead(TXPIN); // Read TX value
    RXval = digitalRead(RXPIN); // Read RX value
    if (!LastTX && TXval) // Rising edge of TX
        timerCounter = ESP.getCycleCount();
    if (!RXval && LastRX && timerCounter != 0) { // Falling edge of RX
        elapsedTime = ESP.getCycleCount() - timerCounter;
        delta = delta + elapsedTime / 240;
        timerCounter = 0; // Reset
        if (i > 500) {
            i = 0; // Reset counter
            delta = delta / 501;
            writeCsv(int(Xcm / NCM), int(Ycm / NCM), delta); // Add to CSV
            sendSocketMessage(); // Send message via WebSocket
            LedPWM(); // Manage magnetism LED
            stato = 3; // Go back to state 3
        } else {
            i++; // Increment counter
        }
    }

    LastTX = TXval; // Update TX value
    LastRX = RXval; // Update RX value

    // Check button press
    if (!digitalRead(BUTTON)){
        beep(); // Beep once;
        navToStato5(); // Go to state 5
    }
}

// End scan and send complete data to front-end
void stato5() {
    pollingSocketClient(3000); // Send complete heatmap every 3 seconds

    // If button not pressed exit
    if (digitalRead(BUTTON)) return;
    beep(); // Beep once
    Fi0 = 29; // Reset tare
    csvString[0] = '\0'; // Empty CSV
    currentScanStatus = READY; // Ready for new scan
    sendSocketMessage(); // Send message via WebSocket
    resetVariabiliLoop(); // Prepare variables for next state
    if (devMode) Serial.println("Ready for new scan, press the button to start calibration");
    delay(1000); // Delay to avoid double button press
    stato = 0; // Restart cycle
}

// Loop
void loop() {
    switch (stato) {
        case 0: // Initial state
            stato0(); // Handle state 0
            break;
        case 1: // Tune the coil
            stato1(); // Handle state 1
            break;
        case 2: // Tune the delay
            stato2(); // Handle state 2
            break;
        case 3: // Check if I moved
            stato3(); // Handle state 3
            break;
        case 4: // Measure magnetism and go back to measure
            stato4(); // Handle state 4
            break;
        case 5: // End scan and send complete data to front-end
            stato5(); // Handle state 5
            break;
    }
}