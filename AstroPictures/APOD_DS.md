# AstroPictures - Projekt Datasheet

## Übersicht

**Projektname:** AstroPictures
**Zweck:** Digitaler Bilderrahmen der täglich das NASA "Astronomy Picture of the Day" (APOD) anzeigt
**Zielplattform:** ESP32-P4 mit MIPI DSI Display (JC4880P443C_I_W)

---

## Funktionen

### Kernfunktionen

| Funktion | Beschreibung |
|----------|--------------|
| **APOD Download** | Lädt täglich das aktuelle NASA Astronomy Picture of the Day herunter |
| **Bildanzeige** | Zeigt das JPEG-Bild skaliert auf dem Display an |
| **Uhrzeit-Anzeige** | Zeigt aktuelle Uhrzeit (HH:MM) unten links über dem Bild |
| **Datum-Anzeige** | Zeigt aktuelles Datum (YYYY-MM-DD) unten rechts über dem Bild |
| **Auto-Update** | Prüft stündlich ob ein neuer Tag ist und lädt neues Bild |
| **Ladebildschirm** | Zeigt animierten Sternenhimmel während des Ladens |

### Netzwerk-Funktionen

| Funktion | Beschreibung |
|----------|--------------|
| **WiFi** | Verbindet sich automatisch mit konfiguriertem WLAN |
| **HTTPS** | Unterstützt sichere Verbindungen zur NASA API und Bildservern |
| **NTP** | Synchronisiert Uhrzeit via pool.ntp.org (Zeitzone: Europa/GMT+1) |
| **OTA** | Over-the-Air Firmware Updates via ArduinoOTA (Hostname: "AstroPictures") |

### Anzeige-Funktionen

| Funktion | Beschreibung |
|----------|--------------|
| **Auto-Helligkeit** | Passt Display-Helligkeit basierend auf Umgebungslicht an |
| **Backlight PWM** | Stufenlose Helligkeitsregelung via PWM |
| **Kamera-Sensor** | Nutzt OV02C10 Kamera-AEC für Umgebungslicht-Messung |

### Konfiguration
const char* APODApiKey API Key für APOD mit #include Credentials.h einbinden
const char* ssid und const char* password in Credentials.h -> default wifi credentials
beim kompilieren/Upload in NVS übernehmen
Kleine Taste auf Bildschirm um "Customer Wifi access" eizugeben -> Taste -> Tastatur auf Bildschirm -> wenn eingabe bestätigt -> NVS - neu start

---

## Hardware-Anforderungen

### Board

| Parameter | Wert |
|-----------|------|
| **Board** | JC4880P443C_I_W |
| **MCU** | ESP32-P4 |
| **Flash** | 16 MB |
| **PSRAM** | Erforderlich (für Framebuffer und Bildverarbeitung) |

### Display

| Parameter | Wert |
|-----------|------|
| **Typ** | MIPI DSI |
| **Controller** | ST7701 |
| **Auflösung** | 480 x 800 Pixel (Portrait) |
| **Farbtiefe** | RGB565 (16-bit) |

### Touch

| Parameter | Wert |
|-----------|------|
| **Controller** | GT911 |
| **Interface** | I2C |
| **Funktion** | Derzeit nicht aktiv genutzt |

### Kamera (optional)

| Parameter | Wert |
|-----------|------|
| **Sensor** | OV02C10 |
| **Interface** | I2C (geteilt mit Touch) |
| **Funktion** | Umgebungslicht-Messung via AEC-Register |

---

## Pin-Belegung

### Display

| Signal | GPIO |
|--------|------|
| LCD_RST | -1 (nicht verwendet) |
| LCD_LED | -1 (nicht verwendet) |
| LCD_PWM | 24 (Backlight PWM) |

### Touch (I2C)

| Signal | GPIO |
|--------|------|
| TP_I2C_SDA | 7 |
| TP_I2C_SCL | 8 |
| TP_RST | -1 |
| TP_INT | -1 |

### Kamera (I2C) - Not confirmed

| Signal | GPIO |
|--------|------|
| SCCB CAM_I2C_SDA | 7 (geteilt mit Touch) |
| SCCB CAM_I2C_SCL | 8 (geteilt mit Touch) |
| CAM_PWDN | 29 |
| CAM_I2C_ADDR | 0x36 |
  PWDN	GPIO 43 oder GPIO 44 
---

## Software-Abhängigkeiten

### Arduino Board Package

```
esp32:esp32 Version 3.3.5
```

### Arduino IDE / CLI Einstellungen

```
Board: ESP32-P4 Dev Module
Partition Scheme: app3M_fat9M_16MB
Flash Mode: QIO
Flash Size: 16MB
PSRAM: Enabled
```

**FQBN:**
```
esp32:esp32:esp32p4:PartitionScheme=app3M_fat9M_16MB,FlashMode=qio,FlashSize=16M,PSRAM=enabled
```

### Bibliotheken

| Bibliothek | Version | Zweck |
|------------|---------|-------|
| **lvgl** | 8.4.0 | UI Framework / Grafik |
| **WiFi** | (ESP32 built-in) | WLAN-Verbindung |
| **WiFiClientSecure** | (ESP32 built-in) | HTTPS-Verbindungen |
| **HTTPClient** | (ESP32 built-in) | HTTP/HTTPS Requests |
| **ArduinoJson** | 7.x | NASA API JSON Parsing |
| **TJpg_Decoder** | - | JPEG Dekodierung |
| **ArduinoOTA** | (ESP32 built-in) | Over-the-Air Updates |

### LVGL Fonts (in lv_conf.h aktivieren)

```
LV_FONT_MONTSERRAT_14 = 1
LV_FONT_MONTSERRAT_16 = 1
LV_FONT_MONTSERRAT_18 = 1
LV_FONT_MONTSERRAT_20 = 1
LV_FONT_MONTSERRAT_28 = 1
LV_FONT_MONTSERRAT_42 = 1
```

---

## Externe Schnittstellen

### NASA APOD API

| Parameter | Wert |
|-----------|------|
| **Endpoint** | `https://api.nasa.gov/planetary/apod` |
| **Authentifizierung** | API Key (Query Parameter) |
| **Response** | JSON mit Bild-URL, Datum, Titel, Media-Type |

**Beispiel Response:**
```json
{
  "date": "2024-01-15",
  "title": "Beispieltitel",
  "media_type": "image",
  "url": "https://apod.nasa.gov/apod/image/...",
  "hdurl": "https://apod.nasa.gov/apod/image/..."
}
```

**Behandelte Media-Types:**
- `image` → Bild-URL verwenden
- `video` → Thumbnail-URL verwenden (falls vorhanden)

### Credentials

**Datei:** `D:\Credentials.h`

**Erforderliche Variablen:**
```cpp
const char* ssid = "WLAN-Name";
const char* password = "WLAN-Passwort";
const char* APODApiKey = "NASA-API-Key";
```

---

## Bildverarbeitung

### Pipeline

```
NASA API → JPEG URL → Download → Dekodierung → Skalierung → Anzeige
```

### Skalierung

| Schritt | Beschreibung |
|---------|--------------|
| **1. Download** | JPEG in PSRAM laden (max 2MB Buffer) |
| **2. TJpgDec** | Hardware-Skalierung (1/1, 1/2, 1/4, 1/8) |
| **3. Bilinear** | Finale Skalierung auf exakt 480px Breite |
| **4. Display** | RGB565 Framebuffer via LVGL |

### Speichernutzung

| Buffer | Grösse | Speicherort |
|--------|--------|-------------|
| JPEG Download | bis 2 MB | PSRAM |
| Dekodier-Buffer | variabel | PSRAM |
| Final Image | 480 x H x 2 Bytes | PSRAM |
| LVGL Framebuffer | 480 x 800 x 2 Bytes (x2) | PSRAM |

---

## UI Layout

```
+---------------------------+
|                           |
|                           |
|      APOD Bild            |
|      (zentriert)          |
|                           |
|                           |
+---------------------------+
| HH:MM              DATUM  |
|         IP (Klein)        |    
+---------------------------+
```

### Ladebildschirm

- Schwarzer Hintergrund
- 200 zufällige weisse "Sterne"
- Status-Text zentriert (z.B. "Loading APOD...", "Downloading image...")

### Fehleranzeige

Bei Fehlern wird Status-Text angezeigt:
- `WiFi Failed`
- `Error: HTTP xxx`
- `DL Err: corrupt JPEG`
- etc.

---

## Timing / Intervalle

| Aktion | Intervall |
|--------|-----------|
| APOD Update Check | 1 Stunde |
| Uhrzeit-Refresh | 10 Sekunden (prüft Minutenwechsel) |
| Auto-Brightness | 2 Sekunden  |
| LVGL Handler | 5ms (in loop) |

---

## Bekannte Limitierungen

1. **TJpgDec** unterstützt keine Progressive JPEGs
2. **TJpgDec** hat Grössenlimits für sehr grosse Bilder
3. **HTTPS** verwendet `setInsecure()` (keine Zertifikatsprüfung)
4. **Video-APOD** wird nur als Thumbnail angezeigt (falls vorhanden)

---

## Dateistruktur

```
AstroPictures/
├── AstroPictures.ino      # Hauptprogramm
├── pins_config.h          # Pin-Definitionen
├── lv_conf.h              # LVGL Konfiguration
└── src/
    ├── lcd/               # ST7701 Display Treiber
    │   ├── st7701_lcd.cpp
    │   ├── st7701_lcd.h
    │   └── esp_lcd_st7701*.c/h
    └── touch/             # GT911 Touch Treiber
        ├── gt911_touch.cpp
        ├── gt911_touch.h
        └── esp_lcd_touch*.c/h
```

---

## Verbesserungsvorschläge für Neuimplementierung

1. **Alternativer JPEG Decoder** - z.B. ESP32 ROM JPEG oder libjpeg für Progressive JPEG Support
2. **Bildformat-Erkennung** - PNG/GIF Support hinzufügen
3. **Caching** - Letztes Bild im Flash speichern für Offline-Betrieb
