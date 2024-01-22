// codice per leggere il tempo che intercorre tra un fronte e l'altro dei
// segnali del metal detector quando una certa distanza ? stata misurata con il
// mouse ottico codice per ESP32 V2.2 il clock dell'ESP32 di default ? 240MHz
// ATTENZIONE! Se il mouse non ? collegato e/o non ? propriamente alimentato, il
// codice si blocca nel setup aspettando che il mouse risponda per l'ESP32,
// quando richiama una funzione da interrupt, questa deve essere precaricata in
// RAM tramite l'attributo IRAM_ATTR (invece che ICACHE_RAM_ATTR per ESP826)
// coefficiente per mouse: 0.01151
// esempio di testo per creare un'immagine con python:
// dati = [((0, 0), 255), ((1, 0), 128), ((2, 0), 0),
//         ((0, 1), 0), ((1, 1), 64), ((2, 1), 255)]
// misura al centro del pixel quadrato e dimensione pixel parametrizzabile

// codice per leggere il tempo che intercorre tra un fronte e l'altro dei segnali del metal detector quando una certa distanza ? stata misurata con il mouse ottico
// codice per ESP32 V2.2
// il clock dell'ESP32 di default ? 240MHz
// ATTENZIONE! Se il mouse non ? collegato e/o non ? propriamente alimentato, il codice si blocca nel setup aspettando che il mouse risponda
// per l'ESP32, quando richiama una funzione da interrupt, questa deve essere precaricata in RAM tramite l'attributo IRAM_ATTR (invece che ICACHE_RAM_ATTR per ESP826)
// coefficiente per mouse: 0.01151
// esempio di testo per creare un'immagine con python:
// dati = [((0, 0), 255), ((1, 0), 128), ((2, 0), 0),
//        ((0, 1), 0), ((1, 1), 64), ((2, 1), 255)]
// misura al centro del pixel quadrato e dimensione pixel parametrizzabile

#include <Arduino.h>
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
volatile unsigned long timerCounter = 0;
volatile float delta = 0;
volatile float Fi0 = 29;
volatile int elapsedTime = 0;
volatile bool timerRunning = false;
volatile bool printed = true;
unsigned int i = 0; // Per fare la media
float soglia = 0.5; // Soglia per sensibilità LED
unsigned long prevMillis = millis();
int XVal = 0, YVal = 0;
int Xprec = 0, Yprec = 0;
float Xcm = 0, Ycm = 0;
byte NCM = 3; // Numero di cm ogni quanto fare una misura
bool first = true; // Per capire se è la prima iterazione
bool firstPython = true;
bool OKXY = true; // Per capire se ho già misurato in un certo pixel
const char *accessPointSSID = "Wall-scanner"; // SSID access point
AsyncWebServer server(80); // Server web

// Start timer
void IRAM_ATTR startTimer() {
    noInterrupts();
    if (!timerRunning) { // Se il timer non è attivo, azzero il contatore
        timerCounter = ESP.getCycleCount();
        timerRunning = true;
    }
    interrupts();
}

// Stop timer 
void IRAM_ATTR stopTimer() {
    if (timerRunning) {
        elapsedTime = ESP.getCycleCount() - timerCounter;
        noInterrupts();
        timerRunning = false;
        printed = false;
    }
}

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

// Gestisco LED per direzione scansione
void LEDUpDown(float Ycm, int Yprec) {
    // Accendo il LED centrale
    if (Ycm - int((Ycm / NCM) * NCM > float(NCM) / 3) && (Ycm - int(Ycm / NCM) * NCM < float(NCM) * 2 / 3)) {
        digitalWrite(centralLED, HIGH);
        digitalWrite(upperLED, LOW);
        digitalWrite(lowerLED, LOW);
    }

    // Accendo il LED in basso
    if (Ycm - int(Ycm / NCM) * NCM > float(NCM) * 2 / 3) {
        digitalWrite(centralLED, LOW);
        digitalWrite(upperLED, LOW);
        digitalWrite(lowerLED, HIGH);
    }

    // Accendo il LED in alto
    if (Ycm - int(Ycm / NCM) * NCM < float(NCM) / 3) {
        digitalWrite(centralLED, LOW);
        digitalWrite(upperLED, HIGH);
        digitalWrite(lowerLED, LOW);
    }
}

// Setup LittleFS
bool setupLittleFS() {
	if (!LittleFS.begin()) { // Check if LittleFS is mounted
		Serial.println("Errore durante la configurazione di LittleFS");
		return false;
	} else {
		return true;
	}
}

// faccio partire il server web
void setupServer() {
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html"); // Serve web page
	server.onNotFound([](AsyncWebServerRequest *request) { // Error handling
		request->send(404); // Page not found
	});

    // Prendo le impostazioni
	server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["resolution"] = NCM; // Risoluzione scansione
        String json;
        serializeJson(doc, json);
		request->send(200, "application/json", json); // Mando risposta
    });

    // Salvo le impostazioni
	server.on("/setSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
        String resolution = request->getParam("resolution")->value();
        NCM = resolution.toInt(); // Sovrascrivo risoluzione
        JsonDocument doc;
        doc["resolution"] = NCM; // Mando la variabile aggiornata al front-end
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json); // Mando risposta
    });

    // Avvio il server e access point
    server.begin(); // Avvio il server web
    WiFi.softAP(accessPointSSID); // Configuro l'ESP come access point
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

// Setup mouse
bool setupMouse() {
    if (mouse.initialise() != 0) { // Errore mouse
        Serial.println("Errore mouse");
        return false;
    } else {
        return true;
    }
}

// Setup
void setup() {
    noInterrupts();
    Serial.begin(115200); // Inizializzo la seriale
    setupLittleFS(); // Setup LittleFS
    setupServer(); // Setup server web
    setupPin(); // Setup dei pin
    setupMouse(); // Setup del mouse
    interrupts();
}

// Loop
void loop() {
    switch (stato) {
        case 0:
            if (!digitalRead(BUTTON)) {
                first = true;
                Serial.println("Pulsante premuto!");
                stato = 1;
                attachInterrupt(TXPIN, startTimer, RISING);
                attachInterrupt(RXPIN, stopTimer, FALLING);
            }
            break;

        case 1: // Taro la bobina
            if (first) {
                first = false;
                delay(1000);
            }

            if (!printed) {
                if (i < 60) {
                    delta = delta + elapsedTime / 240; // in micros
                    elapsedTime = 0;
                    i++;
                    delayMicroseconds(200);
                } else {
                    i = 0;
                    delta = delta / 61;
                    Serial.println(delta, 1);
                    printed = true;
                    LedPWM();
                    stato = 1;
                }
            }

            if (!digitalRead(BUTTON)) {
                first = true;
                stato = 2;
                Serial.println("----------");
            }
            interrupts();
            break;

        case 2: // Taro il delay
            if (!printed) {
                if (i < 5000) {
                    Fi0 = Fi0 + float(elapsedTime) / 240; // In micros
                    elapsedTime = 0;
                    i++;
                    delayMicroseconds(200);
                } else {
                    i = 0;
                    Fi0 = Fi0 / 5035;
                    Serial.println(Fi0, 1);
                    delay(1);
                    Serial.println("----------");
                    LedPWM();
                    delay(1000);
                    printed = true;
                    first = true;
                    stato = 3;
                }
            }
            interrupts();
            break;

        case 3: // Controllo se mi sono mosso
            if (first) {
                first = false;
                noInterrupts();
            }

            if (millis() - prevMillis > 200) {
                prevMillis = millis();
                LedPWM();
                mouse.get_data();
                XVal += mouse.x_movement();
                YVal += mouse.y_movement();
                Xcm = XVal * 0.01151;
                Ycm = YVal * 0.01151;
                LEDUpDown(Ycm, Yprec);
                if (Xprec != int(Xcm) / NCM | Yprec != int(Ycm) / NCM) {
                    Xprec = int(Xcm) / NCM;
                    Yprec = int(Ycm) / NCM;
                    OKXY = true;
                }

                if ((Xcm - int(Xcm / NCM) * NCM > float(NCM) / 3) && (Xcm - int(Xcm / NCM) * NCM < float(NCM) * 2 / 3) && OKXY) {
                    OKXY = false;
                    stato = 4;
                    interrupts();
                }
            }
            break;

        case 4: // Misuro magnetismo e torno a misurare
            if (!printed) {
                if (i < 500) {
                    delta = delta + elapsedTime / 240; // In micros
                    elapsedTime = 0;
                    i++;
                    delayMicroseconds(200);
                } else {
                    i = 0;
                    delta = delta / 501;
                    stampaNormale(int(Xcm / NCM), int(Ycm / NCM), delta);
                    printed = true;
                    LedPWM();
                    stato = 5;
                    first = true;
                }
            }
            break;

        case 5:
            stato = 3;
            break;

        case 6:
            break;
    }
}