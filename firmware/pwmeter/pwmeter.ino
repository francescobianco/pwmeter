// PWMeter — variante ESP-01
//
// Conta gli impulsi del LED del contatore con un sensore digitale su GPIO2
// (basso durante il lampeggio). Senza rete Wi-Fi salvata, o se non è
// raggiungibile, apre l'access point "PWMeter" con una pagina per sceglierla.
// Da connesso la pagina mostra lo stato e permette di dimenticare la rete;
// se la rete sparisce per più di un minuto si riavvia e torna access point.
//
// Scheda: Generic ESP8266 Module, Flash Size 1MB, Flash Mode DOUT.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

constexpr uint8_t SENSOR_PIN = 2;
constexpr float IMPULSES_PER_KWH = 1000.0;     // verificare sul contatore
constexpr unsigned long MIN_PULSE_MS = 100;    // antirimbalzo
constexpr unsigned long CONNECT_TIMEOUT_MS = 20000;
constexpr unsigned long PORTAL_RETRY_MS = 300000;  // poi riprova la rete salvata
constexpr unsigned long WIFI_LOST_MS = 60000;      // poi riavvia

ESP8266WebServer server(80);
DNSServer dns;
bool portal = false;
unsigned long lastConnected = 0;

volatile uint32_t pulses = 0;
volatile uint32_t lastPulseTime = 0;
volatile uint32_t lastInterval = 0;

void IRAM_ATTR onPulse() {
  uint32_t now = millis();
  if (pulses > 0) {
    if (now - lastPulseTime < MIN_PULSE_MS) return;
    lastInterval = now - lastPulseTime;
  }
  pulses++;
  lastPulseTime = now;
}

void handleStatus() {
  noInterrupts();
  uint32_t p = pulses, last = lastPulseTime, interval = lastInterval;
  interrupts();

  // Se è passato più tempo dell'ultimo intervallo, il consumo è sceso
  uint32_t ms = max(interval, (uint32_t)(millis() - last));
  float power = interval ? 3600000000.0 / (IMPULSES_PER_KWH * ms) : 0;

  server.send(200, "application/json",
              "{\"power_w\":" + String(power, 1) +
              ",\"energy_kwh\":" + String(p / IMPULSES_PER_KWH, 3) +
              ",\"pulses\":" + String(p) + "}");
}

String esc(String s) {
  s.replace("&", "&amp;");
  s.replace("'", "&#39;");
  s.replace("<", "&lt;");
  return s;
}

void handlePortal() {
  String page = F("<!doctype html><meta charset=utf-8>"
                  "<meta name=viewport content='width=device-width'>"
                  "<h2>PWMeter</h2><form method=post action=/save>");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    String ssid = esc(WiFi.SSID(i));
    page += "<p><label><input type=radio name=ssid value='" + ssid + "'> " +
            ssid + " (" + WiFi.RSSI(i) + " dBm)</label>";
  }
  page += F("<p>Password<br><input type=password name=pass>"
            "<p><button>Salva</button></form>");
  server.send(200, "text/html", page);
}

void handleStatusPage() {
  String page = F("<!doctype html><meta charset=utf-8>"
                  "<meta name=viewport content='width=device-width'>"
                  "<h2>PWMeter</h2><p>Rete: <b>");
  page += esc(WiFi.SSID()) + "</b> (" + WiFi.localIP().toString() + ")";
  page += F("<p>Potenza: <b id=p>-</b> W<br>Energia: <b id=e>-</b> kWh"
            "<br>Impulsi: <b id=n>-</b>"
            "<form method=post action=/forget "
            "onsubmit=\"return confirm('Dimenticare la rete e riavviare in modalità AP?')\">"
            "<button>Disconnetti Wi-Fi</button></form><script>"
            "function f(){fetch('/api/status').then(r=>r.json()).then(d=>{"
            "p.textContent=d.power_w;e.textContent=d.energy_kwh;n.textContent=d.pulses})"
            ".catch(()=>{})}f();setInterval(f,2000)</script>");
  server.send(200, "text/html", page);
}

void handleRoot() {
  portal ? handlePortal() : handleStatusPage();
}

void handleSave() {
  server.send(200, "text/html", "Salvato, riavvio...");
  delay(500);
  WiFi.begin(server.arg("ssid").c_str(), server.arg("pass").c_str());  // salva in flash
  ESP.restart();
}

void handleForget() {
  server.send(200, "text/html", "Rete dimenticata, riavvio in modalit&agrave; AP (rete PWMeter)...");
  delay(500);
  WiFi.disconnect();  // cancella le credenziali salvate
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), onPulse, FALLING);

  WiFi.mode(WIFI_STA);
  WiFi.begin();  // rete salvata
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    portal = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("PWMeter");
    dns.start(53, "*", WiFi.softAPIP());
    Serial.println("Portale: rete PWMeter, http://192.168.4.1");
  }

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/forget", HTTP_POST, handleForget);
  server.on("/api/status", handleStatus);
  server.onNotFound([] {
    if (!portal) return server.send(404);
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302);
  });
  server.begin();
}

void loop() {
  if (portal) {
    dns.processNextRequest();
    if (millis() > PORTAL_RETRY_MS && WiFi.softAPgetStationNum() == 0 &&
        WiFi.SSID().length()) {
      ESP.restart();
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    lastConnected = millis();
  } else if (millis() - lastConnected > WIFI_LOST_MS) {
    ESP.restart();  // rete sparita: al riavvio, se non la trova, torna AP
  }
  server.handleClient();
}
