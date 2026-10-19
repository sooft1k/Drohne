# Drohne – Flugsteuerung STM32F405

Eigene Flugsteuerung für einen Quadrocopter mit STM32F405RGT6 (STM32-HAL).

| Teil | Anschluss |
|---|---|
| Gyro/Accel | MPU6050 an I2C1 (PB6 SCL, PB7 SDA), 400 kHz |
| Motoren | TIM3, Oneshot125: M1 = PB0, M2 = PB1, M3 = PA6, M4 = PA7 |
| Funk | ELRS-Empfänger, CRSF 420000 Baud an USART1 (PA9 TX, PA10 RX) |
| Terminal | USART2 (PA2 TX, PA3 RX), 115200 Baud |
| Akkuspannung | PC0 (ADC1 Kanal 10), Teiler 11:1 |

Regeltakt 1 kHz über TIM2. Flugmodi Angle (selbstaufrichtend) und Rate (Acro).

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
   (Gyro-Kalibrierung).
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
12. Mit verbundener Funke, flach liegend:
    - Roll-Knüppel rechts → **M3 und M4 höher** (linke Seite hebt sich)
    - Pitch-Knüppel nach vorne → **M1 und M3 höher** (Heck hebt sich)
    - Yaw-Knüppel rechts → **M2 und M3 höher**
13. `kipp` beendet die Anzeige.

Ist in Teil B eine Richtung verkehrt, obwohl Teil A stimmt: nicht fliegen,
Mixer und Regler prüfen.

### Teil C – Motoren (Akku dran, Propeller ab)

14. Funke an, Arm-Schalter unten, Gas ganz unten. Akku anstecken.
15. Arm-Schalter hoch: Alle vier Motoren laufen im Leerlauf. Drehrichtung
    jedes Motors prüfen (Klebeband-Fähnchen auf der Glocke), von oben gesehen:
    M1 hinten rechts CW, M2 vorne rechts CCW, M3 hinten links CCW,
    M4 vorne links CW.
16. Modus Angle, Gas auf etwa 25 %, Drohne gut festhalten:
    - Nase nach unten kippen → die vorderen Motoren laufen schneller.
    - Rechte Seite nach unten → die rechten Motoren laufen schneller.
17. Gas ganz zurück: Die Motoren laufen im Leerlauf weiter und gehen nicht aus.
18. Failsafe: Gas auf etwa 30 %, dann die Funke ausschalten. Nach 0,5 s
    nimmt die Firmware das Gas in 1,5 s auf Leerlauf zurück und schaltet
    danach die Motoren ab. Funke wieder an: Die Motoren bleiben aus, bis der
    Arm-Schalter einmal unten war.
19. Arm-Schalter runter: Motoren sofort aus.

## Leerlauf und Gas

Solange gearmt ist, laufen alle Motoren mindestens mit `MIXER_IDLE_PERCENT`
(15 %, in `Sensors/mixer.h`). Gas 0–100 % wird linear auf Leerlauf bis Vollgas
abgebildet, es gibt keinen Sprung. Disarmed sind alle Motoren aus.

## Failsafe

Kommt länger als `CRSF_TIMEOUT_MS` (500 ms) kein gültiges Funkpaket:

- disarmed: nichts passiert, Arming bleibt gesperrt
- gearmt: Angle-Modus wird erzwungen, Roll/Pitch/Yaw-Sollwert 0 (waagrecht,
  kein Gieren), Gas läuft in `RC_FS_RAMP_S` (1,5 s) linear auf 0, danach
  wird disarmt

Die Rampe läuft immer bis zum Ende, auch wenn das Signal zwischendurch
zurückkommt. Danach lässt sich erst wieder armen, wenn der Arm-Schalter
einmal unten war.

## Terminal-Befehle

| Befehl | Wirkung |
|---|---|
| `arm` / `disarm` (`stop`) | Motoren scharf / aus |
| `status`, `loop`, `rc`, `bat`, `pid`, `mix`, `filt` | Anzeigen |
| `angle`, `rate` | Flugmodus |
| `stream on` / `stream off` | Lage und Drehraten mit 50 Hz ausgeben |
| `kipp` | Kipptest-Anzeige an/aus (nur disarmed) |
| `t <gas>` | Gas in %, nur ohne Funk |
| `rrp 1.2` usw. | PID-Wert setzen: Kreis (r/a), Achse (r/p/y), Term (p/i/d) |
| `glpf <hz>`, `alpf <hz>`, `tau <s>` | Filter einstellen |
| `batcal <volt>` | Akkuspannung kalibrieren |
