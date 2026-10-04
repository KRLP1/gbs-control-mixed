# Portierungs-Protokoll: Leber-Fixes → gbs-control-nx (Kwakx)

Basis: Kwakx/gbs-control-nx @ 94f8979 (v1.4.0)
Quelle: JuergenLeber/gbs-control @ 7bfb9b3
Gemeinsamer Vorfahre (git merge-base): e4e317ab (ramapcsx2/gbs-control, Original-Upstream)

Leber hat seit dem gemeinsamen Vorfahren 44 eigene Commits, Kwakx 28 eigene Commits.
Kwakx hat die komplette Codebasis modularisiert (früher ein ~11.000-Zeilen-.ino,
jetzt sauber in src/gbs, src/video, src/core, src/hardware, src/clock, src/menu,
src/web, src/wifi, src/storage, src/commands, src/utils aufgeteilt) und unterstützt
zusätzlich ESP32. Funktions-/Variablennamen blieben dabei größtenteils erhalten.

⚠️ WICHTIG: In der Sandbox, in der diese Portierung erstellt wurde, war KEINE
Kompilierung möglich (kein Netzwerkzugriff auf die PlatformIO-Toolchain-Server).
Jede Änderung wurde sorgfältig von Hand anhand des Originaldiffs und des exakten
Kwakx-Codes prüft (Register-/Funktions-/Feldnamen verifiziert, Klammerbilanz
geprüft), aber NICHT durch einen echten Build oder Hardware-Test verifiziert.
Vor dem produktiven Einsatz unbedingt selbst kompilieren und auf echter Hardware
testen, insbesondere die Video-Sync-Fixes.

Legende: ✅ portiert | ⏭️ übersprungen (bereits vorhanden/obsolet) | ⚠️ teilweise portiert | ❌ nicht übernommen (Begründung)

## ⏭️ Bereits von Kwakx abgedeckt (übersprungen)
| Leber-Feature | Kwakx-Äquivalent |
|---|---|
| LittleFS-Migration | Kwakx-Commit ae2d2de |
| WiFi-Einstellungen vor Reboot sichern | Kwakx-Commits 1df9c4e, 09c6b60 |
| Firmware-Update im WebUI / OTA | Eigenes, weiterentwickeltes OTA-System inkl. GitHub-Release-Check |
| WebSockets-Bibliothek aktualisieren | Kwakx-Commits 21c6fba, d2caac9, baf3efb |
| **923d265** "websocket log disconnecting permanently on low heap" | Kwakx puffert WebSocket-Log-Ausgaben bereits (SerialMirror.cpp) und trennt bei niedrigem Heap nie die Verbindung — Kwakx-Lösung ist bereits besser |
| Allgemeine Dependency-Modernisierung | eigene, aktuellere PlatformIO-Struktur |

## ✅ Portierte Fixes (15 Commits im Arbeits-Repo)

1. **50f3f43** – Stabilisierung für csync-RGB-Quellen: SP_H_PULSE_IGNOR-Handling, echte
   Sync-Loss-Erkennung über HSACT statt IF-Modusbits, Deinterlacer-Stabilitätsschwelle
   für csync-SD-Quellen erhöht.
2. **87cd8b7** – Erkennung von CRTC-Timing-Änderungen (z. B. Amstrad CPC): hält die
   Recovery-Maschinerie an, bis sich die Zeilenfrequenz wieder normalisiert hat.
3. **bdbfcb7** – Bildschirm wird nach Quellenabschaltung geschwärzt statt ein
   eingefrorenes (oft fehlerhaftes) Bild anzuzeigen.
4. **fb6342c** – Debouncing (3 Messungen) gegen Pixelartefakte/Sprünge bei dunklen
   Szenen, verursacht durch kurzzeitig flackerndes Sync-on-Green-Signal.
5. **f04ff71** – Preset-Speicherung/Slot-Korruption: gemeinsamer Slot-Puffer statt
   Stack-Allokation, Schutz vor kurzen/fehlenden Slot-Dateien, RGB/HV-Upscale-Presets
   werden jetzt unter dem richtigen Dateinamen gespeichert.
6. **9a415d0** (⚠️ Teilportierung) – RGB/HV-Upscale erkennt SD↔EDTV-Formatwechsel jetzt
   korrekt per Band-Vergleich (NTSC/PAL/EDTV) statt paarweiser Schwellwerte. Der riskantere
   Teil (probeDiscreteVSync-basierte Verifikation) wurde NICHT übernommen, siehe unten.
7. **9b662bf + b863fa9** – Neuer Diagnose-Befehl (Clamp/Gain/Sync/SOG-Status) über die
   Weboberfläche abrufbar (Achtung: Befehlsbuchstabe **'G'** statt Leber's 'Q', da 'Q' bei
   Kwakx bereits mit „Pb/U gain++" belegt ist), plus neuer `/status/get`-Endpunkt
   (Heap, Laufzeit, Reset-Grund) mit ESP8266/ESP32-Weiche.
8. **3c3915d** – Slot-Metadata-Puffer (2,3 KB) wird nur noch während der Anfrage
   dynamisch reserviert statt dauerhaft im RAM zu liegen.
9. **7389708** – Schwarzwert driftet nicht mehr zu Grau bei csync-SD-Quellen
   (Clamp-Freigabe auch bei Modus-0-Erkennung mit stabilem Sync).
10. **b17442a** – Sync-Watcher erkennt csync-SD-Quellen jetzt korrekt als „stabil"
    (vorher blieb der Stabilitätszähler ewig bei 0, wodurch Clamp-Nachjustierung,
    Auto-Gain usw. nie liefen).
11. **a68d124** – Externe Taktfrequenz (Clock-Generator) wird nach einem Preset-Wechsel
    zuverlässig wiederhergestellt (Retry alle 5 s statt nur ein einziger Versuch).
12. **92f484d** – Bild ist nach Preset-Laden nicht mehr kurz zu hell: Ausgabe bleibt
    dunkel, bis die Clamp-Position gemessen wurde (bis zu 500 ms Retry-Schleife).
13. **3201025** – Clamp-Positions-Flag wird korrekt zurückgesetzt, wenn eine Quelle
    verschwindet (verhinderte vorher ein sauberes Wiederaufsetzen der Clamp-Fenster
    nach Rückkehr der Quelle).
14. **680f036** – Display kann jetzt in den Standby gehen, wenn die Quelle
    abgeschaltet wird (Sync-Ausgang wird zusätzlich zum Bild gestoppt).

## ❌ Bewusst nicht übernommen

### Atari-ST-Hires-Unterstützung und Folge-Fixes (46cc3db, 09471e9, cb4a0bd, d1517fb, 7bfb9b3)
Diese fünf Commits (~350+ Zeilen) führen eine neue Hilfsfunktion `probeDiscreteVSync()`
und mehrere neue Zustandsvariablen (`fallbackPresetTried`, `noModeWithCsyncCounter`,
`sourceWasClassified`) ein, um Quellen zu erkennen, die der Mode-Detect-Chip nicht
klassifizieren kann (z. B. ein Atari ST im 71-Hz-Monochrom-Hires-Modus). Diese
Infrastruktur **existiert in Kwakx überhaupt nicht** – sie müsste komplett neu entworfen
und an mehreren Stellen der Sync-Watcher-Zustandsmaschine verdrahtet werden. Sogar Leber
selbst brauchte dafür drei Iterationen über mehrere Wochen mit echter Hardware in der
Hand. Ein Fehler hier könnte die Sync-Erkennung für **alle** csync-Quellen beschädigen,
nicht nur für Atari-STs. Ohne Kompilierungsmöglichkeit und ohne Testhardware ist das
Risiko unverhältnismäßig hoch. **Empfehlung:** Falls Atari-ST-Unterstützung gewünscht
ist, sollte das als eigenständige, auf echter Hardware getestete Portierung erfolgen.

### ENABLE_WIFI-Compile-Flag (2fb1fde, fa71bf6)
Ermöglicht bei Leber einen Build ganz ohne WiFi/Web/OTA (u. a. für Japans
Funkzulassung/Giteki). Würde bei Kwakx das WiFi-Bringup in `setup()`/`loop()`, die
Web-Server-Initialisierung, OTA-Init und die Menü-Logik an vielen Stellen mit
`#ifdef` durchziehen müssen – zusätzlich für **zwei** Hardware-Plattformen
(ESP8266 und ESP32, die unterschiedliche WiFi-Stacks haben) und eine neue
PlatformIO-Build-Umgebung. Das Risiko, dabei den normalen (WiFi-aktivierten)
Pfad zu beschädigen, den praktisch alle Nutzer verwenden, überwiegt den Nutzen
eines für diese Community-Fork unüblichen Anwendungsfalls.

### Coast/Clamp-Update direkt nach Presetwechsel (Teil von 50f3f43)
Der ursprüngliche Hunk in `gbs-control.ino::loop()` hat bei Kwakx keine
entsprechende Codestelle mehr – die Coast/Clamp-Zeitsteuerung läuft dort
vollständig über die `continousStableCounter`-Zustandsmaschine in
`runSyncWatcher()`, die bereits über den ebenfalls portierten HSACT-Fix
denselben Sonderfall abdeckt.

## Sonstiges / reine Doku-Commits übersprungen
README-Updates, CI/GitHub-Actions-Workflows, reine "chore"/Rebuild-Commits wurden
nicht portiert, da sie keinen Firmware-Code betreffen bzw. Kwakx eigene
CI-Workflows hat.

## Deutsche Übersetzung (OLED-Display + WebUI)

### OLED-Menü
Alle 43 Textbausteine in `generate_translations.py` um einen `"de-DE"`-Schlüssel
ergänzt und `src/menu/OLEDMenuTranslations.h` mit `DejaVuSans.ttf` neu generiert
(unterstützt deutsche Umlaute/ß nativ, keine Sonderzeichensätze nötig). Das
Menüsystem kompiliert immer nur eine Sprache fest ein (keine Laufzeit-Umschaltung) –
das ist jetzt Deutsch. Um zu einer anderen Sprache zurückzuwechseln oder eine
weitere hinzuzufügen: `python3 generate_translations.py <sprachcode> --fonts <ttf-datei>`
ausführen und die erzeugte `OLEDMenuTranslations.h` einsetzen. Alle Übersetzungen
wurden per generierter Vorschaubilder visuell geprüft; die vier längsten Infotexte
(z. B. die WLAN-Verbindungsanleitung) sind – wie schon im englischen Original –
breiter als die 128 px des Displays und nutzen den vorhandenen automatischen
Scroll-Mechanismus.

### WebUI (Weboberfläche)
`webui/src/index.html.tpl` und `webui/src/index.ts` komplett auf Deutsch
übersetzt (Labels, Tooltips, Hilfetexte, Dialogmeldungen, Platzhalter). Bewusst
unübersetzt gelassen:
- Material-Icon-Ligaturnamen (z. B. `tune`, `wifi`) – das sind keine sichtbaren
  Texte, sondern Icon-Font-Steuercodes
- Entwickler-Konsolenausgaben (`console.log(...)`) – analog zu den ebenfalls
  englischen Seriell-/Debug-Ausgaben der Firmware
- Etablierte Fach-/Markenbegriffe, die deutsche Retro-Gaming-/AV-Nutzer ohnehin
  englisch verwenden: Backup, Bob, Motion Adaptive, FrameTime Lock,
  SyncProcessor/SyncWatcher, Oversampling, sowie Registernamen (HTotal, VTotal,
  SOG Level) und Auflösungs-Suffixe (x720/x960/x1024/x1080)

Die Standard-Slot-Bezeichnung "Empty" wurde konsistent in Firmware
(`src/web/WebServer.cpp`) UND WebUI (`index.ts`) zu "Leer" geändert, da beide
Seiten exakt denselben String vergleichen.

Nach der Übersetzung wurde die WebUI komplett neu gebaut (`tsc` + `build.js` +
`html2h.sh`) und das Ergebnis (`webui/build/webui.html`, `webui_html.h` – das,
was die Firmware tatsächlich ausliefert) ins Repo übernommen und stichprobenartig
auf enthaltene deutsche Texte geprüft. Der TypeScript-Compiler lief dabei ohne
Fehler durch, was die syntaktische Korrektheit der Änderungen bestätigt.

## Nachtrag: Abgleich mit einer früheren Sitzung (16.08.2026)

Der Nutzer hat einen Patch aus einer früheren, unabhängigen Bearbeitung derselben
Aufgabe (gleiche Kwakx-Basis vom 25.03.2026) zur Verfügung gestellt. Der Abgleich
deckte echte Lücken und einen echten Bug in der ursprünglichen Fassung dieses
Dokuments auf. Alle folgenden Punkte sind zusätzlich zu den oben genannten
Fixes eingeflossen:

### Bugfix: Diagnose-Befehl war unerreichbar
Kwakx hat zwei getrennte Einzelzeichen-Befehlskanäle: `serialCommand`
(gespeist von Serial UND `/sc`, Switch in `gbs-control.ino::loop()`) und
`userCommand` (gespeist NUR von `/uc`, Switch in `UserCommandHandler.cpp`).
Der Clamp/Gain-Diagnosebefehl war ursprünglich im zweiten, isolierten Kanal
mit Buchstabe `'G'` verdrahtet, ohne zugehörigen WebUI-Button — praktisch
unerreichbar. Korrigiert: liegt jetzt in `gbs-control.ino` mit Buchstabe
`'Q'` (identisch zu Lebers Original; `'Q'` ist dort frei, `'G'` war
tatsächlich schon belegt).

### ✅ Zusätzlich portiert
- **Hostname-Override** (Leber 270c6f0): Laufzeit-Hostname aus `/hostname.txt`,
  neue Endpunkte `/hostname/get`/`/hostname/set` (inkl. WebUI-Eingabefeld),
  genutzt für mDNS und mit WiFi.hostname()/setHostname().
- **ESP8266-WiFi-Crash-Fix** (Leber 80d0d97): `/wifi/connect` ruft auf ESP8266
  nicht mehr direkt `WiFi.begin()` im AsyncWebServer-Callback auf (Absturzrisiko
  durch Re-Entrancy in lwIP/AsyncTCP), sondern verschiebt Verbindungsaufbau und
  Flash-Persistierung in die Hauptschleife (SDK-Funktionen `wifi_set_opmode`/
  `wifi_station_set_config`). Kwakx hatte das nur für ESP32 sauber gelöst.
- **SerialMirror-Hysterese**: Fester 10000-Heap-Schwellwert ersetzt durch
  Hysterese (aus <8000/ein >12000) gegen Flackern des Debug-Logs.
- **Atari-ST-Hires-Unterstützung / `probeDiscreteVSync()`** (Leber 46cc3db,
  09471e9, cb4a0bd, d1517fb, 7bfb9b3): Ursprünglich wegen Umfang/Risiko
  zurückgestellt, auf ausdrücklichen Wunsch nachträglich integriert. Die
  Implementierung im alten Patch ist deutlich sorgfältiger als Lebers Original
  (Median aus 9 Messungen statt Einzelmessung, ausführliche Kommentare zu
  Fehlermodi, Register werden nach Gebrauch zurückgesetzt) und wurde per
  Drei-Wege-Merge (git merge-file, 11 Konflikte manuell aufgelöst) sauber mit
  den bereits vorhandenen eigenen Sync-Watcher-Fixes zusammengeführt.
  ⚠️ Größte Einzeländerung dieser Portierung (+365/-123 Zeilen in
  SyncWatcher.cpp) — unbedingt auf echter Hardware mit einer csync- oder
  RGB/HV-Quelle testen, bevor produktiv genutzt.

### OLED-Übersetzung: Technik gewechselt
Auf Wunsch von der Bitmap-Pipeline (`generate_translations.py`) auf
Live-Text-Rendering umgestellt (`display->drawString()` mit den bereits in
Kwakx vorhandenen `DejaVu_Sans_Mono_10/12`-Fonts, die Latin-1 inkl. Umlaute
bereits abdecken). Vorteil: keine Python/PIL-Regenerierung mehr nötig, jede
Textänderung ist eine einzeilige Codeänderung.
Dabei selbst entdeckt und behoben: Direkte Vollbild-Dialogtexte (nicht über
`registerItem()`, sondern rohe `drawString()`-Aufrufe) haben KEIN
automatisches Scrollen bei Überlänge — 10 String hätten auf echter Hardware
abgeschnitten ausgesehen (z. B. "Nicht genug Speicherplatz" bei 175 px auf
128 px Display) und wurden gekürzt. Menü-**Listen**-Einträge scrollen
dagegen automatisch (eigener Mechanismus in `OLEDMenuItem::calculate()`).
Bewusst NICHT umgestellt: Statusleisten-Titel/Zurück-Pfeil und der
Bildschirmschoner (dessen Zufallsposition von den Bild-Maßen abhängt) —
beide funktionieren bereits korrekt auf Deutsch über die alte Bitmap-Technik,
ein Umbau hätte nur unnötiges Risiko ohne Nutzen bedeutet.

### Weiterhin nicht übernommen
Der alte Patch enthielt eine eigene `/status/get`-Implementierung, die auf
ESP32 `ESP.getResetReason()` aufruft — eine Methode, die es in der
ESP32-Arduino-Core-API nicht gibt (nur auf ESP8266). Das wäre ein
Kompilierfehler auf ESP32. Die bereits vorhandene, korrekte eigene
`/status/get`-Implementierung (mit sauberer ESP8266/ESP32-Weiche über
`esp_reset_reason()`) wurde beibehalten statt durch die fehlerhafte Version
ersetzt.

## Nachtrag: Kritischer Bug nach echtem Hardware-Test (03.10.2026)

Nach dem ersten erfolgreichen Flash auf echte Hardware meldete der Nutzer zwei
Symptome: (1) im WebUI wurden alle 72 Preset-Slots als befüllt/"Leer" markiert
angezeigt statt nur die tatsächlich leeren schlicht darzustellen, und (2) der
Aufruf des Presets-Menüs im OLED-Display führte zu "Maximum number of sub
items reached: 16" und einem Neustart des Geräts.

**Ursache:** Bei der Übersetzung von "Empty" → "Leer" wurde eine zweite,
separate Definition desselben Sentinel-Werts übersehen: `EMPTY_SLOT_NAME` in
`src/core/slot.h` (verwendet von `OLEDMenuImplementation.cpp`, um per
`strcmp()` zu erkennen, ob ein Slot leer ist) blieb auf dem alten Wert
`"Empty                   "` stehen, während `WebServer.cpp` beim Anlegen
neuer leerer Slots bereits `"Leer                    "` schrieb. Der
Vergleich schlug dadurch für JEDEN Slot fehl — alle 72 galten als "belegt".
Das OLED-Menü versuchte daraufhin, alle 72 als eigene Menüeinträge zu
registrieren, überschritt das Limit von 16 und löste `panicAndDisable()`
aus (Neustart).

**Fix:** `EMPTY_SLOT_NAME` in `slot.h` auf `"Leer                    "`
angeglichen (byte-exakt, 24 Zeichen). Zusätzlich die zwei bisher separat
hartcodierten `"Leer..."`-Literale in `WebServer.cpp` entfernt und durch
Verwendung der gemeinsamen `EMPTY_SLOT_NAME`-Konstante ersetzt, damit beide
Stellen nie wieder auseinanderlaufen können.

Lehre daraus: Beim Übersetzen von Strings, die zugleich als interne
Sentinel-/Vergleichswerte dienen, reicht eine reine Text-Suche nach dem
sichtbaren Literal nicht aus — es kann weitere, separat definierte Kopien
desselben Werts geben, die nur über ihre Verwendung (hier: `strcmp`) als
zusammengehörig erkennbar sind.

## Nachtrag 2: Dritte versteckte "Empty"-Referenz gefunden (03.10.2026)

Nach dem Fix von `EMPTY_SLOT_NAME` zeigte das OLED-Menü korrekt "Keine
Presets", aber die WebUI zeigte weiterhin alle 72 Slots als "Leer"/"EIGEN"
an statt nur den ersten.

**Ursache:** `webui/src/style.css` enthält eine CSS-Geschwister-Selektor-Regel,
die dafür sorgt, dass von mehreren aufeinanderfolgenden leeren Slots nur der
erste sichtbar bleibt, der Rest wird per `display: none` ausgeblendet:
`.gbs-button[gbs-name="Empty"] ~ .gbs-button[gbs-name="Empty"] { display: none; }`
Diese dritte, rein im CSS versteckte Kopie des Sentinel-Werts "Empty" wurde
bei der ursprünglichen Übersetzung übersehen (sie taucht nicht als
JS-String oder C++-String auf, sondern als CSS-Attributselektor).

**Fix:** Selektor auf `gbs-name="Leer"` geändert, WebUI neu gebaut, geprüft
dass kein "Empty" mehr im kompilierten `webui.html`/`webui_html.h` vorkommt.

Damit sind jetzt alle drei Stellen, die den Sentinel-Wert für "leerer Slot"
kennen mussten, konsistent: `src/core/slot.h` (Firmware-Vergleich),
`webui/src/index.ts` (WebUI-Prompt-Vergleich) und `webui/src/style.css`
(WebUI-Anzeigelogik).
