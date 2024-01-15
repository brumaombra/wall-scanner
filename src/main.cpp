#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>

const char *ssid = "Wall-Scanner";
WebServer server(80);
int resolution = 1;

// Spezzo il file e mando il contenuto al client
void splitAndSend(const char* path, const char* type) {
    File file = SPIFFS.open(path, "r");
    if (!file) { // Controllo se il file è stato caricato correttamente
        server.send(500, "text/plain", "Errore interno del server");
        return;
    }

    // Spezzo il file e mando
    server.setContentLength(file.size());
    server.send(200, type, "");
    const size_t bufferSize = 1024;
    char buffer[bufferSize];
    while (file.available()) {
        size_t len = file.readBytes(buffer, bufferSize);
        server.sendContent_P(buffer, len);
    }
    file.close(); // Chiudo il file
}

// faccio partire il server web
void setServerWeb() {
    if (!SPIFFS.begin(true)) { // Inizializzo SPIFFS
        Serial.println("Errore nell'inizializzazione di SPIFFS");
        return;
    }

    // Configuro l'ESP come Access Point
    WiFi.softAP(ssid);

    // Routes file
    server.on("/", HTTP_GET, []() {
        splitAndSend("/index.html", "text/html");
    });
    server.on("/css/bootstrap.min.css", HTTP_GET, []() {
        splitAndSend("/css/bootstrap.min.css", "text/css");
    });
    server.on("/css/style.css", HTTP_GET, []() {
        splitAndSend("/css/style.css", "text/css");
    });
    server.on("/js/bootstrap.bundle.min.js", HTTP_GET, []() {
        splitAndSend("/js/bootstrap.bundle.min.js", "text/javascript");
    });
    server.on("/js/jquery-3.7.1.min.js", HTTP_GET, []() {
        splitAndSend("/js/jquery-3.7.1.min.js", "text/javascript");
    });
    server.on("/js/rainbowVis.min.js", HTTP_GET, []() {
        splitAndSend("/js/rainbowVis.min.js", "text/javascript");
    });
    server.on("/js/script.js", HTTP_GET, []() {
        splitAndSend("/js/script.js", "text/javascript");
    });

    // Routes servizi
    server.on("/settings", HTTP_GET, []() {
        String json = "{\"resolution\": " + String(resolution) + "}";
        server.send(200, "application/json", json);
    });
    server.on("/settings", HTTP_POST, []() {
        String body = server.arg("plain");
        server.send(200, "application/json", body);
    });

    // Avviare il server
    server.begin();
}

// Setup
void setup() {
    Serial.begin(115200);
    setServerWeb(); // Faccio partire il server web
}

// Loop
void loop() {
    server.handleClient();
}