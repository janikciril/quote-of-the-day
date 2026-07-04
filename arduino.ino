/*
 * ============================================================
 *  Polenta Quotes — ESP32-S3 LED Controller
 *
 *  Hardware:
 *    2× HUB75 Panel, 64×32 px, übereinander gestapelt (Hochformat)
 *    → Gesamtauflösung: 64×64 Pixel
 *
 *  Libraries (Arduino Library Manager):
 *    - "ESP32 HUB75 LED MATRIX PANEL DMA Display"  von mrfaptastic
 *    - "ArduinoJson"                                von Benoit Blanchon
 *
 *  Board: ESP32S3 Dev Module
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

// ── !! HIER ANPASSEN !! ───────────────────────────────────
const char* API_URL = "https://quoteoftheday.janikhonegger.ch/api/api.php?winner=true";

struct WifiNetwork {
    const char* ssid;
    const char* password;
};

WifiNetwork WIFI_NETWORKS[] = {
    { "Polenta7000",        "7000Amor3!"    },   // Netzwerk 1
    { "tinkergarden",       "strenggeheim"  },   // Netzwerk 2
    { "iPhone von Alessio", "wuesstischgern"},   // Netzwerk 3
};
const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);
// ─────────────────────────────────────────────────────────

// ── Panel-Konfiguration ───────────────────────────────────
#define PANEL_RES_X   64   // Breite eines Panels
#define PANEL_RES_Y   32   // Höhe eines Panels
#define NUM_ROWS       1   // Panels übereinander
#define NUM_COLS       2   // Panels nebeneinander → physisch 128×32

// Nach 90°-Rotation: logisch 32×128
#define TOTAL_WIDTH   32
#define TOTAL_HEIGHT  128

// ── Timing ────────────────────────────────────────────────
#define FETCH_INTERVAL_MS  30000   // Quote alle 30 Sek. neu laden
#define SCROLL_SPEED_MS    40      // ms pro Scroll-Schritt (kleiner = schneller)
#define BRIGHTNESS         180     // 0–255

// ── Schrift ───────────────────────────────────────────────
#define TEXT_SIZE      2           // TextSize 2 = 16px hoch, 12px breit pro Zeichen
#define CHAR_W        12           // 6px × TextSize
#define CHAR_H        16           // 8px × TextSize

// ── Display ───────────────────────────────────────────────
MatrixPanel_I2S_DMA  *dma     = nullptr;
VirtualMatrixPanel   *display = nullptr;

// ── State ─────────────────────────────────────────────────
String        currentQuote = "";
String        scrollText   = "";
int           scrollY      = TOTAL_HEIGHT;
unsigned long lastFetch    = 0;
unsigned long lastScroll   = 0;

// ============================================================
//  Setup
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Polenta Quotes ===");

    initDisplay();
    showStatus("VERBINDE...");

    connectWiFi();
    fetchQuote();
}

// ============================================================
//  Loop
// ============================================================

void loop() {
    if (millis() - lastFetch > FETCH_INTERVAL_MS) {
        fetchQuote();
    }

    if (millis() - lastScroll > SCROLL_SPEED_MS) {
        lastScroll = millis();
        scrollTick();
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi verloren — reconnect...");
        connectWiFi();
    }
}

// ============================================================
//  Display initialisieren
// ============================================================

void initDisplay() {
    HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, NUM_ROWS * NUM_COLS);

    // Pin-Mapping ESP32-S3
    mxconfig.gpio.r1  = 11;
    mxconfig.gpio.g1  = 12;
    mxconfig.gpio.b1  = 13;
    mxconfig.gpio.r2  = 14;
    mxconfig.gpio.g2  = 15;
    mxconfig.gpio.b2  = 16;
    mxconfig.gpio.a   = 5;
    mxconfig.gpio.b   = 6;
    mxconfig.gpio.c   = 7;
    mxconfig.gpio.d   = 8;
    mxconfig.gpio.e   = -1;
    mxconfig.gpio.clk = 3;
    mxconfig.gpio.lat = 4;
    mxconfig.gpio.oe  = 2;

    mxconfig.clkphase       = true;
    mxconfig.latch_blanking = 4;

    dma = new MatrixPanel_I2S_DMA(mxconfig);
    dma->begin();
    dma->setBrightness8(BRIGHTNESS);
    dma->clearScreen();

    // 1 Zeile, 2 Spalten → physisch 128×32; nach Rotation 90° → logisch 32×128
    display = new VirtualMatrixPanel(*dma, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, CHAIN_TOP_LEFT_DOWN);
    display->setRotation(1);   // 90° im Uhrzeigersinn → schmale Seite oben
    display->setTextWrap(false);
    display->setTextSize(TEXT_SIZE);
}

// ============================================================
//  WiFi verbinden
// ============================================================

void connectWiFi() {
    WiFi.mode(WIFI_STA);

    for (int n = 0; n < WIFI_NETWORK_COUNT; n++) {
        Serial.print("WiFi: " + String(WIFI_NETWORKS[n].ssid));
        showStatus(WIFI_NETWORKS[n].ssid);

        WiFi.begin(WIFI_NETWORKS[n].ssid, WIFI_NETWORKS[n].password);

        int tries = 0;
        while (WiFi.status() != WL_CONNECTED && tries < 20) {
            Serial.print(".");
            delay(500);
            tries++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(" verbunden: " + WiFi.localIP().toString());
            return;
        }

        Serial.println(" fehlgeschlagen.");
        WiFi.disconnect();
        delay(500);
    }

    Serial.println("Kein Netzwerk erreichbar.");
    showStatus("KEIN WIFI");
    delay(3000);
}

// ============================================================
//  Quote von der API laden
// ============================================================

void fetchQuote() {
    lastFetch = millis();
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, API_URL);
    http.setTimeout(8000);

    int code = http.GET();
    Serial.println("HTTP " + String(code));

    if (code == 200) {
        String body = http.getString();
        StaticJsonDocument<512> doc;

        if (!deserializeJson(doc, body)) {
            String winner = doc["winner"] | "";

            if (winner.length() == 0) {
                setScrollText("NOCH KEINE VOTES STIMM AB");
            } else if (winner != currentQuote) {
                currentQuote = winner;
                setScrollText(winner);
                Serial.println("Quote: " + winner);
            }
        }
    } else {
        Serial.println("API Fehler: " + String(code));
    }

    http.end();
}

// ============================================================
//  Scroll-Text setzen
// ============================================================

void setScrollText(const String &text) {
    scrollText = text;
    scrollY = TOTAL_HEIGHT;   // startet unterhalb des Displays
}

// ============================================================
//  Scroll-Animation — Buchstaben laufen von oben nach unten
//
//  Jeder Buchstabe wird einzeln gezeichnet, zentriert auf der
//  X-Achse. Der gesamte Text-Strom scrollt von oben nach unten.
// ============================================================

void scrollTick() {
    if (scrollText.length() == 0) return;

    display->clearScreen();

    int x = (TOTAL_WIDTH - CHAR_W) / 2;   // horizontal zentriert
    uint16_t col = display->color565(220, 220, 220);

    for (int i = 0; i < (int)scrollText.length(); i++) {
        int y = scrollY + i * CHAR_H;

        // Nur zeichnen wenn der Buchstabe im sichtbaren Bereich ist
        if (y > -CHAR_H && y < TOTAL_HEIGHT) {
            display->setCursor(x, y);
            display->setTextColor(col);
            display->print(scrollText[i]);
        }
    }

    scrollY--;

    // Wenn der letzte Buchstabe oben raus ist → von vorne starten
    int totalTextH = scrollText.length() * CHAR_H;
    if (scrollY < -totalTextH) {
        scrollY = TOTAL_HEIGHT;
    }
}

// ============================================================
//  Status-Anzeige (zentriert)
// ============================================================

void showStatus(const char* msg) {
    display->clearScreen();
    display->setTextSize(1);
    int x = max(0, (TOTAL_WIDTH - (int)strlen(msg) * 6) / 2);
    int y = (TOTAL_HEIGHT - 8) / 2;
    display->setTextColor(display->color565(255, 200, 0));
    display->setCursor(x, y);
    display->print(msg);
    display->setTextSize(TEXT_SIZE);
}
