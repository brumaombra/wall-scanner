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
enum scanStatus { READY = 0, TUNING = 1, SCANNING = 2, ENDED = 3 }; // Stati della scansione
scanStatus currentScanStatus = READY; // Stato della scansione
unsigned long timerCounter = 0;
float delta = 0;
float Fi0 = 29;
int elapsedTime = 0;
unsigned int i = 0; // Per fare la media
const float soglia = 0.5; // Soglia per sensibilità LED
unsigned long prevMillis = millis();
int XVal = 0, YVal = 0;
int Xprec = 0, Yprec = 0;
float Xcm = 0, Ycm = 0;
byte NCM = 3; // Numero di cm ogni quanto fare una misura
bool OKXY = true; // Per capire se ho già misurato in un certo 
bool devMode = true; // Per capire se stampare i valori
bool TXval, RXval, LastTX, LastRX; // Valori PIN
const char accessPointSSID[] = "Wall-scanner"; // SSID access point
char csvString[10000] = ""; // Stringa per salvare i dati registrati
AsyncWebServer server(80); // Server web
AsyncWebSocket ws("/ws"); // WebSocket
int connectedClients = 0; // Numero di client connessi
Preferences preferences; // Preferenze (Per savare la configurazione)

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

// Aggiungo il valore di riferimento al CSV
void addReferenceValueToCsv() {
    // char tempBuffer[30];
    sprintf(csvString, "%.1f;", Fi0);
    // strcat(csvString, tempBuffer);
}

// Creo la string CSV
void writeCsv(int X, int Y, float mag) {
    char tempBuffer[30];
    sprintf(tempBuffer, "%d,%d,%.1f;", X, Y, mag);
    strcat(csvString, tempBuffer);
    if (devMode) Serial.println(tempBuffer); // Stampo misura corrente
}

// Gestisco LED per direzione scansione
void LEDUpDown(float Ycm, int Yprec) {
    if (Ycm - int((Ycm / NCM) * NCM > float(NCM) / 3) && (Ycm - int(Ycm / NCM) * NCM < float(NCM) * 2 / 3)) { // Accendo il LED centrale
        digitalWrite(centralLED, HIGH);
        digitalWrite(upperLED, LOW);
        digitalWrite(lowerLED, LOW);
    }
    if (Ycm - int(Ycm / NCM) * NCM > float(NCM) * 2 / 3) { // Accendo il LED in basso
        digitalWrite(centralLED, LOW);
        digitalWrite(upperLED, LOW);
        digitalWrite(lowerLED, HIGH);
    }
    if (Ycm - int(Ycm / NCM) * NCM < float(NCM) / 3) { // Accendo il LED in alto
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

// Leggo la configurazione salvata
bool readConfig() {
    preferences.begin("config", false);
    NCM = preferences.getInt("resolution", NCM); // Risoluzione scansione
    preferences.end();
    if (devMode) Serial.println("Configurazione caricata correttamente");
    return true; // Tutto OK
}

// Salvo la configurazione
bool writeConfig() {
    preferences.begin("config", false);
    preferences.putInt("resolution", NCM); // Risoluzione scansione
    preferences.end();
    if (devMode) Serial.println("Configurazione salvata correttamente");
    return true; // Tutto OK
}

// Mando il messaggio via WebSocket
void sendSocketMessage() {
    if (connectedClients <= 0) return; // Se non ci sono client connessi, non faccio nulla
    if (devMode) Serial.println("Invio messaggio via WebSocket");
    JsonDocument doc;
    doc["status"] = currentScanStatus; // Stato lettura
    doc["data"] = csvString; // Stringa CSV completa
    size_t jsonLength = measureJson(doc) + 1; // Grandezza del documento JSON
    char json[jsonLength];
    serializeJson(doc, json, sizeof(json));
    ws.textAll(json); // Invio il messaggio
}

// Mando il messaggio tramite WebSocket al front-end con polling
void pollingSocketClient(const int frequence) {
    if (millis() - prevMillis < frequence) return; // Solo ogni tot tempo, se no esco
    prevMillis = millis(); // Aggiorno prevMillis
    sendSocketMessage(); // Mando il messaggio via WebSocket
}

// Gestisco la risposta del Websocket
void processSocketMessage(const char* message, size_t length) {
    // Leggo il messaggio
}

// Evento per Websocket
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT: // Evento di connessione
            connectedClients++; // Incrementa il numero di client connessi
            if (devMode) Serial.println("WebSocket connected");
            break;
        case WS_EVT_DISCONNECT: // Evento di disconnessione
            connectedClients--; // Decrementa il numero di client connessi
            if (devMode) Serial.println("WebSocket disconnected");
            break;
        case WS_EVT_DATA:
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0; // Aggiungo il carattere di fine stringa
                processSocketMessage((const char*)data, len); // Gestisco la risposta del Websocket
            }
            break;
    }
}

// Faccio partire il server web
bool setupServer() {
    server.serveStatic("/home", LittleFS, "/").setDefaultFile("index.html"); // Serve web page
    server.serveStatic("/js", LittleFS, "/js"); // Serve web page
    server.serveStatic("/css", LittleFS, "/css"); // Serve web page
    server.serveStatic("/webfonts", LittleFS, "/webfonts"); // Serve web page
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
        writeConfig(); // Salvo la configurazione
        JsonDocument doc;
        doc["resolution"] = NCM; // Mando la variabile aggiornata al front-end
        size_t jsonLength = measureJson(doc) + 1; // Grandezza del documento JSON
		char json[jsonLength];
		serializeJson(doc, json, sizeof(json));
        request->send(200, "application/json", json); // Mando risposta
    });

    // Aggiungo evento per Websocket
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

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
    if (devMode) Serial.begin(115200); // Inizializzo la seriale
    setupLittleFS(); // Setup LittleFS
    readConfig(); // Leggo configurazione salvata
    setupServer(); // Setup server web
    setupPin(); // Setup dei pin
    setupMouse(); // Setup del mouse
    if (devMode) Serial.println("Setup OK");
    if (devMode) Serial.println("Pronto per nuova scansione, premi il pulsante per iniziare la calibrazione");
}

// Lampeggio di successo LED verde (Delay totale 1800ms)
void successBlinkingLed() {
    for (int counter = 0; counter < 3; counter++) {
        digitalWrite(GREENLED, HIGH);
        delay(300);
        digitalWrite(GREENLED, LOW);
        delay(300);
    }
}

// Resetto le variabili usate nel loop
void resetVariabiliLoop() {
    LastTX = digitalRead(TXPIN); // Leggo valore TX
    LastRX = digitalRead(RXPIN); // Leggo valore RX
    i = timerCounter = 0; // Reset variabili
    prevMillis = millis(); // Reset millis
}

// Passo allo stato 5
void navToStato5() {
    resetVariabiliLoop(); // Preparo variabili per il prossimo stato
    currentScanStatus = ENDED; // Scansione terminata
    sendSocketMessage(); // Mando il messaggio via WebSocket
    if (devMode) Serial.println("Scansione terminata!");
    successBlinkingLed(); // Lampeggio LED verde + delay per evitare doppia pressione tasti
    stato = 5; // Passo allo stato finale
}

// Stato iniziale
void stato0() {
    pollingSocketClient(3000); // Mando stato ogni 3 secondi
    
    // Se il pulsante non è premuto non faccio niente
    if (digitalRead(BUTTON)) return;
    resetVariabiliLoop(); // Preparo variabili per il prossimo stato
    currentScanStatus = TUNING; // Calibrazione iniziata
    if (devMode) Serial.println("Pulsante premuto! Comincio taratura bobina...");
    delay(1000); // Delay per evitare doppia pressione tasti
    stato = 1; // Passo allo stato 1
}

// Taro la bobina
void stato1() {
    TXval = digitalRead(TXPIN); // Leggo valore TX
    RXval = digitalRead(RXPIN); // Leggo valore RX
    if (!LastTX && TXval) // Fronte di salita del TX
        timerCounter = ESP.getCycleCount(); // Faccio partire il timer
    if (!RXval && LastRX && timerCounter != 0) { // Fronte di discesa del RX
        elapsedTime = ESP.getCycleCount() - timerCounter; // Stop timer
        delta = delta + elapsedTime / 240;
        timerCounter = 0; // Reset timer
        if (i > 600) {
            delta = delta / 601; // Calcolo media
            LedPWM(); // Gestione LED megnetismo
            if (devMode) Serial.println(delta, 1);
            sprintf(csvString, "%.1f", delta); // Riempio la variabile del CSV con un solo valore
            pollingSocketClient(1000); // Mando dato ogni secondo
            i = 0; // Reset contatore
            delta = 0; // Reset delta
        } else {
            i++; // Incremento contatore
        }
    }

    LastTX = TXval; // Aggiorno valore TX
    LastRX = RXval; // Aggiorno valore RX

    if (digitalRead(BUTTON)) return; // Se il pulsante non è premuto esco
    if (devMode) Serial.print("Valore di riferimento: ");
    resetVariabiliLoop(); // Preparo variabili per il prossimo stato
    delay(1000); // Delay per evitare doppia pressione tasti
    stato = 2; // Passo al prossimo stato
}

// Taro il delay
void stato2() {
    TXval = digitalRead(TXPIN); // Leggo valore TX
    RXval = digitalRead(RXPIN); // Leggo valore RX
    if (!LastTX && TXval) // Fronte di salita del TX
        timerCounter = ESP.getCycleCount();
    if (!RXval && LastRX && timerCounter != 0) { // Fronte di discesa del RX
        elapsedTime = ESP.getCycleCount() - timerCounter;
        Fi0 = Fi0 + elapsedTime / 240;
        timerCounter = 0; // Reset timer
        if (i > 5000) {
            Fi0 = Fi0 / 5001;
            // LedPWM(); // Gestione LED megnetismo
            if (devMode) Serial.println(Fi0, 1); // Stampo valore di riferimento
            addReferenceValueToCsv(); // Aggiungo il valore di riferimento al CSV
            sendSocketMessage(); // Mando il messaggio via WebSocket
            successBlinkingLed(); // Lampeggio LED verde + delay per visualizzare il valore di riferimento sul front-end
            i = 0; // Reset contatore
            currentScanStatus = SCANNING; // Setto stato SCANNING
            sendSocketMessage(); // Mando il messaggio via WebSocket
            if (devMode) Serial.println("Scansione avviata...");
            stato = 3; // Passo al prossimo stato
        } else {
            i++; // Incremento contatore
        }
    }

    LastTX = TXval; // Aggiorno valore TX
    LastRX = RXval; // Aggiorno valore RX
}

// Controllo se mi sono mosso
void stato3() {
    if (millis() - prevMillis < 200) return; // Se non sono passati 200 ms, non faccio nulla
    prevMillis = millis(); // Aggiorno prevMillis
    LedPWM(); // Gestione LED megnetismo
    mouse.get_data(); // Leggo dati dal mouse
    XVal += mouse.x_movement(); // Prendo movimento x
    YVal += mouse.y_movement(); // Prendo movimento y
    Xcm = XVal * 0.01151; // Converto in cm
    Ycm = YVal * 0.01151; // Converto in cm
    LEDUpDown(Ycm, Yprec); // Gestisco l'LED in base al movimento del mouse
    if (Xprec != int(Xcm) / NCM | Yprec != int(Ycm) / NCM) {
        Xprec = int(Xcm) / NCM;
        Yprec = int(Ycm) / NCM;
        OKXY = true;
    }
    if ((Xcm - int(Xcm / NCM) * NCM > float(NCM) / 3) && (Xcm - int(Xcm / NCM) * NCM < float(NCM) * 2 / 3) && OKXY) {
        OKXY = false;
        stato = 4; // Passo al prossimo stato
    }

    // Controllo pressione del pulsante
    if (!digitalRead(BUTTON))
        navToStato5(); // Passo allo stato 5
}

// Misuro magnetismo e torno a misurare
void stato4() {
    TXval = digitalRead(TXPIN); // Leggo valore TX
    RXval = digitalRead(RXPIN); // Leggo valore RX
    if (!LastTX && TXval) // Fronte di salita del TX
        timerCounter = ESP.getCycleCount();
    if (!RXval && LastRX && timerCounter != 0) { // Fronte di discesa del RX
        elapsedTime = ESP.getCycleCount() - timerCounter;
        delta = delta + elapsedTime / 240;
        timerCounter = 0; // Reset
        if (i > 500) {
            i = 0; // Reset contatore
            delta = delta / 501;
            // currentScanStatus = SCANNING; // Setto stato SCANNING
            writeCsv(int(Xcm / NCM), int(Ycm / NCM), delta); // Aggiungo al CSV
            sendSocketMessage(); // Mando il messaggio via WebSocket
            LedPWM(); // Gestione LED megnetismo
            stato = 3; // Torno allo stato 3
        } else {
            i++; // Incremento contatore
        }
    }

    LastTX = TXval; // Aggiorno valore TX
    LastRX = RXval; // Aggiorno valore RX

    // Controllo pressione del pulsante
    if (!digitalRead(BUTTON))
        navToStato5(); // Passo allo stato 5
}

// Termino scansione e mando dati completi a front-end
void stato5() {
    pollingSocketClient(3000); // Mando heatmap completa ogni 3 secondi

    // Se il pulsante non è premuto esco
    if (digitalRead(BUTTON)) return;
    csvString[0] = '\0'; // Svuoto CSV
    currentScanStatus = READY; // Pronto per nuova scansione
    sendSocketMessage(); // Mando il messaggio via WebSocket
    resetVariabiliLoop(); // Preparo variabili per il prossimo stato
    if (devMode) Serial.println("Pronto per nuova scansione, premi il pulsante per iniziare la calibrazione");
    delay(1000); // Delay per evitare doppia pressione tasti
    stato = 0; // Ricomincio il ciclo
}

// Loop
void loop() {
    switch (stato) {
        case 0: // Stato iniziale
            stato0(); // Gestione stato 0
            break;
        case 1: // Taro la bobina
            stato1(); // Gestione stato 1
            break;
        case 2: // Taro il delay
            stato2(); // Gestione stato 2
            break;
        case 3: // Controllo se mi sono mosso
            stato3(); // Gestione stato 3
            break;
        case 4: // Misuro magnetismo e torno a misurare
            stato4(); // Gestione stato 4
            break;
        case 5: // Termino scansione e mando dati completi a front-end
            stato5(); // Gestione stato 5
            break;
    }
}