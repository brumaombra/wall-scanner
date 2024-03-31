/*#include <Arduino.h>
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
#define upperLED 17
#define centralLED 16
#define lowerLED 4
#define BUTTON 15
#define REDCH 0
#define YELLCH 1

// Variabili globali
PS2MouseHandler mouse(MOUSE_CLOCK, MOUSE_DATA, PS2_MOUSE_REMOTE); // Istanza mouse
unsigned int stato = 0; // Stato switch
enum scanStatus { READY = 0, SCANNING = 1, ENDED = 2 }; // Stati della scansione
scanStatus currentScanStatus = READY; // Stato della scansione
unsigned long timerCounter = 0;
float delta = 0;
float Fi0 = 29;
int elapsedTime = 0;
bool timerRunning = false;
volatile bool printed = true;
unsigned int i = 0; // Per fare la media
float soglia = 0.5; // Soglia per sensibilità LED
unsigned long prevMillis = millis();
int XVal = 0, YVal = 0;
int Xprec = 0, Yprec = 0;
float Xcm = 0, Ycm = 0;
byte NCM = 3; // Numero di cm ogni quanto fare una misura
bool first = true; // Per capire se è la prima iterazione
bool OKXY = true; // Per capire se ho già misurato in un certo 
bool devMode = true; // Per capire se stampare i valori
const char accessPointSSID[] = "Wall-scanner"; // SSID access point
char csvString[1000] = ""; // Stringa per salvare i dati registrati
AsyncWebServer server(80); // Server web
AsyncWebSocket ws("/ws"); // WebSocket
volatile int connectedClients = 0; // Numero di client connessi
Preferences preferences; // Preferenze (Per savare la configurazione)
bool TX, RX, LastTx, LastRX;

// PWM per i LED
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

// Stampo valori
void stampaNormale(int X, int Y, float mag) {
    Serial.print(X);
    Serial.print(",");
    Serial.print(Y);
    Serial.print(",");
    Serial.println(mag, 1);
}

// Set dei pin
void setupPin() {
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(TXPIN, INPUT);
    pinMode(RXPIN, INPUT);
    pinMode(REDLED, OUTPUT);
    pinMode(GREENLED, OUTPUT);
    pinMode(centralLED, OUTPUT);
    pinMode(upperLED, OUTPUT);
    pinMode(lowerLED, OUTPUT);
    ledcSetup(REDCH, 1000, 8);
    ledcAttachPin(REDLED, REDCH);
    ledcSetup(YELLCH, 1000, 8);
    ledcAttachPin(GREENLED, YELLCH);
}



// Setup
void setup() {
    if (devMode) Serial.begin(115200); // Inizializzo la seriale
    // setupLittleFS(); // Setup LittleFS
    // readConfig(); // Leggo configurazione salvata
    // setupServer(); // Setup server web
    setupPin(); // Setup dei pin
    // setupMouse(); // Setup del mouse
    // xTaskCreatePinnedToCore(WebServerTask, "WebServerTask", 10000, NULL, 1, NULL, 0);
    if (devMode) Serial.println("Setup OK");
}

// Loop
void loop() {
    switch (stato) {
        case 0: // Stato iniziale
            if (!digitalRead(BUTTON)) {
                first = true;
                if (devMode) Serial.println("Pulsante premuto!");
                stato = 1;
            }
            break;

        case 1: // Taro la bobina
            if (first) {
                first = false;
                TX = digiralRead(TXPIN);
                RX = digiralRead(RXPIN);
                LastTX = TX;
                LastRX = RX;
                i = 0;
                delta = 0;
                timerRunning = false;
                delay(500);
            }

            if ((LastTX != TX) && TX) { // Fronte di salita del TX
                timerCounter = ESP.getCycleCount();
                timerRunning = true;
            }

            if ((LastRX != RX) && LastRX && timerRunning) { // Fronte di discesa del RX
                elapsedTime = ESP.getCycleCount() - timerCounter;
                timerRunning = false;
                i++;
                delta = delta + elapsedTime / 240;
                if (i > 600){
                    delta = delta / 601;
                    LEDPWM();
                    Serial.println(delta, 1);
                    i = 0;
                    delta = 0;
                }
            }
            
            if (!digitalRead(BUTTON)) {
                first = true;
                stato = 2;
                if (devMode) Serial.println("----------");
            }
            break;

        case 2: // Stato trappola
            Serial.println(Basta...);
            delay(1000);
            break;
    }
}    

*/