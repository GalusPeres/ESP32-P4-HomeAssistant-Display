# HomeTiles – Übergabe: sporadischer ESP-Hosted-WiFi-Wedge auf ESP32-P4 + ESP32-C6

Stand: 2026-07-31  
Arbeitsverzeichnis: `C:\Users\seb_w\Desktop\Projekte\HomeTiles`  
Aktuelle Testversion laut `version.txt`: `v0.6.3b1`

## Ziel

Den sporadischen Ausfall der P4↔C6-WLAN-Verbindung dauerhaft beheben, ohne MQTT zu pausieren, Speicherreserven zu reduzieren oder gerätespezifische Sonderlösungen einzubauen. Betroffen sind mindestens:

- Waveshare Touch LCD 8
- M5Stack Tab5
- Waveshare 4B

Alle drei Geräte verwenden ESP32-P4 als Host und ESP32-C6 über ESP-Hosted/SDIO für WLAN.

## Typisches Fehlerbild

Nach Minuten oder Stunden funktioniert der WLAN-Datenpfad nicht mehr. MQTT trennt sich zuerst. Danach scheitern neue MQTT-Verbindungen. Ein späterer ESP-Hosted-Control-RPC antwortet nicht mehr:

```text
[MQTT] Verbindung in loop verloren, State=-4
MQTT: Verbindung fehlgeschlagen, State=-2
E (...) rpc_core: Response not received for [0x103](Req_GetWifiMode)
[Network] WLAN-Liveness-Probe blockierte 5001 ms
[Network] WLAN-Treiber reagiert nicht mehr - sicherer Neustart
E (...) rpc_core: Response not received for [0x119](Req_WifiStop)
```

Der vorhandene Watchdog erkennt den Zustand und startet P4 und C6 sicher neu. Das verhindert einen dauerhaften Hänger, beseitigt aber nicht die Ursache.

## Belege aus den Geräten

### Waveshare 8

Aktueller Langzeitlog:

`C:\Users\seb_w\.codex\attachments\8e72c774-52b7-4124-a4e8-b7d817800e3d\pasted-text.txt`

- Firmware: `hometiles-v0.6.3b1-waveshare_touch_lcd_8`
- Letzter Kamerastream lief ungefähr 5 Stunden und 10 Minuten.
- Wedge nach ungefähr 12 Stunden und 55 Minuten Uptime.
- Kurz vor dem Fehler:
  - interner Heap frei: ca. 113 KB
  - größter interner/DMA-Block: ca. 57 KB
  - PSRAM frei: ca. 9.7 MB
- Keine Speicherwarnung, kein Camera-Decoderfehler, kein Panic und kein Watchdog vor dem Wedge.
- Im Log fehlen die sonst bekannten SDIO-Fehler `0x107`, `H_SDIO`, `sdmmc` oder `Unrecoverable host sdio state`.
- Nach dem Neustart scheiterte einmal `Req_GetCoprocessorFwVersion`; danach konnte WLAN wieder normal starten.

Ältere Waveshare-8-Crashlogs zeigen denselben RPC-Wedge bereits mit v0.6.2, teilweise ohne Kamera. Beispiele aus dem bisherigen Chat:

```text
Uptime 651 s  | int=88KB  largest=30KB dma_largest=16KB
Uptime 233 s  | int=98KB  largest=30KB dma_largest=22KB
Uptime 1252 s | int=88KB  largest=30KB dma_largest=13KB
Uptime 2645 s | int=120KB largest=30KB dma_largest=29KB
Uptime 231 s  | int=165KB largest=69KB dma_largest=69KB
Uptime 500 s  | int=119KB largest=69KB dma_largest=69KB
```

### Tab5

Der Fehler trat ebenfalls mehrfach mit v0.6.2 auf, auch unabhängig von der Kamera:

```text
Uptime 59486 s | int=120KB largest=53KB  dma_largest=53KB
Uptime 32462 s | int=146KB largest=95KB  dma_largest=95KB
Uptime 16705 s | int=148KB largest=101KB dma_largest=101KB
```

Brownout-Einträge beim Tab5 sind ein separates bekanntes Hardware-/Versorgungsthema und dürfen nicht mit dem ESP-Hosted-Wedge vermischt werden.

### Waveshare 4B

Der Wedge trat bereits mit alter Firmware und ohne Kameralast auf:

```text
v0.6.2: Uptime 27370 s | int=161KB largest=107KB dma_largest=107KB
```

Mit v0.6.3 gab es weitere Fälle:

```text
Uptime 40456 s | int=181KB largest=135KB dma_largest=135KB
Uptime 10597 s | int=209KB largest=155KB dma_largest=155KB
```

## Sicher ausgeschlossen bzw. nicht gemeinsame Ursache

- **Kamera:** Der Fehler existierte bereits vor dem Kamerafeature und tritt auch ohne offenen Stream auf.
- **v0.6.3 allein:** Der Wedge ist schon in v0.5.6/v0.6.0/v0.6.1/v0.6.2 dokumentiert.
- **Knappes RAM/DMA als gemeinsame Ursache:** Der Fehler trat sowohl bei nur 13–30 KB als auch bei 107–155 KB größtem DMA-Block auf.
- **Bestimmte Displaygröße:** 4B, 8 Zoll und Tab5 zeigen denselben Ausfall.
- **RGB-Framebuffer-Cachewrite:** Die betroffenen P4-Geräte benutzen bereits `esp_cache_msync` beziehungsweise einen Wrapper, der darauf abbildet.
- **MQTT als alleinige Ursache:** MQTT verliert den Datenpfad zuerst; der nachfolgende ESP-Hosted-Control-RPC-Timeout zeigt, dass die Störung darunter liegt.

## Bereits vorhandene ESP-Hosted-Patches

Verzeichnis:

`tools/esp-hosted-3.3.7-rx-fix/`

Vorhandene Anpassungen umfassen unter anderem:

- RX-OOM-Pfade ohne Absturz
- Transportpools bevorzugt im PSRAM
- Realloc-/Speicherfixes
- Workarounds aus Espressif-Issue #167:
  - `TOKEN_RDATA` per vier CMD52-Lesezugriffen statt CMD53-Bytezugriff
  - kein block-only CMD53-Schreibmodus, sondern exakte Länge

Patchdatei:

`tools/esp-hosted-3.3.7-rx-fix/issue-167-sdio-workarounds.patch`

Die v0.6.3b1-Waveshare-8-Bin enthielt beide #167-Workarounds und wedgte trotzdem nach ungefähr 13 Stunden. Diese Workarounds können konkrete SDIO-Transaktionsfehler beheben, reichen aber für den stillen RPC-Wedge nicht aus.

Das lokale Buildskript wendet die Patches auf alle P4-Profile an:

`tools/build-firmware-local.ps1`

Die Objektinjektion erfolgt über:

`tools/apply-esp-hosted-3.3.7-fixes-local.ps1`

## Wichtigste neue technische Erkenntnis

Die in Arduino-ESP32 3.3.7 enthaltene ESP-Hosted-Version 2.11.6 besitzt eine Race-Bedingung für parallele synchrone RPCs.

Untersuchte Quelle:

`C:\Users\seb_w\AppData\Local\Temp\hometiles-esp-hosted-2.11.6\esp_hosted\host\drivers\rpc\core\rpc_core.c`

Eine weitere Kopie liegt unter:

`build\crash-analysis\esp-hosted-2.11.6-source\host\drivers\rpc\core\rpc_core.c`

Problem:

1. Bis zu fünf synchrone RPCs sind laut Konfiguration gleichzeitig erlaubt.
2. Jede Anfrage besitzt zwar ein eigenes Semaphore in `sync_rsp_table`.
3. Alle synchronen Antworten werden jedoch in **dieselbe globale FIFO** `rpc_rx_q` gelegt.
4. Der passende UID-Eintrag weckt das richtige Semaphore.
5. Der geweckte Aufrufer nimmt anschließend einfach das erste Element der globalen FIFO – ohne zu prüfen, ob dessen UID zu seiner Anfrage gehört.
6. `uid++` sowie Tabellenzugriffe sind ebenfalls nicht durch einen Mutex geschützt.

Relevante Funktionen:

- `get_response()` – wartet auf das eigene Semaphore, dequeued danach aber nur den globalen FIFO-Kopf.
- `post_sync_resp_sem()` – postet anhand der UID das passende Semaphore.
- `set_sync_resp_sem()` – registriert UID und Semaphore ohne Lock.
- `rpc_send_req()` – inkrementiert die globale UID und registriert/queued ohne Lock.
- `rpc_wait_and_parse_sync_resp()` – verbraucht und gibt Anfrage/Antwort frei.

Beispiel der Race-Bedingung:

- Anfrage A und B laufen parallel.
- Antwort B kommt vor Antwort A und wird zuerst in die globale Queue gelegt.
- Beide Semaphores werden passend gepostet.
- Thread A läuft zuerst weiter und nimmt Antwort B aus der FIFO.
- Die Control-Zustände können dadurch vertauscht oder ein späterer Aufrufer kann ohne passende Antwort warten.

Dieser Aufbau ist auch in ESP-Hosted 2.12.11 noch weitgehend vorhanden. Das ist daher kein bereits dokumentierter vollständiger Upstream-Fix.

## Woher die parallelen RPCs in HomeTiles kommen können

Arduino-ESP32 3.3.7:

`C:\Users\seb_w\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.7\libraries\WiFi\src\STA.cpp`

- Arduino-WiFi-AutoReconnect läuft aus dem WLAN-Event-Task.
- Beim Disconnect führt es intern erneut `disconnect()`/`connect()` und mehrere ESP-Hosted-RPCs aus.
- HomeTiles hat gleichzeitig eine eigene Reconnect-Schleife in `src/network/network_manager.cpp`.
- HomeTiles aktiviert aktuell `WiFi.setAutoReconnect(true)`.
- Bei einem kurzen AP-/Transportproblem können deshalb Arduino-Event-Task und HomeTiles-Loop gleichzeitig synchrone ESP-Hosted-RPC-Sequenzen starten.
- Der erste Arduino-Reconnectversuch findet in `STA.cpp` sogar unabhängig von `_autoReconnect` statt; nur `setAutoReconnect(false)` allein beseitigt die Upstream-Race daher nicht vollständig.

Das erklärt plausibel, warum zuerst MQTT/Data ausfallen und der erste sichtbare RPC-Timeout oft erst vom späteren HomeTiles-Liveness-Probe (`WiFi.getMode()`) stammt. Es ist eine fundierte Arbeitshypothese, aber noch nicht durch einen Langzeittest des Patches bewiesen.

## Geplanter gemeinsamer Fix

Kein Geräte-Sonderpfad. Der Fix gehört in die gemeinsame ESP-Hosted-RPC-Schicht für alle P4+C6-Geräte.

Minimaler risikoarmer Ansatz:

1. Einen Mutex für synchrone RPC-Transaktionen in `rpc_core.c` ergänzen.
2. Für eine synchrone Anfrage den Mutex vor UID-/Semaphore-Registrierung übernehmen.
3. Den Mutex halten, bis `rpc_wait_and_parse_sync_resp()` die Antwort verbraucht oder der Timeout beendet wurde.
4. Auf sämtlichen Fehlerpfaden zuverlässig entsperren.
5. UID-Vergabe separat schützen oder unter demselben kurzen Lock durchführen, damit auch asynchrone Anfragen keine UID-Race erzeugen.
6. Zusätzlich beim Dequeue UID und erwartete Response-ID prüfen und bei Abweichung deutlich loggen.
7. In HomeTiles auf P4 `WiFi.setAutoReconnect(false)` setzen und HomeTiles als einzigen normalen Reconnect-Owner verwenden. Der RPC-Mutex bleibt trotzdem nötig, weil Arduinos erster Retry weiterhin automatisch erfolgen kann.

Warum das MQTT nicht pausiert:

- Der Mutex serialisiert nur seltene ESP-Hosted-**Control-RPCs** wie Mode/Connect/Config.
- WLAN-Datenpakete, MQTT-Sockets und Kameradaten laufen nicht durch diese RPC-FIFO und werden nicht absichtlich angehalten.
- Die vorhandenen RAM-/DMA-Reserven bleiben unverändert.

Vor der Implementierung müssen insbesondere Deinit-/Timeout-Pfade geprüft werden, damit kein neuer Deadlock entsteht.

## Nächste konkrete Arbeitsschritte

1. `rpc_core.c`-Mutexpatch sauber implementieren und alle Exit-Pfade prüfen.
2. Patch im Repository ablegen, zum Beispiel als:
   - `tools/esp-hosted-3.3.7-rx-fix/rpc-sync-serialization.patch`
3. `rpc_core.c.obj` für beide P4-Varianten neu kompilieren:
   - `esp32p4-libs`
   - `esp32p4_es-libs`
4. Objektdateien über das vorhandene Injektionsskript in `libespressif__esp_hosted.a` einbauen.
5. README im Patchverzeichnis um Ursache, Patch und SHA256 ergänzen.
6. P4-Reconnect-Policy in HomeTiles vereinheitlichen; S3-Verhalten nicht verändern.
7. Nur Waveshare-8-Test-Bin `v0.6.3b1` bauen, zunächst nicht veröffentlichen und nicht pushen.
8. Bin und ELF/Map behalten; im Startlog eindeutig anzeigen, dass RPC-Serialisierung aktiv ist.
9. Langzeittest mit normalem MQTT-Verkehr und zeitweise offener Kamera.
10. Danach denselben gemeinsamen Code auf Tab5 und B4 testen – keine drei unterschiedlichen Implementierungen.

## Erfolgskriterien des Tests

- MQTT läuft immer weiter; keine absichtliche Pause während der Kamera.
- Keine Verringerung der vorhandenen DMA-/Heap-Sicherheitsreserve.
- WLAN-Reconnect funktioniert nach AP-Neustart oder kurzer Unterbrechung.
- Kein `Response not received for Req_GetWifiMode/WifiStop` nach langen Laufzeiten.
- Keine neue Blockade beim OTA, Webportal oder Wechsel zwischen WLAN/Ethernet.
- Falls der Wedge erneut auftritt, müssen neue Logs zeigen:
  - ob ein RPC auf den Serialisierungs-Mutex wartete,
  - welche UID und Request-ID aktiv waren,
  - ob eine falsche UID/Response-ID in der Queue lag,
  - ob der SDIO-Datenpfad oder nur der RPC-Control-Pfad stehen blieb.

## Relevante Internetquellen

- Espressif ESP-Hosted-MCU Issue #167 – P4+C6 SDIO-Ausfälle nach Minuten/Stunden:  
  https://github.com/espressif/esp-hosted-mcu/issues/167
- Espressif ESP-Hosted-MCU Issue #184 – verwandter eingehender Datenpfad-Stall:  
  https://github.com/espressif/esp-hosted-mcu/issues/184
- ESP-Hosted Issue #742 – andere Versions-/Alignment-Probleme, nicht exakt derselbe Fehler:  
  https://github.com/espressif/esp-hosted/issues/742
- ESP-Hosted 2.12.11 Changelog:  
  https://components.espressif.com/components/espressif/esp_hosted/versions/2.12.11/changelog?language=en

## Arbeitsbaum unbedingt erhalten

Der Arbeitsbaum ist absichtlich bereits stark verändert. Keine fremden Änderungen zurücksetzen, kein `git reset --hard` und kein pauschales Checkout.

Zum Zeitpunkt der Übergabe waren unter anderem folgende Dateien verändert oder neu:

```text
M HomeTiles.ino
M lv_conf.h
M src/core/display_manager.cpp
M src/core/github_update.cpp
M src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.cpp
M src/fonts/mdi_icons_32.c
M src/fonts/mdi_icons_40.c
M src/fonts/mdi_icons_48.c
M src/fonts/ui_symbols_20.c
M src/fonts/ui_symbols_24.c
M src/types/pixelanim/renderer.cpp
M src/ui/image_screensaver.cpp
M tools/build-firmware-local.ps1
M tools/esp-hosted-3.3.7-rx-fix/README.md
M tools/esp-hosted-3.3.7-rx-fix/esp32p4-libs/sdio_drv.c.obj
M tools/esp-hosted-3.3.7-rx-fix/esp32p4_es-libs/sdio_drv.c.obj
M tools/generate-mdi-fonts.ps1
M version.txt
?? DEVICE_SUPPORT_PLAN.md
?? docs/images/...
?? forum-assets/
?? src/core/psram_budget.h
?? tools/esp-hosted-3.3.7-rx-fix/issue-167-sdio-workarounds.patch
```

Vor der nächsten Änderung erneut `git status --short` prüfen.

## Wichtige Vorgaben des Nutzers

- Erst sorgfältig analysieren, dann ändern.
- Keine Behauptung, der Fehler sei sicher behoben, bevor der Langzeittest dies zeigt.
- MQTT niemals absichtlich pausieren.
- Speicherreserve nicht reduzieren.
- Keine gerätespezifische Extrawurst; gemeinsame P4+C6-Lösung bauen.
- Zunächst nur eine Waveshare-8-Test-Bin erstellen.
- Noch nicht nach `master` mergen und nichts ungefragt veröffentlichen/pushen.
- Kamera ist experimentell, darf aber nicht als Ausrede für den schon älteren Netzwerkfehler dienen.

## Update 2026-08-01: offizieller SDIO-RX-Fix und Diagnose-Builds

Die drei neuen Langzeitlogs enthalten keinen Ausfall selbst. Tab5 startet im Log bereits frisch neu und läuft danach etwa 5:47 Stunden gesund; Waveshare 8 und B4 laufen mit geöffneter Kamera ebenfalls stundenlang ohne Buffer-Drops, Speicherleck, RPC-Timeout oder MQTT-Abbruch. Die Kamera war auf Tab5 nach dem Neustart geschlossen, weil der Popup-Zustand nur im RAM lebt und bei jedem Boot neu initialisiert wird. Zur eindeutigen Einordnung dieses konkreten Tab5-Neustarts fehlt weiterhin der letzte Block aus `/crashlog.txt`.

Bei der Treiberanalyse wurde ein passender offizieller Espressif-Fix gefunden:

- Upstream-Commit `a8204f950927c17ecf6d737927cf34b4200ffbd3`
- Titel: `fix(sdio): recover a dropped RX read instead of deadlocking`
- Bestandteil von ESP-Hosted 2.12.12

Die bisherige HomeTiles-Rückportierung behandelte den maskierten 20-Bit-Wert `0xFFFFF` fälschlich als Busfehler. Dieser Wert ist jedoch ein legaler Stand des modulo-2^20 laufenden kumulativen Paketlängenzählers. Nach dem Verwerfen konnte das gelöschte NEW_PACKET-Ereignis fehlen und der RX-Pfad dauerhaft im blockierenden Warten stehen. Der offizielle Fix prüft stattdessen das vollständige rohe 32-Bit-Register vor der Maske, leert bereits anstehende Daten und wiederholt eine vorübergehend fehlgeschlagene Pufferallokation.

Umgesetzt wurde die relevante, auf ESP-Hosted 2.11.6 zurückportierte Teilmenge zusammen mit den bestehenden Issue-#167-Workarounds und der RPC-UID-Serialisierung. Die Quellpatch-Datei ist:

`tools/esp-hosted-3.3.7-rx-fix/pkt-len-pending-rx-recovery.patch`

Zusätzliche sparsame Zähler protokollieren:

- rohe `0xFFFFFFFF`-Buslesefehler,
- legale `0xFFFFF`-Zählerstände,
- Pending-Drain-Durchläufe,
- Allokations-Retries,
- Interrupts und konsumierte RX-Bytes.

Beim nächsten Wedge werden diese Werte zusammen mit `camera_stream=active/inactive` dauerhaft nach `/crashlog.txt` geschrieben. Die Startzeile `HomeTiles SDIO RX recovery active ...` wird absichtlich auf Error-Level ausgegeben, damit sie mit dem aktuell reduzierten ESP-Loglevel sichtbar bleibt; sie ist selbst keine Fehlermeldung.

Erzeugte OTA-Binärdateien:

```text
Tab5:
build/v0.6.3b1-sdio-a8204-diag/tab5/HomeTiles.ino.bin
SHA256 38B3E4A8CA107DCA9AE45059A76F6E753EACD1CA64D8DF7162E9A0FF308F66D3

Waveshare 8 Zoll:
build/v0.6.3b1-sdio-a8204-diag/waveshare_8/HomeTiles.ino.bin
SHA256 764009370F5AA082067605CD85B1CFFCEE9C0E35AB5D4A9FB6080DA2FDE48ECA

Waveshare B4:
build/v0.6.3b1-sdio-a8204-diag/waveshare_b4/HomeTiles.ino.bin
SHA256 8DEB7780FBE192BA59ECDF4D4018FD4DF9716EE7627D54C5665933FC30882A6E
```

Alle drei Builds enthalten die erwarteten Geräteprofile, den RPC-Marker, den neuen SDIO-Recovery-Marker und die Crashlog-Diagnose. Der alte fehlerhafte Drop-Text und die bekannten fatalen Allokations-Asserts sind nicht enthalten. Alle drei Binärdateien passen in den OTA-Slot. Ein echter Langzeittest muss noch zeigen, ob der seltene Wedge damit in der Praxis beseitigt ist; die Diagnose trennt beim nächsten Auftreten den SDIO-Fall deutlich besser von Brownout, Watchdog oder Panic.
