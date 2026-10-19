# Drohne – Flugsteuerung STM32F405

Eigene Flugsteuerung für einen Quadrocopter mit STM32F405RGT6 (STM32-HAL).

| Teil | Anschluss |
|---|---|
| Gyro/Accel | MPU6050 an I2C1 (PB6 SCL, PB7 SDA), 400 kHz |
| Motoren | TIM3, Oneshot125: M1 = PB0, M2 = PB1, M3 = PA6, M4 = PA7 |
| Funk | ELRS-Empfänger, CRSF 420000 Baud an USART1 (PA9 TX, PA10 RX) |
| Kanäle | 1 Roll, 2 Pitch, 3 Gas, 4 Yaw, **5 Arm-Schalter (AUX1)**, 7 Modus Angle/Rate |
| Terminal | USART2 (PA2 TX, PA3 RX), 115200 Baud |
| Akkuspannung | PC0 (ADC1 Kanal 10), Teiler 11:1 |
| Strom | PA4 (ADC2 Kanal 4), Stromausgang „C“ des ESC – höchstens 3,3 V! |

Regeltakt 1 kHz über TIM2. Flugmodi Angle (selbstaufrichtend) und Rate (Acro).

> **Umstellung:** Der Arm-Schalter liegt seit Block 5 auf **Kanal 5 (AUX1)**,
> vorher war es Kanal 8. Am Sender im Mischer den Schalter auf Kanal 5 legen
> und vor dem ersten Flug Schritt 20 des Kipptests (Arm-Schalter) prüfen.
> Mit Funk zeigt `rc` die Kanäle; Kanal 5 muss beim Umlegen zwischen etwa
> 1000 und 2000 wechseln.

## Verdrahtung (eigener Aufbau mit MPU6050)

Alle Massen (GND) sind miteinander verbunden: Akku-Minus über den ESC, die
Flugsteuerung, Empfänger, MPU6050 und der USB-Seriell-Adapter. **Beim Löten
und Anstecken immer zuerst GND verbinden**, dann die Signale.

### Übersicht

| Von | An STM32 | Hinweis |
|---|---|---|
| ESC M1 / M2 / M3 / M4 | PB0 / PB1 / PA6 / PA7 | je 10 kΩ nach GND empfohlen (siehe unten) |
| ESC GND | GND | gemeinsame Masse, Pflicht |
| ESC Strom „C“/„CUR“ | PA4 über 4,7 kΩ, 100 nF nach GND | höchstens 3,3 V am Pin |
| Akku+ (z. B. ESC-Pin „V“/„VBAT“) | PC0 über Teiler 10 kΩ / 1 kΩ, 100 nF | Teiler 11:1 |
| ELRS TX | PA10 (USART1 RX) | |
| ELRS RX | PA9 (USART1 TX) | für die Akku-Telemetrie |
| ELRS 5V / GND | 5 V / GND | |
| MPU6050 SCL / SDA | PB6 / PB7 | Breakout mit Pull-ups (GY-521: 4,7 kΩ) |
| MPU6050 VCC / GND | 3,3 V / GND | Bauteilseite oben, X-Pfeil nach vorne |
| USB-Seriell RX / TX | PA2 / PA3 (USART2) | 3,3-V-Pegel, 115200 Baud |

### 1. Akkuspannung an PC0

```
 Akku+ (VBAT, bis 25,2 V)
        │
       ┌┴┐
       │ │ R1  10 kΩ, 1 %, 0,25 W
       └┬┘
        │
        ├──────────┬──────────── PC0  (ADC1, Kanal 10)
        │          │
       ┌┴┐       ──┴──
       │ │ R2    ─────  C1  100 nF, Keramik X7R
       └┬┘ 1 kΩ    │
        │   1 %    │
 GND ───┴──────────┴──────────── GND STM32 (= Akku-Minus über den ESC)
```

- **R1 = 10 kΩ, R2 = 1 kΩ**, Metallschicht, 1 %. Teilerfaktor 11:
  25,2 V (6S voll) → 2,29 V an PC0, 16,8 V (4S) → 1,53 V.
- Leistung in R1 bei 25,2 V: 25,2² / 11 kΩ ≈ 58 mW – 0,25-W-Widerstand reicht.
- **C1 = 100 nF** direkt an PC0 nach GND, glättet Störungen der Motoren.
- R2 und C1 möglichst nah am STM32-Pin löten, die lange Leitung kommt an R1.
- Andere Widerstände gehen auch, dann aber `BAT_R_OBEN` und `BAT_R_UNTEN` in
  `Sensors/battery.h` anpassen. PC0 darf nie mehr als 3,3 V sehen.
- Nach dem ersten Anstecken mit einem Multimeter die Akkuspannung messen
  und `batcal <volt>` eingeben.

### 2. Stromeingang PA4 schützen

```
 ESC "C" / "CUR"
        │
       ┌┴┐
       │ │ R3  4,7 kΩ
       └┬┘
        │
        ├──────────────────────── PA4  (ADC2, Kanal 4)
        │
      ──┴──
      ─────  C2  100 nF, Keramik X7R
        │
 GND ───┴──────────────────────── GND STM32
```

- **R3 = 4,7 kΩ in Reihe, C2 = 100 nF von PA4 nach GND**, beide direkt am Pin.
  Zusammen ein Tiefpass mit etwa 340 Hz: Er glättet die PWM-Welligkeit des
  ESC (24/48 kHz), und R3 begrenzt den Strom, falls der Ausgang doch über
  3,3 V geht.
- PA4 ist **nicht 5-V-fest**. Deshalb vorher messen (siehe Punkt 3). Kann
  der Ausgang über 3,0 V gehen, statt R3 allein einen Teiler einbauen:

```
 ESC "C" ──[ 10 kΩ ]──┬── PA4
                      ├──[ 20 kΩ ]── GND
                      └──|| 100 nF ── GND
```

  Der Teiler macht aus 4,5 V dann 3,0 V. Danach `CURR_MV_PER_A_DEFAULT` in
  `battery.h` mit 2/3 multiplizieren (bzw. einfach mit `curcal`
  kalibrieren).

### 3. Woher das Stromsignal kommt

Der B-Cube-4in1 hat einen Stromsensor (Shunt mit Verstärker) für alle vier
Motoren zusammen. Sein Ausgang liegt am 8-poligen Stecker zur Flugsteuerung,
beschriftet mit **„C“, „CUR“ oder „CURR“**. Die Pin-Reihenfolge ist je nach
Version verschieden. Aus der früheren Platinenplanung war sie
`1 GND · 2 VBAT · 3 M1 · 4 M2 · 5 M3 · 6 M4 · 7 C · 8 frei` – **trotzdem am
Aufdruck neben dem Stecker prüfen**, nicht dem Kabel vertrauen.

Mit dem Multimeter (Akku dran, Propeller ab, Motoren aus):

1. **GND:** Durchgang zum Akku-Minus.
2. **VBAT:** zeigt die Akkuspannung.
3. **C/CUR:** zeigt 0 V bis einige 10 mV. Nach dem Armen (Motoren im
   Leerlauf) steigt der Wert leicht. Bei 12,75 mV/A macht 1 A nur 13 mV
   aus.
4. **Höchstwert:** Die Skalierung ist nicht bekannt (12–40 mV/A üblich). Bei
   40 mV/A wären 3,3 V schon bei 82 A erreicht – das schafft ein 5-Zoll-Quad
   bei Vollgas. Deshalb prüfen, woher der Verstärker seine Versorgung
   bekommt: Ist auf der ESC-Platine nur eine 3,3-V-Schiene in der Nähe,
   kann der Ausgang nicht über 3,3 V gehen. Ist es 5 V, den Teiler aus
   Punkt 2 nehmen.

Der Pin „TLM“ (falls vorhanden) bleibt frei – BLHeli_S sendet keine
Telemetrie.

### 4. Motorsignale

```
 PB0 ──┬──── ESC M1        (ebenso PB1→M2, PA6→M3, PA7→M4)
       │
      ┌┴┐
      │ │ 10 kΩ nach GND (empfohlen)
      └┬┘
 GND ──┴──── ESC GND
```

Nach einem Reset sind die STM32-Pins einige Millisekunden lang offen. Liegt
eine Signalleitung dabei länger als etwa 15 ms auf High, während der ESC
startet, springt BLHeli_S in seinen Bootloader und reagiert nicht mehr, bis
das Signal wieder läuft. Ein 10-kΩ-Widerstand nach GND an jeder Leitung hält
sie in dieser Zeit sicher auf Low.

### Bauteilliste

| Bauteil | Wert | Anzahl | Wofür |
|---|---|---|---|
| Widerstand 1 %, 0,25 W | 10 kΩ | 1 | R1 Akkuteiler |
| Widerstand 1 % | 1 kΩ | 1 | R2 Akkuteiler |
| Widerstand | 4,7 kΩ | 1 | R3 Strom |
| Widerstand | 10 kΩ | 4 | Pull-down Motorsignale |
| Keramikkondensator X7R | 100 nF | 2 | C1 an PC0, C2 an PA4 |
| optional Widerstand | 10 kΩ + 20 kΩ | je 1 | Teiler am Strom, falls > 3,0 V |

## ESC einstellen (BLHeli_S)

**Vor dem ersten Motortest einstellen.** BLHeli_S nimmt ab Werk nur
1148–1832 µs als Gasbereich an, bei Oneshot125 geteilt durch 8, also
143,5–229 µs. Die Firmware gibt aber 125–250 µs aus. Mit den
Werkseinstellungen:

- liegt der Leerlauf (15 % = 143,75 µs) genau an der Anlaufschwelle – ein
  Motor kann im Leerlauf stehen bleiben;
- tut sich über 229 µs (83 % Ausgang) nichts mehr – bei viel Gas fehlt der
  Regelung der Stellweg nach oben, die Drohne lässt sich nicht mehr halten.

Einstellungen (für alle vier ESCs gleich):

| Einstellung | Wert | Warum |
|---|---|---|
| PPM Min Throttle | 1040 µs (= 130 µs bei Oneshot125) | Stopp-Puls 125 µs liegt darunter, Leerlauf 15 % sicher darüber |
| PPM Max Throttle | 2000 µs (= 250 µs) | voller Ausgangsbereich nutzbar |
| Programming by TX | aus | kein versehentlicher Programmiermodus beim Einschalten |
| Motor Direction | je Motor passend | siehe Kipptest Schritt 15 |
| Rest | Werkseinstellung | |

So kommst du an die ESCs: Die eigene Flugsteuerung kann kein
BLHeli-Passthrough. Du brauchst eine 1-Wire-Schnittstelle, die an die
Motorsignal-Pads (M1–M4) und GND des ESC kommt:

- einen fertigen USB-Linker für BLHeli (z. B. „BLHeli USB Linker“), oder
- einen Arduino Nano/Uno mit der 4-Way-Interface-Firmware aus BLHeliSuite, oder
- vorübergehend eine Betaflight-Flugsteuerung, dann über
  [esc-configurator.com](https://esc-configurator.com) (Chrome/Edge).

Programmiert wird mit Akku am ESC, aber ohne Propeller. Danach die
Signalleitungen wieder an PB0/PB1/PA6/PA7.

**Danach den Leerlauf prüfen** (Kipptest Schritt 15): Alle vier Motoren
müssen im Leerlauf sicher und gleichmäßig drehen. Die 15 % in
`MIXER_IDLE_PERCENT` (`Sensors/mixer.h`) wurden mit den Werkseinstellungen
gemessen; mit Min Throttle 1040 drehen sie etwas schneller. Laufen sie
deutlich zu schnell, den Wert in kleinen Schritten senken (nicht unter die
Stelle, an der ein Motor stockt).

## Bauen und Flashen

```sh
git submodule update --init --recursive   # STM32CubeF4 mit HAL und CMSIS
make
make flash                                # OpenOCD-Pfad steht im Makefile
```

## Vorzeichenkonvention

Gilt für die ganze Firmware: Lagewinkel, Gyro-Drehraten, Sollwerte und
Reglerausgaben. Die Beschreibung steht auch oben in `Sensors/mixer.c`.

Körperachsen wie in der Luftfahrt, rechtshändig:

- **X** nach vorne, **Y** nach rechts, **Z** nach unten

Positive Drehrichtungen:

| Achse | positiv heißt | Knüppel |
|---|---|---|
| Roll | rechte Seite geht nach unten | Roll rechts → + |
| Pitch | Nase geht nach oben | Pitch nach vorne → − (Nase runter) |
| Yaw | Nase dreht nach rechts (von oben im Uhrzeigersinn) | Yaw rechts → + |

Ein positiver Reglerausgang erzeugt eine Drehung in positiver Richtung.
Der Mixer setzt das so um:

| Stellgröße | mehr Gas | weniger Gas |
|---|---|---|
| Roll + | M3, M4 (links) | M1, M2 (rechts) |
| Pitch + | M2, M4 (vorne) | M1, M3 (hinten) |
| Yaw + | M2, M3 (CCW) | M1, M4 (CW) |

Motorbelegung, Blick von oben:

```
    M4 (CW)    M2 (CCW)     vorne
          \    /
           \  /
           /  \
          /    \
    M3 (CCW)   M1 (CW)      hinten
```

### Einbaulage des Sensors

Der MPU6050 liegt mit der Bauteilseite nach oben. In `Sensors/mpu6050.h`
wird eingestellt, wohin der X-Pfeil auf dem Sensor-Board zeigt:

```c
#define IMU_ALIGN IMU_X_VORNE   /* oder IMU_X_RECHTS, IMU_X_HINTEN, IMU_X_LINKS */
```

Die Firmware rechnet die Chip-Achsen damit auf die Körperachsen um.

## Kipptest ohne Propeller

Vor jedem Erstflug und nach jeder Änderung an Sensor, Mixer oder Verkabelung.
**Propeller ab.**

### Teil A – Sensor (nur USB, kein Akku)

1. Flugsteuerung flach hinlegen, Nase nach vorne. Terminal an USART2 mit
   115200 Baud. Reset drücken und bis `>> Bereit.` nicht bewegen
   (Gyro-Kalibrierung). Wird sie bewegt, meldet die Firmware „Bewegung
   erkannt“ und misst neu (bis zu 5 Versuche). Klappt keiner, bleibt Arming
   bis zum Neustart gesperrt. Die Lagewinkel starten direkt mit dem Wert aus
   dem Beschleunigungssensor.
2. `stream on` eingeben. Flach liegend: `R` und `P` nahe 0, `Gx`, `Gy`, `Gz`
   nahe 0.
3. Rechte Seite etwa 30° nach unten kippen: `R` wird positiv (ungefähr +30).
4. Nase etwa 30° nach oben kippen: `P` wird positiv.
5. Flach liegend von oben gesehen im Uhrzeigersinn drehen (Nase nach rechts):
   `Gz` ist während der Drehung positiv.
6. Stimmt Schritt 3 oder 4 nicht, ist die Einbaulage falsch eingestellt.
   Beobachtung bei der Einstellung `IMU_X_VORNE`:

   | Rechte Seite runter | Nase hoch | richtige Einstellung |
   |---|---|---|
   | R + | P + | `IMU_X_VORNE` (passt) |
   | R − | P − | `IMU_X_HINTEN` |
   | P − | R + | `IMU_X_RECHTS` |
   | P + | R − | `IMU_X_LINKS` |

   Zeigt `R` flach liegend etwa ±180, liegt der Chip kopfüber. Das wird nicht
   unterstützt, Sensor mit der Bauteilseite nach oben einbauen.
7. `stream off`.

### Teil B – Regler und Mixer (nur USB, disarmed)

8. `angle` eingeben (bei verbundener Funke: Modus-Schalter auf Angle),
   dann `kipp`. Das zeigt fünfmal pro Sekunde, was der Mixer bei 50 % Gas
   ausgeben würde. Die Motoren bleiben aus. Flach liegend stehen alle vier
   Werte etwa gleich (57,5).
9. Nase etwa 20° nach unten drücken: **M2 und M4 (vorne) höher** als M1 und
   M3. Der Regler will die Nase wieder anheben.
10. Rechte Seite etwa 20° nach unten: **M1 und M2 (rechts) höher** als M3 und M4.
11. Flach liegend im Uhrzeigersinn drehen: während der Drehung **M1 und M4
    (CW) höher** als M2 und M3. Der Regler bremst die Drehung.
12. Mit verbundener Funke (Arm-Schalter unten), flach liegend:
    - Roll-Knüppel rechts → **M3 und M4 höher** (linke Seite hebt sich)
    - Pitch-Knüppel nach vorne → **M1 und M3 höher** (Heck hebt sich)
    - Yaw-Knüppel rechts → **M2 und M3 höher**
13. `kipp` beendet die Anzeige.

Ist in Teil B eine Richtung verkehrt, obwohl Teil A stimmt: nicht fliegen.
Die Vorzeichen in `mixer.c` nicht einfach umdrehen – sie folgen der
Konvention oben. Der Fehler liegt dann woanders (Einbaulage, Verkabelung).

### Teil C – Motoren (Akku dran, Propeller ab)

14. Funke an, Arm-Schalter unten, Gas ganz unten. Akku anstecken.
15. Drohne etwa 30° schräg halten und den Arm-Schalter hochnehmen: Die
    Motoren bleiben aus, das Terminal meldet `Arming abgelehnt: zu schief`.
    Arm-Schalter wieder runter, Drohne flach hinstellen.
    Arm-Schalter hoch: Alle vier Motoren laufen im Leerlauf. Drehrichtung
    jedes Motors prüfen (Klebeband-Fähnchen auf der Glocke), von oben gesehen:
    M1 hinten rechts CW, M2 vorne rechts CCW, M3 hinten links CCW,
    M4 vorne links CW. Dreht ein Motor falsch herum: im ESC-Configurator
    („Motor Direction“) umstellen oder zwei der drei Motordrähte tauschen –
    nie die Vorzeichen im Mixer ändern.
16. Modus Angle, Gas auf etwa 30 %, Drohne gut festhalten:
    - Nase nach unten kippen → die vorderen Motoren laufen schneller.
    - Rechte Seite nach unten → die rechten Motoren laufen schneller.
17. Drohne flach halten, Gas ganz zurück: Die Motoren laufen im Leerlauf
    weiter und gehen nicht aus. (Weil das Gas in Schritt 16 schon über 25 %
    war, ist jetzt Airmode aktiv: Schräg gehalten würden die tieferen Motoren
    weiter hochdrehen. Siehe „Leerlauf und Gas“.)
    Terminal-Test: `disarm` eingeben, während der Arm-Schalter noch oben ist.
    Die Motoren gehen aus und bleiben aus, bis der Schalter einmal unten war.
18. Failsafe: Gas auf etwa 30 %, dann die Funke ausschalten. Nach 0,5 s
    nimmt die Firmware das Gas in 1,5 s auf Leerlauf zurück und schaltet
    danach die Motoren ab. Funke wieder an: Die Motoren bleiben aus, bis der
    Arm-Schalter einmal unten war.
19. Arm-Schalter während der Rampe (nur, wenn sich die Verbindung schnell
    unterbrechen und wiederherstellen lässt, z. B. am Sender das interne
    HF-Modul kurz auf AUS und zurück): armen, Gas etwa 30 %, Verbindung
    unterbrechen, nach etwa 1 s wiederherstellen und sofort den Arm-Schalter
    runter: Motoren sofort aus. Vorher zum Armen Gas ganz runter. Ein
    Neustart des Senders dauert zu lang – dann hat die Rampe schon
    abgeschaltet und der Test sagt nichts aus.
20. Gas ganz unten, Arm-Schalter hoch (Motoren im Leerlauf), dann
    Arm-Schalter runter: Motoren sofort aus.

## Leerlauf und Gas

Solange gearmt ist, laufen alle Motoren mindestens mit `MIXER_IDLE_PERCENT`
(15 %, in `Sensors/mixer.h`). Gas 0–100 % wird linear auf Leerlauf bis Vollgas
abgebildet, es gibt keinen Sprung. Disarmed sind alle Motoren aus.

Airmode: Erst wenn das Gas seit dem Armen einmal mindestens
`MIXER_AIRMODE_START_PERCENT` (25 %) erreicht hat, darf der Mixer das
Grundgas anheben, damit die Lageregelung auch bei Gas ganz unten voll wirkt
(wichtig im Flug, z. B. beim Sinken). Vorher – am Boden – bleibt es bei
Gas 0 beim reinen Leerlauf, auch wenn die Drohne schief steht, und die
Korrekturen werden gestaucht. Das Schwebegas sollte über 25 % liegen,
sonst den Wert in `mixer.h` senken.

**Nach dem Landen sofort disarmen.** Airmode bleibt bis zum Disarm aktiv:
Steht die gelandete Drohne schief, drehen die tieferen Motoren hoch.

## Failsafe

Kommt länger als `CRSF_TIMEOUT_MS` (500 ms) kein gültiges Funkpaket:

- disarmed: Arming über die Funke ist gesperrt, bis das Signal zurück ist
  und der Arm-Schalter einmal unten war
- gearmt: Angle-Modus wird erzwungen, Roll/Pitch/Yaw-Sollwert 0 (waagrecht,
  kein Gieren), Gas läuft in `RC_FS_RAMP_S` (1,5 s) linear auf 0, danach
  wird disarmt
- gearmt, aber das Gas war schon mindestens 1 s unter 5 %
  (`RC_FS_LANDED_MS`, `RC_FS_LANDED_THR`): Die Drohne gilt als gelandet und
  wird sofort disarmt, ohne Rampe

Die Rampe läuft bis zum Ende, auch wenn das Signal zwischendurch
zurückkommt – Knüppel und Modus-Schalter wirken bis dahin nicht. Nur der
Arm-Schalter wirkt immer: runter heißt sofort aus. Danach lässt sich über
die Funke erst wieder armen, wenn der Arm-Schalter einmal unten war.

Ohne Funk lässt sich weiter über das Terminal armen (`arm`, dann `ja`,
Prüfstand). Die Motoren gehen dann sofort in den Leerlauf. Mit verbundener
Funke lehnt das Terminal `arm` ab. Kommt die Funke dazu, hat sie Vorrang:
Steht der Arm-Schalter unten oder wurde nicht über die Funke gearmt,
schaltet die Firmware ab.

**Terminal-Arm braucht ein Lebenszeichen.** Solange über das Terminal
gearmt ist, muss mindestens jede Sekunde ein fehlerfreies Zeichen ankommen
(`COMMAND_KEEPALIVE_MS` in `Sensors/command.h`), sonst disarmt die Firmware
sofort. Abgezogenes USB-Kabel, abgestürztes Terminalprogramm oder
eingeschlafener Laptop schalten so die Motoren ab. Als Lebenszeichen reicht
ein Enter (leere Zeile) alle 0,5 s – im Terminalprogramm „zyklisch senden“
einstellen (z. B. HTerm: `\r` mit Wiederholung 500 ms) oder dieses kleine
Python-Skript benutzen (`pip install pyserial`, Port anpassen):

```python
import serial, threading, time
s = serial.Serial("COM5", 115200, timeout=0.1)   # Linux: "/dev/ttyUSB0"
def keepalive():
    while True:
        s.write(b"\r"); time.sleep(0.5)
def lesen():
    while True:
        print(s.read(200).decode(errors="replace"), end="", flush=True)
threading.Thread(target=keepalive, daemon=True).start()
threading.Thread(target=lesen, daemon=True).start()
while True:
    s.write((input() + "\r").encode())
```

Beendest du das Skript, gehen die Motoren nach 1 s aus. Nach dem Disarm
meldet das Terminal `DISARMED - laenger als 1000 ms kein Zeichen vom
Terminal.` Über den Arm-Schalter gearmt gilt das nicht, dort wacht der
Funk-Failsafe. Der Terminal-Arm ist trotzdem nur für den Prüfstand ohne
Propeller gedacht.

## Regelung

- Kaskade: Winkelregler (nur P) gibt die Soll-Drehrate vor, der
  Drehratenregler (PID) die Stellgröße. Im Rate-Modus entfällt der äußere
  Kreis. Startwerte und Grenzen stehen oben in `Sensors/control.c`.
- Die Zykluszeit `dt` wird mit dem Taktzähler des Prozessors gemessen
  (normal 1 ms) und für Filter, Lagewinkel und PID verwendet.
- D-Anteil auf den Messwert, Tiefpass 25 Hz, aus Grenzfrequenz und `dt`
  berechnet. Nach einem Reset (Armen, Gainänderung) gibt es keinen
  D-Ausschlag: Der erste Messwert gilt als Vorwert.
- I-Anteil: begrenzt wird der I-Anteil selbst (Drehrate ±100 von ±400 =
  25 % Stellbereich), unabhängig von Ki. Er wächst nicht weiter, solange der
  Reglerausgang am Limit steht oder der Mixer die Korrekturen stauchen muss
  (Anti-Windup). Integriert wird erst, wenn die Drohne fliegt: Airmode an
  (Gas seit dem Armen einmal über 25 %) und Gas über 10 %. Am Boden lädt
  sich der I-Anteil so nicht gegen eine Schräglage auf.
- Moduswechsel Angle/Rate setzt nur den Winkelregler zurück, der I-Anteil
  des Drehratenreglers bleibt.
- Gyro-Messbereich ±2000 °/s, Accel ±4 g.

**Solange gearmt ist, schweigt das Terminal.** Eine Textzeile kostet bei
115200 Baud mehrere Millisekunden und würde den Regeltakt stören. Gearmt
wirken nur `disarm`/`stop`, `t <gas>` und `curcal <ampere>`, alle anderen Befehle (Anzeigen,
PID-Werte, Filter, `stream`, `kipp`) gehen nur disarmed.

## Sicherheitsabfragen beim Armen

Gilt für den Arm-Schalter und für `arm` im Terminal (`Sensors/arming.c`):

| Bedingung | Grenze |
|---|---|
| Sensor | MPU6050 erkannt, nicht ausgefallen, seit 0,5 s fehlerfrei |
| Neigung | Roll und Pitch jeweils höchstens 25° (`ARM_MAX_TILT_DEG`) |
| Akku | Spannung erkannt und mindestens 3,50 V pro Zelle (`BAT_WARN_V`) |
| Gas | Knüppel unter 5 % (nur Arm-Schalter) |

Scheitert eine Bedingung, bleibt die Drohne aus und das Terminal zeigt den
Grund (`Arming abgelehnt: ...`). Der Arm-Schalter muss dann erst wieder
runter – die Drohne armt nicht von selbst, sobald die Bedingung später passt.
Ohne angeschlossenen Spannungsteiler an PC0 lässt sich normalerweise nicht
armen (ein offener Eingang kann allerdings zufällige Werte liefern). Die
Neigung wird zweimal geprüft: am gefilterten Lagewinkel und direkt am
Beschleunigungssensor. Die Zellenerkennung ist bei teilentladenen 5S/6S-Akkus
nicht eindeutig – dann kann ein zu leerer Akku als „OK“ durchgehen
(siehe „Akku und Strom“).

`arm` im Terminal fragt zusätzlich nach: Erst nach `ja` innerhalb von 5 s
wird gearmt, die Prüfungen laufen dabei noch einmal. `t <gas>` geht nur,
wenn gearmt ist. Gearmt muss mindestens jede Sekunde ein Zeichen kommen
(siehe „Failsafe“).

## Sensorausfall, Watchdog und Fehler

- Der MPU6050 wird jede Millisekunde per Interrupt gelesen, ohne die
  Regelschleife zu blockieren. Einzelne Lesefehler überbrückt die Regelung
  mit den letzten Werten.
- 20 Fehler in Folge (20 ms) oder 100 exakt gleiche Messungen hintereinander
  (eingefrorene Werte, 100 ms): Der Sensor gilt als ausgefallen, die Motoren
  gehen sofort aus. Wieder armen lässt sich erst nach 0,5 s fehlerfreiem Lesen.
- Scheitert das Lesen dreimal in Folge, setzt die Firmware den I2C-Bus und
  die Peripherie neu auf (höchstens alle 20 ms).
- Watchdog (IWDG): Läuft der Regelzyklus etwa 100 ms lang nicht (je nach
  Toleranz des internen Oszillators 68–188 ms), startet der Prozessor neu.
  Danach sind die Motoren aus. Beim Start meldet das Terminal
  `Letzter Neustart durch den Watchdog`, wenn das passiert ist. Der Watchdog
  läuft erst ab `>> Bereit.`, also nach der Gyro-Kalibrierung.
- Fault-Handler, `Error_Handler` und `assert_failed` setzen als Erstes alle
  vier Motorpulse auf Stopp. Der Motortimer läuft sonst in Hardware weiter
  und gibt endlos den letzten Gaswert aus.

## Akku und Strom

**Spannung:** Teiler 11:1 an PC0, 100 Messungen pro Sekunde, Tiefpass 0,5 s.
Der Filter startet beim Anstecken mit dem ersten Messwert. Nach 0,5 s wird
die Zellenzahl bestimmt (3,3–4,38 V pro Zelle, also auch HV-LiPo voll) und bleibt dann fest. Bei
teilentladenen 5S/6S-Akkus ist das nicht eindeutig (ein 6S mit 3,5 V/Zelle
sieht aus wie ein voller 5S). Wer immer denselben Akkutyp fliegt, trägt die
Zellenzahl fest in `BAT_CELLS_DEFAULT` (`Sensors/battery.h`) ein.

**Warnung und Gasbegrenzung:**

| Zustand | Zellspannung | Gas höchstens | LED |
|---|---|---|---|
| OK | ab 3,50 V | 100 % | blinkt mit 2 Hz |
| Warnung | unter 3,50 V | 80 % | blinkt mit 5 Hz |
| Kritisch | unter 3,30 V | 60 % | blinkt mit 12,5 Hz |
| Sensor fehlt / Kalibrierung gescheitert | – | Armen gesperrt | leuchtet dauernd |

Gearmt zählt ein Zustand erst, wenn die Spannung 2 s lang unter der
Schwelle bleibt (Spannungseinbrüche bei Vollgas lösen nichts aus), und er
wird bis zum Disarm nicht wieder besser. Die Gasgrenze sinkt mit 20 % pro
Sekunde, damit die Drohne nicht absackt. Werte in `battery.h`.

**Telemetrie an die Funke:** Die Firmware schickt fünfmal pro Sekunde
Spannung, Strom, verbrauchte mAh und eine grobe Restladung an den
ELRS-Empfänger. Am Sender unter Telemetrie „Sensoren suchen“; danach gibt es
die Sensoren RxBt, Curr, Capa und Bat%. Für eine Ansage bei niedrigem Akku
dort eine Warnschwelle (z. B. RxBt < 14,0 V bei 4S) oder eine
Spezialfunktion einstellen.

**Strom:** Stromausgang „C“ des 4in1-ESC an PA4. PA4 verträgt höchstens
3,3 V – vor dem Anschließen am Ausgang messen (Akku dran, Motoren aus und
unter Last). Schutzbeschaltung (4,7 kΩ in Reihe, 100 nF nach Masse) siehe
„Verdrahtung“, Punkt 2.

Die Skalierung ist für den B-Cube-4in1 nicht gesichert. Voreingestellt sind
12,75 mV/A (`CURR_MV_PER_A_DEFAULT`, Angabe von B-Cube für den AM32-4in1),
üblich sind 12–40 mV/A. Deshalb kalibrieren:

1. Nullpunkt: Wird automatisch gemessen, 3 s nach dem Anstecken des Akkus
   über 0,5 s (disarmed, Motoren stehen; vorher piepst der ESC). Streuen die
   Werte um mehr als 5 mV, misst die Firmware neu. Bis dahin zeigt sie 0 A
   und zählt keine mAh. Was Flugsteuerung, Empfänger und Kamera dauerhaft
   ziehen, zählt als 0 A. Von Hand: `curzero`.
2. Skalierung über die Kapazität (Hauptweg, ohne Zusatzgeräte): Akku voll
   laden, fliegen bzw. ohne Propeller laufen lassen, danach `bat` ablesen und
   mit der Menge vergleichen, die das Ladegerät wieder hineinlädt.
   Neuer Wert: `CURR_MV_PER_A_DEFAULT × angezeigte mAh / geladene mAh`.
3. Alternativ mit Zangen- oder Leistungsmesser in der Akkuleitung: über
   `arm`/`ja` und `t <gas>` im Terminal (ohne Propeller, ohne Funk) einen
   gleichmäßigen Strom einstellen (mindestens 1 A, besser 5–10 A), dann
   `curcal <ampere>` – das geht auch gearmt. Gearmt antwortet das Terminal
   nicht; nach `disarm` zeigt `bat` den neuen mV/A-Wert.

`batcal` nimmt nur Werte an, die höchstens 20 % von der Messung abweichen
(sonst Tippfehler oder falsche Widerstände). Beide Kalibrierungen (`batcal`, `curcal`) liegen nur im RAM und sind nach
einem Neustart weg. Den ermittelten mV/A-Wert dauerhaft in
`CURR_MV_PER_A_DEFAULT` eintragen.

## Terminal-Befehle

| Befehl | Wirkung |
|---|---|
| `arm`, dann `ja` | Motoren scharf, nur ohne Funk und mit Sicherheitsabfragen. Gearmt mindestens jede Sekunde ein Zeichen senden (Enter alle 0,5 s), sonst aus |
| `disarm` (`stop`) | Motoren aus. Danach armt der Arm-Schalter erst wieder, wenn er einmal unten war |
| `status` | Zustand, Sensorfehler und ob Armen gerade erlaubt wäre |
| `bat` | Spannung, Zellen, Strom, verbrauchte mAh, Gasgrenze |
| `loop`, `rc`, `pid`, `mix`, `filt` | Anzeigen |
| `angle`, `rate` | Flugmodus |
| `stream on` / `stream off` | Lage und Drehraten mit 50 Hz ausgeben (nur disarmed) |
| `kipp` | Kipptest-Anzeige an/aus (nur disarmed) |
| `t <gas>` | Gas in %, nur gearmt und ohne Funk (mit Funk überschreibt der Knüppel) |
| `rrp 1.2` usw. | PID-Wert setzen: Kreis (r/a), Achse (r/p/y), Term (p/i/d) |
| `glpf <hz>`, `alpf <hz>`, `tau <s>` | Filter einstellen |
| `batcal <volt>` | Akkuspannung kalibrieren |
| `curzero`, `curcal <ampere>` | Strommessung: Nullpunkt setzen / Skalierung mit bekanntem Strom |
