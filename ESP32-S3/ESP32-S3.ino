/*
 * ============================================================
 *  Polenta Quotes — ESP32-S3 LED Controller
 *
 *  Hardware:
 *    2× HUB75 Panel, 64×32 px, nebeneinander
 *    → Gesamtauflösung: 128×32 Pixel
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

// ── !! HIER ANPASSEN !! ───────────────────────────────────
const char* WIFI_SSID     = "homebase-2.4";
const char* WIFI_PASSWORD = "nedapuj5Rujznjjv";
const char* API_URL       = "https://quoteoftheday.janikhonegger.ch/api/api.php?winner=true";
// ─────────────────────────────────────────────────────────

// ── Panel-Konfiguration ───────────────────────────────────
#define PANEL_RES_X   64   // Breite eines Panels
#define PANEL_RES_Y   32   // Höhe eines Panels
#define PANEL_CHAIN    2   // Anzahl verketteter Panels

#define TOTAL_WIDTH   (PANEL_RES_X * PANEL_CHAIN)   // 128 px

// ── Timing ────────────────────────────────────────────────
#define FETCH_INTERVAL_MS  30000   // Quote alle 30 Sek. neu laden
#define SCROLL_SPEED_MS    25      // ms pro Scroll-Schritt (kleiner = schneller)
#define BRIGHTNESS         180     // 0–255 (abends reicht 150–200)

// ── Display ───────────────────────────────────────────────
MatrixPanel_I2S_DMA *display = nullptr;

// ── State ─────────────────────────────────────────────────
String        currentQuote = "";
String        scrollText   = "";
int           scrollX      = TOTAL_WIDTH;
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
    HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);

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
    mxconfig.gpio.e   = -1;   // kein E-Pin bei 32-Zeilen-Panels
    mxconfig.gpio.clk = 3;
    mxconfig.gpio.lat = 4;
    mxconfig.gpio.oe  = 2;

    mxconfig.clkphase       = true;
    mxconfig.latch_blanking = 4;

    display = new MatrixPanel_I2S_DMA(mxconfig);
    display->begin();
    display->setBrightness8(BRIGHTNESS);
    display->clearScreen();
    display->setTextWrap(false);
    display->setTextSize(2);   // 16px hohe Buchstaben — gut lesbar bei 10 m
}

// ============================================================
//  WiFi verbinden
// ============================================================

void connectWiFi() {
    Serial.print("WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
        Serial.print(".");
        delay(500);
        tries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" verbunden: " + WiFi.localIP().toString());
    } else {
        Serial.println(" fehlgeschlagen.");
        showStatus("KEIN WIFI");
        delay(3000);
    }
}

// ============================================================
//  Quote von der API laden
// ============================================================

void fetchQuote() {
    lastFetch = millis();
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure client;
    client.setInsecure();   // HTTPS ohne Zertifikatsprüfung (für Festival-Einsatz ok)

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
                setScrollText("NOCH KEINE VOTES — STIMM AB!");
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
    scrollText = "   " + text + "          ";
    scrollX    = TOTAL_WIDTH;
}

// ============================================================
//  Scroll-Animation
// ============================================================

void scrollTick() {
    if (scrollText.length() == 0) return;

    display->clearScreen();

    // TextSize 2 = 16px hoch → vertikal zentriert in 32px
    int y = (PANEL_RES_Y - 16) / 2;
    display->setCursor(scrollX, y);
    display->setTextColor(display->color565(255, 58, 58));   // Rot
    display->print(scrollText);

    // 6px Basisbreite × TextSize 2 = 12px pro Zeichen
    int textW = scrollText.length() * 12;
    scrollX--;
    if (scrollX < -textW) {
        scrollX = TOTAL_WIDTH;
    }
}

// ============================================================
//  Status-Anzeige (kleine Schrift, zentriert)
// ============================================================

void showStatus(const char* msg) {
    display->clearScreen();
    display->setTextSize(1);
    int x = max(0, (TOTAL_WIDTH - (int)strlen(msg) * 6) / 2);
    int y = (PANEL_RES_Y - 8) / 2;
    display->setTextColor(display->color565(255, 200, 0));   // Gelb
    display->setCursor(x, y);
    display->print(msg);
    display->setTextSize(2);
}
