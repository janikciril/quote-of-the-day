/*
 * ============================================================
 *  Polenta Quotes — ESP32 LED Controller
 *  Polenta7000 Chur
 *
 *  Hardware:
 *    6× P10 RGB Outdoor Panel (32×16 px), 3 breit × 2 hoch
 *    → Gesamtauflösung: 96 × 32 Pixel
 *
 *  Benötigte Libraries (Arduino Library Manager):
 *    1. "ESP32 HUB75 LED MATRIX PANEL DMA Display" von mrfaptastic
 *    2. "ArduinoJson" von Benoit Blanchon
 *
 *  Board: "ESP32 Dev Module" (in Arduino IDE unter
 *         Werkzeuge → Board → esp32 by Espressif)
 * ============================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

// ── !! HIER ANPASSEN !! ───────────────────────────────────
const char* WIFI_SSID     = "DEIN_WLAN_NAME";
const char* WIFI_PASSWORD = "DEIN_WLAN_PASSWORT";
const char* API_URL       = "https://DEINE-DOMAIN.ch/quote-of-the-day/api/api.php?winner=true";
// ─────────────────────────────────────────────────────────

// ── Timing ───────────────────────────────────────────────
#define FETCH_INTERVAL_MS  30000  // Quote alle 30 Sek. neu laden
#define SCROLL_SPEED_MS    22     // ms pro Scroll-Schritt (kleiner = schneller)

// ── Panel-Layout ─────────────────────────────────────────
#define PANEL_W    32   // Pixel pro Panel, Breite
#define PANEL_H    16   // Pixel pro Panel, Höhe
#define NUM_COLS    3   // Panels nebeneinander
#define NUM_ROWS    2   // Panels übereinander

#define TOTAL_W  (PANEL_W * NUM_COLS)   // 96 px
#define TOTAL_H  (PANEL_H * NUM_ROWS)   // 32 px

// ── Helligkeit (0–255) ───────────────────────────────────
#define BRIGHTNESS  180   // Abends reicht 150–200; tagsüber 255

// ── Objekte ──────────────────────────────────────────────
MatrixPanel_I2S_DMA *dma     = nullptr;
VirtualMatrixPanel  *display = nullptr;

// ── Farben (werden in setup() gesetzt) ───────────────────
uint16_t COL_RED, COL_WHITE, COL_YELLOW, COL_DIM;

// ── Zustand ──────────────────────────────────────────────
String        currentQuote = "";
String        scrollText   = "";
int           scrollX      = TOTAL_W;
unsigned long lastFetch    = 0;
unsigned long lastScroll   = 0;
bool          fetchPending = false;

// ============================================================
//  Setup
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Polenta Quotes LED Controller ===");

    initDisplay();

    COL_RED    = display->color565(255,  58,  58);
    COL_WHITE  = display->color565(255, 255, 255);
    COL_YELLOW = display->color565(255, 200,   0);
    COL_DIM    = display->color565( 80,  80,  80);

    showStatus("VERBINDE...", COL_YELLOW);
    connectWiFi();

    // Ersten Quote sofort laden
    fetchQuote();
}

// ============================================================
//  Loop
// ============================================================

void loop() {
    // Quote regelmässig aktualisieren
    if (millis() - lastFetch > FETCH_INTERVAL_MS) {
        fetchQuote();
    }

    // Text scrollen
    if (millis() - lastScroll > SCROLL_SPEED_MS) {
        lastScroll = millis();
        scrollTick();
    }

    // WiFi-Reconnect im Hintergrund
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi verloren, reconnect...");
        connectWiFi();
    }
}

// ============================================================
//  Display initialisieren
// ============================================================

void initDisplay() {
    HUB75_I2S_CFG cfg(PANEL_W, PANEL_H, NUM_COLS * NUM_ROWS);

    // Falls dein Panel einen E-Pin braucht (32-Zeilen-Panels):
    // cfg.gpio.e = 18;

    dma = new MatrixPanel_I2S_DMA(cfg);
    dma->begin();
    dma->setBrightness8(BRIGHTNESS);
    dma->clearScreen();

    // VirtualMatrixPanel: ordnet die 6 verketteten Panels als 3×2-Raster an.
    // "true" = serpentine (Schlangenlinie) — probiere "false" falls Panels
    // in falscher Reihenfolge leuchten.
    display = new VirtualMatrixPanel(*dma, NUM_ROWS, NUM_COLS, PANEL_W, PANEL_H, true);
    display->setTextWrap(false);
    display->setTextSize(2);   // 2 = 16 px hohe Buchstaben → gut lesbar bei 10 m
}

// ============================================================
//  WiFi verbinden
// ============================================================

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("WiFi");

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
        delay(500);
        Serial.print(".");
        tries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" verbunden: " + WiFi.localIP().toString());
    } else {
        Serial.println(" fehlgeschlagen.");
        showStatus("KEIN WIFI", COL_RED);
        delay(5000);
    }
}

// ============================================================
//  Quote von der API laden
// ============================================================

void fetchQuote() {
    lastFetch = millis();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Kein WiFi — Fetch übersprungen.");
        return;
    }

    Serial.println("Lade Quote von API...");

    HTTPClient http;
    http.begin(API_URL);
    http.setInsecure();   // HTTPS ohne Zertifikatsprüfung (für Festival-Einsatz ok)
    http.setTimeout(8000);

    int code = http.GET();
    Serial.println("HTTP Status: " + String(code));

    if (code == 200) {
        String body = http.getString();
        Serial.println("Antwort: " + body);

        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, body);

        if (err) {
            Serial.println("JSON Fehler: " + String(err.c_str()));
        } else {
            String winner = doc["winner"] | "";

            if (winner.length() == 0) {
                // Noch keine Sprüche vorhanden
                setScrollText("NOCH KEINE VOTES — STIMM AB!");
            } else if (winner != currentQuote) {
                currentQuote = winner;
                // Leerzeichen am Ende = Pause bevor Text wieder beginnt
                setScrollText(winner);
                Serial.println("Neuer Quote: " + winner);
            } else {
                Serial.println("Quote unverändert.");
            }
        }
    } else {
        Serial.println("API Fehler, Code: " + String(code));
    }

    http.end();
}

// ============================================================
//  Scroll-Text setzen
// ============================================================

void setScrollText(const String &text) {
    // Führende Leerzeichen: Text startet ausserhalb des Bildschirms rechts
    // Nachfolgende Leerzeichen: Pause am Ende bevor der Loop neu beginnt
    scrollText = "   " + text + "          ";
    scrollX    = TOTAL_W;
}

// ============================================================
//  Scroll-Animation (einmal pro Tick aufgerufen)
// ============================================================

void scrollTick() {
    if (scrollText.length() == 0) return;

    display->clearScreen();

    // Vertikal zentriert: TextSize 2 = 16 px hoch
    int y = (TOTAL_H - 16) / 2;   // = 8 px von oben

    display->setCursor(scrollX, y);
    display->setTextColor(COL_RED);
    display->print(scrollText);

    // Textbreite: 6 px × TextSize × Zeichen
    int textW = (int)scrollText.length() * 6 * 2;
    scrollX--;

    if (scrollX < -textW) {
        scrollX = TOTAL_W;
    }
}

// ============================================================
//  Status-Nachricht (kleine Schrift, zentriert)
// ============================================================

void showStatus(const char *msg, uint16_t color) {
    display->clearScreen();
    display->setTextSize(1);
    int x = max(0, (TOTAL_W - (int)strlen(msg) * 6) / 2);
    int y = (TOTAL_H - 8) / 2;
    display->setTextColor(color);
    display->setCursor(x, y);
    display->print(msg);
    display->setTextSize(2);
}
