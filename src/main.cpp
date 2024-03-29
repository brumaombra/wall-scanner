/*

----------- WORKFLOW -----------
1. Accendo l'ESP
2. ESP pronto
3. Mi collego al WiFi
4. Parte il polling per capire cosa sta facendo l'ESP
5. La pagina web si sincronizza con l'ESP

Stati lettura:
- READY: l'ESP è in attesa di far partire una scansione
- SCANNING: La scansione è in corso
- ENDED: Scansione terminata

----------- WORKFLOW PAGINA WEB -----------
1. Parte il polling
    1a. Se la scansione non è ancora partita faccio vedere una pagina web che dice di premere il pulsante sull'ESP
    1b. Se la scansione è già in corso mi sincronizzo e faccio vedere la heath map
    1c. Se la scansione è terminata faccio vedere il risultato

*/

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
enum scanStatus { READY = 0, SCANNING = 1, ENDED = 2 }; // Stati della scansione
scanStatus currentScanStatus = READY; // Stato della scansione
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
bool OKXY = true; // Per capire se ho già misurato in un certo 
bool devMode = true; // Per capire se stampare i valori
const char accessPointSSID[] = "Wall-scanner"; // SSID access point
char csvString[1000] = ""; // Stringa per salvare i dati registrati
AsyncWebServer server(80); // Server web

// Start timer
void IRAM_ATTR startTimer() {
    // detachAllInterrupts();
    if (!timerRunning) { // Se il timer non è attivo, azzero il contatore
        timerCounter = ESP.getCycleCount();
        timerRunning = true;
    }
    // attachAllInterrupts();
}

// Stop timer 
void IRAM_ATTR stopTimer() {
    if (timerRunning) {
        elapsedTime = ESP.getCycleCount() - timerCounter;
        // detachAllInterrupts();
        timerRunning = false;
        printed = false;
    }
}

// Attivo gli interrupt
void attachAllInterrupts() {
    attachInterrupt(TXPIN, startTimer, RISING);
    attachInterrupt(RXPIN, stopTimer, FALLING);
}

// Disabilito gli interrupt
void detachAllInterrupts() {
    detachInterrupt(TXPIN);
    detachInterrupt(RXPIN);
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

// Creo la string CSV
void writeCsv(int X, int Y, float mag) {
    char tempBuffer[30];
    sprintf(tempBuffer, "%d,%d,%.1f;", X, Y, mag);
    strcat(csvString, tempBuffer);
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
		if (devMode) Serial.println("Errore durante la configurazione di LittleFS");
		return false;
	} else {
		return true;
	}
}

// Faccio partire il server web
bool setupServer() {
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html"); // Serve web page
	server.onNotFound([](AsyncWebServerRequest *request) { // Error handling
		request->send(404); // Page not found
	});

    // Prendo le impostazioni
	server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["resolution"] = NCM; // Risoluzione scansione
        size_t jsonLength = measureJson(doc) + 1; // Grandezza del documento JSON
		char json[jsonLength];
		serializeJson(doc, json, sizeof(json));
		request->send(200, "application/json", json); // Mando risposta
    });

    // Salvo le impostazioni
	server.on("/setSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
        String resolution = request->getParam("resolution")->value();
        NCM = resolution.toInt(); // Sovrascrivo risoluzione
        JsonDocument doc;
        doc["resolution"] = NCM; // Mando la variabile aggiornata al front-end
        size_t jsonLength = measureJson(doc) + 1; // Grandezza del documento JSON
		char json[jsonLength];
		serializeJson(doc, json, sizeof(json));
        request->send(200, "application/json", json); // Mando risposta
    });

    // Polling per capire cosa sta facendo l'ESP
	server.on("/readScan", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["status"] = currentScanStatus; // Stato lettura
        doc["data"] = csvString; // Stringa CSV completa
        size_t jsonLength = measureJson(doc) + 1; // Grandezza del documento JSON
		char json[jsonLength];
		serializeJson(doc, json, sizeof(json));
		request->send(200, "application/json", json); // Mando risposta
    });

    /* Faccio partire la scansione
    server.on("/startScan", HTTP_GET, [](AsyncWebServerRequest *request) {
        attachAllInterrupts(); // Attivo interrupt
        stato = 1; // Stato switch
        Serial.println("Scansione avviata...");
        JsonDocument doc;
        doc["status"] = "OK"; // Success
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json); // Mando risposta
    });
    */

    /* Fermo la scansione
    server.on("/stopScan", HTTP_GET, [](AsyncWebServerRequest *request) {
        detachAllInterrupts(); // Disabilito interrupt
        stato = 0; // Stato switch
        csvString[0] = '\0'; // Pulisco stringa CSV
        Serial.println("Scansione fermata...");
        JsonDocument doc;
        doc["status"] = "OK"; // Success
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json); // Mando risposta
    });
    */

    // Avvio access point e server
    if (!WiFi.softAP(accessPointSSID)) // Configuro l'ESP come access point
        return false; // Errore
    server.begin(); // Avvio il server web
    return true; // Tutto OK
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
        if (devMode) Serial.println("Errore mouse");
        return false;
    } else {
        return true;
    }
}

// Setup
void setup() {
    Serial.begin(115200); // Inizializzo la seriale
    setupLittleFS(); // Setup LittleFS
    setupServer(); // Setup server web
    setupPin(); // Setup dei pin
    setupMouse(); // Setup del mouse
    if (devMode) Serial.println("Setup OK");
}

// Loop
void loop() {
    switch (stato) {
        case 0: // Stato iniziale
            currentScanStatus = READY; // Setto stato READY
            if (!digitalRead(BUTTON)) {
                first = true;
                if (devMode) Serial.println("Pulsante premuto!");
                stato = 1;
                attachAllInterrupts(); // Attivo interrupt
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
                    if (devMode) Serial.println(delta, 1);
                    printed = true;
                    LedPWM();
                    stato = 1;
                }
            }

            if (!digitalRead(BUTTON)) {
                first = true;
                stato = 2;
                if (devMode) Serial.println("----------");
            }
            attachAllInterrupts();
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
                    if (devMode) Serial.println(Fi0, 1);
                    delay(1);
                    if (devMode) Serial.println("----------");
                    LedPWM();
                    delay(1000);
                    printed = true;
                    first = true;
                    stato = 3;
                }
            }
            attachAllInterrupts();
            break;

        case 3: // Controllo se mi sono mosso
            if (first) {
                first = false;
                detachAllInterrupts();
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
                    attachAllInterrupts();
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
                    if (devMode) stampaNormale(int(Xcm / NCM), int(Ycm / NCM), delta); // Stampo valori su seriale
                    currentScanStatus = SCANNING; // Setto stato SCANNING
                    writeCsv(int(Xcm / NCM), int(Ycm / NCM), delta); // Aggiungo al CSV
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
    }
}