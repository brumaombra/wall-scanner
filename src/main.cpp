#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>

const char *ssid = "ESP32-Access-Point";
WebServer server(80);

void setup()
{
    Serial.begin(115200);

    // Inizializzare SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("Errore nell'inizializzazione di SPIFFS");
        return;
    }

    // Configurare l'ESP32 come Access Point
    WiFi.softAP(ssid);
    Serial.println("Access Point avviato");

    // Route per servire la pagina web
    server.on("/", HTTP_GET, []()
              { server.send(200, "text/html", SPIFFS.open("/index.html", "r").readString()); });

    // Route per file CSS
    server.on("/css/bootstrap.min.css", HTTP_GET, []() {
        server.send(200, "text/css", SPIFFS.open("/css/bootstrap.min.css", "r").readString());
    });

    // Route per file JavaScript
    server.on("/js/bootstrap.bundle.min.js", HTTP_GET, [](){
        server.send(200, "text/javascript", SPIFFS.open("/js/bootstrap.bundle.min.js", "r").readString());
        Serial.println(SPIFFS.open("/js/bootstrap.bundle.min.js", "r").readString());
    });

    /* Route per file JavaScript
    server.on("/js/bootstrap.bundle.min.js", HTTP_GET, []() {
        File file = SPIFFS.open("/js/bootstrap.bundle.min.js", "r");
        if (!file) {
            Serial.println("Errore nell'apertura del file");
            server.send(500, "text/plain", "Errore interno del server");
            return;
        }

        server.setContentLength(file.size());
        server.send(200, "text/javascript", "");
        
        const size_t bufferSize = 1024;
        char buffer[bufferSize];
        while (file.available()) {
            size_t len = file.readBytes(buffer, bufferSize);
            server.sendContent_P(buffer, len);
        }
        file.close();
    });
    */

    // Avviare il server
    server.begin();
}

void loop()
{
    server.handleClient();
}