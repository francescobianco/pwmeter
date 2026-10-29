# PWMeter — Misuratore Wi-Fi dei consumi elettrici con ESP8266 e LDR

## 1. Obiettivo del progetto

PWMeter è un piccolo dispositivo open source, economico e autocostruibile, che permette di leggere i consumi elettrici di un ufficio o di un'abitazione sfruttando il LED metrologico del contatore elettronico.

Il progetto nasce con questi requisiti:

- Costo prossimo a zero, utilizzando componenti elettronici di recupero.
- Nessun intervento sull'impianto elettrico o sui sigilli del contatore.
- Misurazione dei consumi in watt e kWh.
- Comunicazione Wi-Fi tramite ESP8266.
- API REST HTTP per interrogare il dispositivo.
- Funzionamento locale, senza cloud, servizi esterni o abbonamenti.
- Possibilità futura di integrare Home Assistant, MQTT e automazioni per lo spegnimento remoto delle macchine.

Il dispositivo non sostituisce il contatore ufficiale e non costituisce una misura elettrica indipendente da esso: utilizza i suoi impulsi ottici come fonte di dati.

## 2. Principio di funzionamento

[Smart Meter: Autolettura e Funzionamento del Contatore 2G](https://images.openai.com/static-rsc-4/6vfjfDuA6kgqOqr5Mt8kOXNr5XHVnMN0RrnxDUx-ZcLDETJQwSMmwHibWqlIHrWkNo8vR6SAk4ZtCrh3ozGLnFgPGVtaOBBbS-3xW6A0UR0YYtXJ4h8Orsqa37t1Lmh6oVReGumTiNKFQu5-XJbH_lfUiM4lSCvpvC0CdBjS3gw?purpose=inline)

[Moduł WiFi ESP8266 + NodeMCU v3 Sklep Botland](https://images.openai.com/static-rsc-4/vG-6QSXb0mIfxNKdb3eu2cvwljy7slABmWBtKZH_h4CejnXt5glMZcs49i1zzQop1tjnCjbDCnmmjgqyW4E7dD94Ec1pN35Pw1-a0R6qGCoV0tsjudmqSw8hyLhFS3xQDBbLv4kg2J8PVcuTw4L9KTrnIvR0emcpIqsdFuEnyJM?purpose=inline)

[5528 LDR Mini Photoresistor -  150V DC max voltage .](https://images.openai.com/static-rsc-4/Vq38-eUsRcHEyJ-bUY4Ux1b4T4lw5dq8Idci-92j8SAJn0KOTzc47SHihm5Ok5gnsfFnnaITYjEwOIiwktQddrpwM72vvQEFbPflkgL1rHbMkKZ0Z5Zv0DG-GrnqYzVJhbWLN1-hBwyLMrStSRLwyObuLp_-JkrdU2Ld082jPOg?purpose=inline)

Il contatore elettronico emette impulsi luminosi proporzionali all'energia consumata. Per molti contatori Open Meter 2G, il LED dell'energia attiva è configurato a:

1.000 impulsi per kWh (imp/kWh)

Il valore va comunque verificato sulla serigrafia del contatore specifico.

Il nostro LDR, posizionato davanti al LED, rileva i lampeggi. L'ESP8266 conta gli impulsi, misura il tempo tra essi ed elabora i dati.

Ogni impulso equivale a:

\\[ E=\frac{1}{1000}\text{ kWh}=1\text{ Wh} \\]

La potenza media nell'intervallo tra due impulsi si calcola con:

\\[ P(W)=\frac{3\\,600\\,000}{N\cdot\Delta t(s)} \\]

dove \\(N\\) è il numero di impulsi per kWh. Con \\(N=1000\\), diventa:

\\[ \boxed{P(W)=\frac{3600}{\Delta t(s)}} \\]

| Intervallo fra impulsi | Potenza stimata |
| ---------------------- | --------------- |
| 1 secondo              | 3.600 W         |
| 2 secondi              | 1.800 W         |
| 3,6 secondi            | 1.000 W         |
| 6 secondi              | 600 W           |
| 12 secondi             | 300 W           |
| 36 secondi             | 100 W           |

Questa è la potenza media tra due impulsi, non la potenza istantanea esatta.

## 3. Componenti necessari

| Componente                      | Quantità | Costo indicativo |
| ------------------------------- | -------- | ---------------- |
| ESP8266 NodeMCU o Wemos D1 Mini | 1        | 3–5 €            |
| Fotoresistenza LDR              | 1        | 0,20–0,50 €      |
| Resistenza 10 kΩ                | 1        | 0,05 €           |
| Cavetti di collegamento         | 3        | 0,50 €           |
| Alimentatore USB 5 V            | 1        | 2–3 €            |
| Supporto opaco per LDR          | 1        | Recupero         |

Totale indicativo: 6–10 €, oppure praticamente zero con materiali già disponibili.

## 4. Schema elettrico

Il circuito consiste in un partitore resistivo.

Il LED illumina l'LDR, la sua resistenza diminuisce e la tensione sull'ingresso ADC aumenta.

Importante per ESP8266: il chip ESP8266 ha un ADC con fondo scala di circa 1 V. Le schede NodeMCU e alcune Wemos D1 Mini includono un partitore sull'ingresso A0, ma occorre verificare la tensione massima ammessa dalla scheda specifica. Non collegare automaticamente il partitore da 3,3 V ad A0 senza questa verifica. Per un ESP8266 con ADC nudo a 1 V serve dimensionare il circuito affinché non superi tale limite.

Il sensore va fissato esternamente in corrispondenza del LED dell'energia attiva, senza aprire il contatore, danneggiare sigilli o coprire informazioni obbligatorie.

## 5. Architettura software

```
[ Contatore Open Meter ]
          |
       LED rosso
          |
         LDR
          |
     ADC ESP8266
          |
  Rilevatore impulsi
    + isteresi
    + filtraggio
          |
     PWMeter Core
       |      |
       |      +-- Conteggio kWh
       +--------- Calcolo potenza
          |
       Wi-Fi
          |
    HTTP REST API
          |
     Rete locale
          |
  PC / Dashboard / Automazioni
```

L'elaborazione è completamente locale.

L'ESP8266 deve poter funzionare anche quando nessun client HTTP sta interrogando le API.

## 6. Firmware ESP8266

> Il prototipo attuale usa un ESP-01, che non espone l'ingresso A0: il firmware effettivo è in `firmware/pwmeter/` e legge il sensore in digitale su GPIO2. Vedi la sezione 11. Lo sketch qui sotto resta come riferimento per schede con A0 (NodeMCU, Wemos D1 Mini).

Per il primo prototipo utilizziamo Arduino Framework e la libreria `ESP8266WiFi`, senza dipendenze aggiuntive.

Il firmware implementa:

- Campionamento ADC ogni 10 ms.
- Soglia alta e bassa per evitare conteggi multipli.
- Tempo minimo tra impulsi validi.
- Calcolo della potenza in watt.
- Conteggio dell'energia in kWh.
- Endpoint REST `/api/status` e `/api/raw`.
- Azzeramento della potenza stimata se non arrivano nuovi impulsi.

Per semplicità, in questa versione le soglie sono fisse e l'energia accumulata viene azzerata a ogni riavvio. La persistenza e l'autocalibrazione saranno evoluzioni successive.

```

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";

constexpr uint8_t LDR_PIN = A0;

// Verificare l'etichetta del contatore
constexpr float IMPULSES_PER_KWH = 1000.0;

// Da calibrare osservando /api/raw
constexpr int THRESHOLD_HIGH = 650;
constexpr int THRESHOLD_LOW  = 500;

constexpr unsigned long SAMPLE_MS = 10;
constexpr unsigned long MIN_PULSE_MS = 100;
constexpr unsigned long POWER_TIMEOUT_MS = 120000;

ESP8266WebServer server(80);

uint32_t pulses = 0;
uint32_t lastPulseTime = 0;
uint32_t lastSampleTime = 0;

bool sensorHigh = false;
bool havePulse = false;
bool haveInterval = false;

float powerWatts = 0;
int rawADC = 0;

void handleStatus() {
  unsigned long now = millis();

  float effectivePower = powerWatts;

  if (!haveInterval ||
      (now - lastPulseTime > POWER_TIMEOUT_MS)) {
    effectivePower = 0;
  }

  float energyKwh = pulses / IMPULSES_PER_KWH;

  String json = "{";
  json += "\"power_w\":" + String(effectivePower, 2);
  json += ",\"energy_kwh\":" + String(energyKwh, 4);
  json += ",\"pulses\":" + String(pulses);
  json += ",\"uptime_s\":" + String(now / 1000);
  json += "}";

  server.send(200, "application/json", json);
}

void handleRaw() {
  String json = "{";
  json += "\"adc\":" + String(rawADC);
  json += ",\"threshold_high\":" +
          String(THRESHOLD_HIGH);
  json += ",\"threshold_low\":" +
          String(THRESHOLD_LOW);
  json += ",\"sensor_high\":";
  json += sensorHigh ? "true" : "false";
  json += "}";

  server.send(200, "application/json", json);
}

void onPulse(unsigned long now) {
  if (havePulse) {
    unsigned long delta = now - lastPulseTime;

    if (delta < MIN_PULSE_MS) return;

    powerWatts =
        3600000000.0 / (IMPULSES_PER_KWH * delta);

    haveInterval = true;
  }

  pulses++;
  lastPulseTime = now;
  havePulse = true;
}

void sampleSensor() {
  unsigned long now = millis();

  if (now - lastSampleTime < SAMPLE_MS) return;
  lastSampleTime = now;

  rawADC = analogRead(LDR_PIN);

  if (!sensorHigh && rawADC >= THRESHOLD_HIGH) {
    sensorHigh = true;
    onPulse(now);
  }
  else if (sensorHigh && rawADC <= THRESHOLD_LOW) {
    sensorHigh = false;
  }
}

void setup() {
  Serial.begin(115200);

  // Inizializzazione stato ottico
  rawADC = analogRead(LDR_PIN);
  sensorHigh = rawADC >= THRESHOLD_HIGH;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("PWMeter IP: ");
  Serial.println(WiFi.localIP());

  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/raw", HTTP_GET, handleRaw);

  server.begin();
}

void loop() {
  sampleSensor();
  server.handleClient();
  yield();
}

```

Le soglie `650` e `500` sono soltanto valori di partenza: potrebbero non funzionare con il tuo LDR. Prima di affidarti ai risultati bisogna misurare i valori ADC a LED acceso e spento.

Inoltre il timeout di due minuti è una scelta iniziale: sotto circa 30 W potrebbe azzerare la potenza anche mentre è presente un consumo reale. In una versione più precisa calcoleremo una stima basata sul tempo trascorso dall'ultimo impulso.

## 7. API REST

Esempio di interrogazione dalla rete locale:

```
curl http://192.168.1.50/api/status
```

Risposta d'esempio:

```
{
  "power_w": 625.00,
  "energy_kwh": 2.418,
  "pulses": 2418,
  "uptime_s": 32780
}
```

Per calibrare il sensore:

```
curl http://192.168.1.50/api/raw
```

```
{
  "adc": 480,
  "threshold_high": 650,
  "threshold_low": 500,
  "sensor_high": false
}
```

L'indirizzo IP è solo un esempio: quello effettivo viene stampato sulla seriale all'avvio. È consigliabile riservare l'IP sul router.

Il server HTTP del prototipo non ha autenticazione: va utilizzato esclusivamente sulla LAN fidata. Per raggiungerlo da Internet è preferibile una VPN, non il port forwarding diretto.

## 8. Installazione e calibrazione

1. Prepara il circuito e verifica la tensione ammessa dall'ingresso A0 della tua scheda ESP8266.
2. Carica il firmware tramite Arduino IDE o PlatformIO, inserendo SSID e password Wi-Fi.
3. Fissa l'LDR davanti al LED corretto del contatore, schermando la luce ambientale con materiale opaco rimovibile.
4. Interroga `/api/raw` durante un lampeggio e tra due lampeggi, per osservare i valori ADC minimo e massimo.
5. Imposta due soglie comprese tra i valori rilevati, con un margine sufficiente a evitare falsi impulsi, e ricompila.
6. Accendi un carico noto, per esempio una lampada o un riscaldatore da circa 1.000 W, e controlla che la frequenza degli impulsi sia coerente. Non utilizzare carichi non sicuri o improvvisati.
7. Confronta l'energia registrata da PWMeter con la differenza delle letture kWh del contatore su un intervallo sufficientemente lungo.

Un dettaglio importante: il valore `/api/status` non è istantaneo nel senso stretto. Rimane l'ultima potenza media calcolata tra due impulsi, finché non arriva un nuovo impulso o scade il timeout.

## 9. Evoluzioni del progetto

Una volta ottenuto un prototipo funzionante, svilupperei PWMeter in tre versioni.

| Versione    | Funzionalità                                                             |
| ----------- | ------------------------------------------------------------------------ |
| PWMeter 0.1 | Lettura LDR, conteggio impulsi, API HTTP                                 |
| PWMeter 0.2 | Calibrazione automatica, persistenza dei contatori, Wi-Fi riconnettibile |
| PWMeter 1.0 | MQTT, Home Assistant, dashboard, statistiche, OTA e automazioni          |

In particolare, nella versione 0.2 introdurrei il salvataggio periodico del conteggio in memoria flash, evitando di scrivere a ogni impulso per limitare l'usura.

### Integrazione nell'ufficio

L'obiettivo finale è avere un flusso come questo:

```
PWMeter
   |
   | HTTP / MQTT
   v
Server di controllo
   |
   +-- Storico consumi
   |
   +-- Rilevamento consumi anomali
   |
   +-- Spegnimento PC tramite SSH
   |
   +-- Controllo prese intelligenti
   |
   +-- Dashboard energetica
```

Ad esempio, se l'ufficio dovrebbe essere inattivo dalle 22:00 alle 07:00 ma continua ad assorbire 250 W, il server potrebbe segnalare l'anomalia e spegnere le macchine autorizzate, prima tramite arresto software e successivamente disattivando le relative prese.

## 10. Limiti da conoscere

L'LDR è una scelta economica, ma non necessariamente la migliore per rilevare impulsi brevi. Ha una risposta più lenta rispetto a un fototransistor e risente dell'illuminazione esterna. Se i lampeggi non vengono rilevati in modo affidabile, la prima sostituzione consigliata è un fototransistor sensibile alla luce rossa, mantenendo lo stesso ESP8266.

Inoltre:

- I dati derivano dal contatore ufficiale, non da una seconda misura elettrica.
- Il conteggio si azzera a ogni riavvio nella versione iniziale.
- Durante una perdita di alimentazione o un riavvio gli impulsi non vengono acquisiti.
- La velocità degli aggiornamenti dipende dal consumo effettivo.
- Il sistema non deve essere utilizzato per fatturazione o misure legalmente rilevanti.

Risultato atteso: con un ESP8266, una LDR, una resistenza e un alimentatore USB, PWMeter può trasformare il contatore esistente in una sorgente di dati energetici accessibile via Wi-Fi, senza collegamenti alla rete elettrica a 230 V.

È un progetto sufficientemente piccolo da diventare anche una repository open source autonoma, con firmware, schema elettrico, guida di assemblaggio e un'interfaccia HTTP documentata.

## 11. Variante ESP-01 (prototipo attuale)

L'hardware effettivamente disponibile è un **ESP-01** (ESP8266EX, flash 1 MB) montato su un adattatore USB con CH340 e zoccolo 2×4, senza tasti né reset automatico.

### Perché cambia lo schema

L'ESP-01 porta sul connettore solo 8 pin e **non espone l'ingresso analogico** (il pin TOUT del chip non è collegato). La lettura dell'LDR su A0 descritta nella sezione 4 non è quindi possibile: il sensore va letto come ingresso digitale. Per contare gli impulsi basta distinguere "luce" da "buio".

Piedinatura vista dal lato componenti, antenna in alto:

```
  GND   GPIO2  GPIO0  RX     ← fila interna
  TX    CH_PD  RST    VCC    ← fila esterna
```

### Sensore su GPIO2

Opzione consigliata, modulo comparatore LM393 ("sensore di luce" con uscita DO):

```
Modulo LM393          ESP-01
  VCC  ───────────── 3,3 V
  GND  ───────────── GND
  DO   ───────────── GPIO2
```

Opzione a costo zero, LDR + trimmer:

```
3,3 V ── trimmer 100 kΩ ──┬── GPIO2
                          │
                         LDR
                          │
                         GND
```

In entrambi i casi GPIO2 è **alto al buio e basso durante il lampeggio**. Il firmware non attiva il pull-up interno (circa 30–50 kΩ, troppo basso rispetto a una LDR): la resistenza verso 3,3 V è sempre esterna e va scelta in base all'LDR. Al banco, con il pull-up interno attivo, la LDR alla luce ambiente non riusciva a portare il pin basso; la prova in corso usa 680 kΩ. Senza resistenza esterna GPIO2 resta flottante. È la polarità giusta: l'ESP8266 si avvia dalla flash solo se GPIO2 è alto all'accensione. GPIO3 (RX) è sconsigliato perché sull'adattatore è collegato al TX del CH340.

La calibrazione si fa col trimmer: sul modulo LM393 il LED di bordo deve lampeggiare in sincronia con quello del contatore. L'opzione LDR + trimmer non ha isteresi e può generare fronti multipli; il firmware li scarta (contatore `rejected` in `/api/raw`), ma se sono frequenti conviene passare al modulo LM393.

### Programmazione

1. Ponticello tra **GPIO0 e GND** mentre si inserisce l'adattatore nell'USB (bootloader).
2. `./firmware/build.sh flash` compila (Generic ESP8266 Module, 1 MB, flash mode **DOUT**) e carica il firmware.
3. Togliere il ponticello e ricollegare l'adattatore.
4. Al primo avvio non c'è una rete salvata: l'ESP apre l'access point aperto **PWMeter**. Collegandosi, la pagina `http://192.168.4.1` elenca le reti visibili; si sceglie la rete, si inserisce la password e si salva. Le credenziali restano nella flash dell'ESP e il dispositivo si riavvia collegandosi alla rete.
5. Se la rete salvata non risponde entro 20 secondi, il portale si riapre. Dopo 5 minuti senza client collegati l'ESP si riavvia e riprova la rete salvata (utile se il router si riaccende dopo un blackout).
6. Da connesso, `http://<ip>/` mostra lo stato (rete, potenza, energia, impulsi e livello del sensore su GPIO2, aggiornati ogni 0,5 s) e il tasto **Disconnetti Wi-Fi**, che cancella la rete salvata e riavvia in modalità AP.
7. Se la rete sparisce per più di un minuto (router spento o rete dell'ufficio cambiata), l'ESP si riavvia: non trovandola, torna in modalità AP e si può riconfigurare.

La flash dell'ESP-01 in uso (produttore `0xe0`, probabilmente BergMicro) richiede la modalità DOUT: con QIO/DIO il chip va in crash all'avvio (`Fatal exception (0)`).

Dall'Arduino IDE le impostazioni equivalenti sono: scheda *Generic ESP8266 Module*, *Flash Size* 1MB (FS:none), *Flash Mode* DOUT, *Reset Method* "no dtr (aka ck)".

### Differenze del firmware rispetto alla sezione 6

| Sezione 6 (ADC su A0)            | Firmware ESP-01 (GPIO2)                                    |
| -------------------------------- | ---------------------------------------------------------- |
| Campionamento ADC ogni 10 ms     | Interrupt sul fronte di discesa                            |
| Soglie software con isteresi     | Soglia hardware tramite trimmer                            |
| Timeout fisso di 2 minuti        | La potenza cala col tempo trascorso dall'ultimo impulso    |
| SSID e password nel codice       | Portale di configurazione, credenziali salvate in flash    |
| `/api/status` e `/api/raw`       | Solo `/api/status`                                         |

### Alimentazione definitiva

L'adattatore USB stesso può fare da alimentatore: ha un regolatore a 3,3 V e basta collegarlo a un caricatore USB. Il regolatore di questi adattatori è al limite per i picchi di corrente del Wi-Fi (300–400 mA): se si osservano riavvii, aggiungere un condensatore da 100–470 µF tra VCC e GND vicino all'ESP-01.
