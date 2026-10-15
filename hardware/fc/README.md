# Flight Controller – KiCad-Projekt

Eigene FC-Platine für die 5-Zoll-Drohne nach `fc-spezifikation.md` (Vorgabe des Nutzers, liegt nicht im Repo; die
Spalte „Spezifikation“ der Abweichungstabelle bezieht sich darauf).
Erstellt mit KiCad 7.0 (öffnet auch in KiCad 8/9; dabei wird einmalig konvertiert).

| Datei | Inhalt |
|---|---|
| `fc.kicad_pro` | Projekt mit Designregeln und Netzklassen |
| `fc.kicad_sch` | Schaltplan (ein A1-Blatt, nach Funktionsgruppen, Netzlabels + Versorgungssymbole) |
| `fc.kicad_pcb` | Platine: 42 × 41 mm, 4 Lagen, platziert und vollständig geroutet |
| `fc_custom.kicad_sym` | Projektsymbole: ICM-42688-P, Versorgungssymbole VBAT, +3V3A und +4V5 |
| `fc_custom.pretty` | LMR33630-Footprint ohne Kupfer auf der Unterseite (dort sitzen Steckverbinder); USB-C-Buchse mit an der Platinenkante gekürztem Bestückungsdruck |
| `fertigung/` | Gerber + Bohrdateien (ZIP für JLCPCB), Stückliste und Bestückkoordinaten für beide Seiten, Prüfliste |
| `docs/` | Schaltplan als PDF, Lagenbilder (oben, unten gespiegelt, Lage 2, Lage 3), 3D-Ansichten, DRC- und ERC-Bericht, Routing-Kennzahlen |
| `scripts/` | Generatoren – alles wird aus `scripts/design.py` erzeugt |

## Neu erzeugen

```
export KICAD7_FOOTPRINT_DIR=/usr/share/kicad/footprints
cd hardware/fc
mkdir -p build
python3 scripts/gen_lib.py      # Projektbibliothek
python3 scripts/gen_sch.py      # Schaltplan
kicad-cli sch export netlist -o build/fc.net fc.kicad_sch
python3 scripts/check_netlist.py build/fc.net   # Schaltplan == design.py, Pin für Pin
python3 scripts/gen_pcb.py      # Umriss, Bohrungen, Platzierung, Flächen, Handverdrahtung, Fanout
python3 scripts/route.py        # Autorouting (Freerouting 2.0.1, Java) + Nachrouter, Zonen füllen
python3 scripts/run_drc.py fc.kicad_pcb build/drc.rpt
python3 scripts/report.py fc.kicad_pcb build/fc.net   # Kennzahlen + Abgleich Platine/Schaltplan
python3 scripts/sync_fields.py  # nur Werte/LCSC-Nummern in die geroutete Platine uebernehmen
python3 scripts/fab.py          # Gerber, Bohrdaten, ZIP, JLCPCB-BOM/CPL (beide Seiten), Pruefliste
# Doku (docs/)
cp build/drc.rpt docs/fc_drc_bericht.txt
python3 scripts/report.py fc.kicad_pcb build/fc.net > docs/fc_routing_kennzahlen.txt
python3 scripts/kontur.py > docs/fc_aussenkontur.txt
python3 scripts/render.py fc.kicad_pcb build/fc top,bot,in1,in2 && cp build/fc_{top,bot,in1,in2}.png docs/
kicad-cli sch export pdf -o docs/fc_schaltplan.pdf fc.kicad_sch
```

ERC und 3D-Ansichten mit KiCad 9 (kicad-cli 7 kann beides nicht):
`kicad-cli sch erc --format report --severity-all -o erc.rpt fc.kicad_sch`, für die 3D-Bilder
`kicad-cli pcb render --side top|bottom --quality high --floor -w 1800 -h 1800 -o docs/fc_3d_oben.png fc.kicad_pcb`
(schräg: `--perspective --rotate 305,0,330 --zoom 0.75` bzw. `--side bottom --rotate 55,0,210`).
Die JST-SH-3D-Modelle gibt es dort nur als `.step`; in einer Arbeitskopie der Platine die
`.wrl`-Verweise der Buchsen auf `.step` ändern.

`route.py` probiert für den ersten Freerouting-Lauf bis zu sechs Parametersätze (Via- und
Rip-up-Kosten) und nimmt den ersten vollständigen; bleiben Verbindungen offen, folgen bis zu
drei weitere Freerouting-Läufe auf dem gerouteten Stand und zuletzt ein eigener Nachrouter
(A*-Suche auf 0,1-mm-Raster, F.Cu/B.Cu mit Vias, darf notfalls Autorouter-Bahnen eines anderen
Netzes aufreißen und neu verlegen). Freerouting ist nicht vollständig deterministisch – ein
neuer Lauf ergibt ein anderes, ebenfalls zu prüfendes Routing. Maßgeblich ist die eingecheckte
`fc.kicad_pcb`.

## Lagenaufbau

| Lage | Verwendung |
|---|---|
| 1 F.Cu | Bauteile, Signale, Restfläche GND |
| 2 In1.Cu | durchgehende GND-Fläche (nur Via-Durchbrüche, keine Leiterbahnen, keine Schlitze) |
| 3 In2.Cu | Versorgung: +3V3 (Mitte unter MCU/Gyro/Flash), +5V (Ost, Süd, West), VBAT (Ring am Rand: Nordband zum Regler, schmale Streifen West/Süd zum Verpolschutz). Die 4,5-V-Schiene (+4V5) hat keine Fläche, sie ist als Leiterbahn (0,4 mm, am Diodenknoten 0,6 mm) auf F.Cu/B.Cu verlegt |
| 4 B.Cu | Steckverbinder (JST-SH, außer ELRS/I²C/GPS), Signale, Restfläche GND |

Keine Leiterbahn auf Lage 2 und 3 – dort liegen nur Flächen. Montagebohrungen M3
(Ø 3,2 mm) auf 30,5 × 30,5 mm; nur **H2 (Südost, neben dem ESC-Stecker)** ist mit GND
verbunden, die anderen drei sind nicht durchkontaktiert und haben 3,3 mm Kupfer-Sperrkreis
für die Abstandshalter.

## Flugrichtung

**Vorn ist die Nordkante.** Sie ist auf der Oberseite mit „VORNE“ und zwei Pfeilen markiert.
Von oben gesehen, vorn oben: der ESC-Stecker J2 liegt hinten (Unterseite, Südkante), USB-C
rechts (Ostkante), ELRS/I²C/GPS links (Westkante). Alle Lageangaben in dieser Datei
(Nord/Süd/Ost/West, oben/unten im Bild) beziehen sich auf diese Ansicht.

## Pinbelegung STM32F405RGT6 (für die Firmware)

Die Fallstricke für die Firmware stehen gesammelt im Abschnitt
[Firmware-Hinweise](#firmware-hinweise) (NOVBUSSENS, DMA-Konflikt, Gyro-Ausrichtung).


| Pin | Port | Netz | Funktion |
|---|---|---|---|
| 26 | PB0 | M1 | TIM3_CH3 (wie bisher, `Sensors/motor.c`) – DMA-Konflikt siehe Firmware-Hinweise |
| 27 | PB1 | M2 | TIM3_CH4 (wie bisher) |
| 22 | PA6 | M3 | TIM3_CH1 (wie bisher) |
| 23 | PA7 | M4 | TIM3_CH2 (wie bisher) |
| 8 | PC0 | VBAT_SENSE | ADC123_IN10, Teiler 100 k / 10 k (11:1) |
| 20 | PA4 | CURR_SENSE | **ADC12_IN4** (ADC1 oder ADC2, Kanal 4), Strommessausgang „C“ des ESC (J2 Pin 7) über 1 k / 100 nF – unverändert |
| 55 | PB3 | SPI1_SCK | Gyro + Flash |
| 56 | PB4 | SPI1_MISO | Gyro + Flash |
| 57 | PB5 | SPI1_MOSI | Gyro + Flash |
| 58 | PB6 | GYRO_CS | 10 k Pull-up |
| 52 | PC11 | GYRO_INT1 | EXTI11 (Data-Ready) |
| 59 | PB7 | GYRO_INT2 | Gyro-Pin 9 (INT2/FSYNC/CLKIN), TIM4_CH2 → optional 32-kHz-CLKIN |
| 51 | PC10 | FLASH_CS | 10 k Pull-up |
| 42 | PA9 | ELRS_TX | USART1_TX (wie bisher) – **PA9 ist zugleich VBUS-Erkennung des USB: NOVBUSSENS setzen**, siehe Firmware-Hinweise |
| 43 | PA10 | ELRS_RX | USART1_RX (wie bisher) |
| 16 | PA2 | DBG_TX | USART2_TX (wie bisher) |
| 17 | PA3 | DBG_RX | USART2_RX (wie bisher) |
| 53 | PC12 | GPS_TX | UART5_TX |
| 54 | PD2 | GPS_RX | UART5_RX |
| 37 | PC6 | VTX_TX | USART6_TX |
| 38 | PC7 | VTX_RX | USART6_RX |
| 61 | PB8 | I2C_SCL | I2C1 (AF4) – **neu: bisher PB6** |
| 62 | PB9 | I2C_SDA | I2C1 (AF4) – **neu: bisher PB7** |
| 44 | PA11 | USB_DM | USB OTG FS |
| 45 | PA12 | USB_DP | USB OTG FS |
| 46 | PA13 | SWDIO | |
| 49 | PA14 | SWCLK | |
| 2 | PC13 | LED_ARM | LED rot (Druck „A“), **aktiv-low** (Pin = 0 → LED an) |
| 3 | PC14 | LED_ERR | LED gelb (Druck „E“), aktiv-low |
| 4 | PC15 | LED_LINK | LED grün (Druck „L“), aktiv-low |
| 14 | PA0 | BUZZER | TIM5_CH1 (AF2) bzw. TIM2_CH1 (AF1), high = Piepser an (N-FET AO3400A). TIM2 ist in der Firmware der Regeltakt – für Ton-PWM TIM5 nehmen |
| 9 | PC1 | GPIO1 | Buchse GPIO (J10) Pin 1 |
| 10 | PC2 | GPIO2 | Pin 2 |
| 11 | PC3 | GPIO3 | Pin 3 |
| 25 | PC5 | GPIO4 | Pin 4 |
| 21 | PA5 | GPIO5 | Pin 5 |
| 24 | PC4 | GPIO6 | Pin 6 |
| 5/6 | PH0/PH1 | OSC_IN/OUT | 8-MHz-Quarz (PH1 über 0-Ω-Brücke R3) |
| 60 | BOOT0 | BOOT0 | 10 k nach GND, Lötbrücke JP1 nach 3,3 V = DFU |
| 28 | PB2 | BOOT1 | 10 k nach GND (für zuverlässigen DFU-Start) |
| 7 | NRST | NRST | 100 nF, auch an SWD |

Frei: PA1, PA8, PA15, PB10–PB15, PC8, PC9.

Die Motorpins sind beim Umbau von J2 unverändert geblieben (passend zu `Sensors/motor.c`);
umgebaut ist nur der Stecker. M3/M4 und M1/M2 kreuzen sich deshalb auf dem kurzen Weg zum
ESC-Stecker (je ein Via).

## Firmware-Hinweise

### 1. USB: NOVBUSSENS setzen

PA9 ist USART1_TX (ELRS) und zugleich der VBUS-Erkennungspin des USB-OTG-FS. VBUS ist auf
dieser Platine an **keinen** Prozessorpin geführt. Die VBUS-Erkennung muss deshalb **aus**
sein, sonst wartet der USB-Kern auf VBUS an PA9 (dort liegt das ELRS-Sendesignal) und der FC
meldet sich am PC nicht an.

- Register: `OTG_FS_GCCFG.NOVBUSSENS = 1`, `VBUSASEN = 0`, `VBUSBSEN = 0`
- STM32Cube-HAL: `hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;` (setzt genau das).
  Die Firmware im Repo nutzt USB noch nicht; der Hinweis gilt, sobald USB dazukommt.
- Betaflight: Ziele mit UART1 auf PA9/PA10 laufen so; ein USB-Erkennungspin
  (`USB_DETECT_PIN`) existiert auf dieser Platine nicht.

### 2. DMA-Konflikt: DShot ↔ GPS-Senden ↔ Debug-Empfang

Alle vier Motoren hängen an TIM3. DShot über die Kanal-DMA von TIM3 belegt vier Streams von
DMA1 (Kanal 5). Zwei davon braucht auch Kanal 4 für UART5-TX (GPS) bzw. USART2-RX (Debug).
Ein Stream kann immer nur einen Kanal bedienen (RM0090, Tabelle 42):

| Motor | Pin | Timer | DMA1-Stream | Konflikt |
|---|---|---|---|---|
| M1 | PB0 | TIM3_CH3 | **Stream 7**, Kanal 5 | **UART5_TX** = GPS-Senden (Stream 7, Kanal 4) |
| M2 | PB1 | TIM3_CH4 | Stream 2, Kanal 5 | – |
| M3 | PA6 | TIM3_CH1 | Stream 4, Kanal 5 | – |
| M4 | PA7 | TIM3_CH2 | **Stream 5**, Kanal 5 | **USART2_RX** = Debug-Empfang (Stream 5, Kanal 4) |

Lösungen (eine genügt):
- **DShot im Burst-Modus**: alle vier Kanäle über TIM3_UP auf DMA1 Stream 2 (Kanal 5). Dann
  sind Stream 5 und 7 frei für Debug-Empfang und GPS-Senden.
- **GPS-Senden und Debug-Empfang ohne DMA** (Interrupt). Zum GPS gehen nur
  Konfigurationsbefehle, der Debug-Empfang ist langsam.
- **Betaflight**: Mit den Standardwerten (`dshot_bitbang = AUTO`, `dshot_bidir = OFF`,
  `dshot_burst = OFF`) nutzt Betaflight auf F4 die Kanal-DMA von TIM3, also genau den
  Konfliktpfad. AUTO schaltet auf F4 nur bei `dshot_bidir = ON` auf Bitbang. Deshalb
  `dshot_bitbang = ON` (Bitbang, keine TIM3-DMA) oder `dshot_burst = ON` setzen.

Nicht betroffen: ELRS (USART1) und VTX (USART6) liegen auf DMA2. Die Firmware im Repo
(Stand jetzt: Oneshot125 per Timer-Vergleichsregister, UART-Empfang per Interrupt, kein DMA)
hat den Konflikt noch nicht – er entsteht erst beim Umstieg auf DShot mit Timer-DMA.

### 3. Gyro-Ausrichtung: CW270

Der ICM-42688-P (U2) ist um 180° gedreht eingebaut. Mit der Nordkante vorn zeigen die
Sensorachsen so:

| Sensorachse | zeigt nach |
|---|---|
| +X | links (Westkante) |
| +Y | hinten (ESC-Stecker) |
| +Z | oben |

- **Betaflight bis 4.5**: `set gyro_1_sensor_align = CW270` (Konfigurator: Gyro-Ausrichtung
  „CW 270°“), Board-Ausrichtung 0°.
- **Betaflight ab 2025.12**: Die Gyro-Ausrichtung ist dort nicht mehr einstellbar, sie kommt aus
  der Board-Konfiguration. In der eigenen Konfiguration `#define GYRO_1_ALIGN CW270_DEG`
  setzen. Ersatzweise mit einer Konfiguration ohne diese Angabe (Vorgabe CW0):
  `set align_board_yaw = 270` – rechnerisch dasselbe.
- **Eigene Firmware** (Körperachsen X vorn, Y links, Z oben):
  `x = −y_chip`, `y = +x_chip`, `z = +z_chip`. Für X vorn, Y rechts, Z unten:
  `x = −y_chip`, `y = −x_chip`, `z = −z_chip`. Gilt für Drehrate und Beschleunigung.
- Kontrolle im Konfigurator (3D-Modell): Nase nach unten kippen → Modell kippt nach vorn;
  rechte Seite nach unten → Modell rollt nach rechts.

### 4. Strommessung und Akkuspannung

| Messung | Pin | ADC-Kanal | Beschaltung |
|---|---|---|---|
| Strom (ESC-Pin „C“, J2 Pin 7) | **PA4** (LQFP-Pin 20) | **ADC12_IN4** (ADC1 oder ADC2, Kanal 4) | 1 k in Reihe, 100 nF nach GND |
| Akkuspannung | PC0 (LQFP-Pin 8) | ADC123_IN10 | Teiler 100 k / 10 k (11:1) |

Skalierung des Stromsignals (mV/A, Offset) aus dem Datenblatt des B-Cube 4in1 übernehmen.

### 5. Versorgung an USB

An USB allein (kein Akku) laufen: Prozessor, Gyro, Flash, LEDs, alle 3,3-V-Pins (I²C, UART,
GPIO, SWD) sowie **ELRS (J3) und GPS (J5)** über die 4,5-V-Schiene. **Nicht** versorgt sind
VTX (J6), Kamera (J7) und Piepser (J11) – sie hängen an +5 V, und das kommt nur aus dem
Schaltregler (D7 sperrt Richtung +5 V). Mit Akku liegt die 4,5-V-Schiene bei etwa 4,6 V
(5,0 V minus Schottky-Diode), an USB bei etwa 4,5–4,7 V. Der AMS1117 braucht etwa 1,1 V über
sich: unter etwa 4,4 V am Eingang (schwacher USB-Port, langes Kabel) sinkt die 3,3-V-Schiene
leicht ab, der Prozessor läuft trotzdem.

Mit Akku und ohne USB misst man an VBUS (USB-C-Buchse, C33) bis zu etwa 4,5 V: Der Sperrstrom
von D2 (µA) lädt C33/C34 auf, weil VBUS keinen Ableitwiderstand hat. Das ist keine
Rückspeisung (kein Strom), nur eine hochohmige Spannung.

### 6. LEDs und Piepser

LEDs PC13–PC15 aktiv-low (Pin = 0 → LED an), Piepser PA0 aktiv-high (N-FET). Der Piepser hängt
an +5 V und bleibt an USB stumm. Für einen Ton per PWM TIM5_CH1 verwenden: TIM2 ist in der
Firmware schon die 1-kHz-Zeitbasis des Regeltakts (`Core/Src/main.c`).

## Steckverbinder (JST-SH 1,0 mm, seitlich steckbar)

Alle Buchsen sind JST-SH-kompatibel (SMxxB-SRSS-TB, bestückt wird Megastar ZX-SH1.0-xPWT) und
werden von JLCPCB mitbestückt. Passende Gegenstecker: JST SHR-0xV-S bzw. die üblichen
„JST-SH 1,0 mm“-Kabel. **ELRS, I²C und GPS sitzen oben** an der Westkante (beim Entwickeln oft
gesteckt, im verschraubten Stack erreichbar), alle übrigen auf der Unterseite (vor dem
Verschrauben einstecken). Lageangaben von oben gesehen, vorn (Nord) oben, USB-C rechts (Ost),
ESC-Stecker hinten (Süd); genaue Maße für den Rahmen in `docs/fc_aussenkontur.txt`
(`scripts/kontur.py`).

| Ref | Name | Seite, Lage, Steckrichtung | Pin 1 → n |
|---|---|---|---|
| J5 | GPS, 4-polig | **oben**, Westkante Nord, von Westen | 1 TX · 2 RX · 3 4,5 V (auch an USB) · 4 GND |
| J4 | I2C, 4-polig | **oben**, Westkante Mitte, von Westen | 1 SDA · 2 SCL · 3 3V3 · 4 GND |
| J3 | ELRS, 4-polig | **oben**, Westkante Süd, von Westen | 1 TX · 2 RX · 3 4,5 V (auch an USB) · 4 GND |
| J2 | ESC (B-Cube 4in1), 8-polig | unten, Südrand Mitte (hinten), von Süden | 1 GND · 2 VBAT · 3 M1 · 4 M2 · 5 M3 · 6 M4 · 7 Strom (C) · 8 NC (unbeschaltet) – Lage von oben siehe unten |
| J6 | VTX, 4-polig | unten, Südrand West, von Süden | 1 TX · 2 RX · 3 5V (nur mit Akku) · 4 GND |
| J11 | Piepser, 2-polig | unten, Nordrand West, von Norden | 1 +5V · 2 BZ− (Drain) |
| J9 | UART (Debug), 4-polig | unten, Nordrand Mitte, von Norden | 1 TX · 2 RX · 3 3V3 · 4 GND |
| J10 | GPIO, 8-polig | unten, Westrand, von Westen | 1–6 GPIO1–6 · 7 3V3 · 8 GND |
| J7 | CAM, 3-polig | unten, Westrand, von Westen | 1 5V (nur mit Akku) · 2 VIDEO · 3 GND (VIDEO zusätzlich auf Lötpad TP5) |
| J8 | SWD, 5-polig | unten, Ostrand, von Osten | 1 SWDIO · 2 SWCLK · 3 NRST · 4 3V3 · 5 GND |
| J1 | USB-C | oben, Ostrand (steht 0,5 mm über) | |

Zählweise: Von der Bestückungsseite der Buchse aus gesehen, Öffnung zum Betrachter, ist
Kontakt 1 links (wie im JST-Datenblatt). Ein normales 1:1-Kabel verbindet Kontakt n mit
Kontakt n der Gegenbuchse. Pin 1 ist im Bestückungsdruck mit einem Strich markiert. Die
Buchsen am Rand sitzen 0,7 mm hinter der Platinenkante (Kupferabstand 0,5 mm).

TX/RX jeweils aus Sicht des FC (FC-TX an Empfänger-RX anschließen).

### ESC-Stecker J2 (B-Cube 4in1)

Belegung nach der Messung am ESC: Beschriftung auf dessen bestückter Oberseite, Öffnung vom
Betrachter weg, links → rechts **NC · C · M4 · M3 · M2 · M1 · V · G**. Bei einer normalen
JST-SH-Buchse liegt in dieser Ansicht Kontakt 1 rechts, also am ESC 1 G, 2 V, 3 M1, 4 M2, 5 M3,
6 M4, 7 C, 8 NC. J2 ist genauso belegt, damit ein **1:1-Kabel** (Kontakt n ↔ Kontakt n) passt.

**J2 von oben auf den FC gesehen** (vorn = Nordkante oben, USB-C rechts): Die Buchse sitzt
darunter an der Hinterkante, ihre Öffnung zeigt nach hinten zum Betrachter. Von links nach
rechts:

```
            vorn
   ┌──────────────────────┐
   │                      │
   │       FC (oben)      │ USB-C
   │                      │
   │ NC  C  M4 M3 M2 M1 V  G   ← J2 auf der Unterseite, Öffnung nach hinten
   └──────────────────────┘
            hinten
```

G liegt rechts außen (neben der Massebohrung H2), NC links außen.

**Beide Stecker in Einbaulage zum Gegenhalten beim Zusammenbau.** FC über dem ESC, beide
Oberseiten oben. Beide Buchsen jeweils **von oben** angesehen, die **Öffnung zeigt zu dir**.
In Klammern steht die Kontaktnummer:

```
  FC – J2 (auf der FC-Unterseite, kopfüber)
  ┌────────────────────────────────────────────────┐
  │  NC    C     M4    M3    M2    M1    V     G   │
  │ (8)   (7)   (6)   (5)   (4)   (3)   (2)   (1)  │
  └────────────────────────────────────────────────┘
                   Öffnung zu dir ↓

  ESC – Buchse (auf der ESC-Oberseite)
  ┌────────────────────────────────────────────────┐
  │  G     V     M1    M2    M3    M4    C     NC  │
  │ (1)   (2)   (3)   (4)   (5)   (6)   (7)   (8)  │
  └────────────────────────────────────────────────┘
                   Öffnung zu dir ↓

  Kabel 1:1:  FC (1) ↔ ESC (1)  …  FC (8) ↔ ESC (8)
              G↔G, V↔V, M1↔M1, M2↔M2, M3↔M3, M4↔M4, C↔C, NC↔NC
```

Von oben gesehen ist die Reihenfolge **spiegelbildlich** (am FC G rechts, am ESC G links). Das
ist richtig so, weil J2 kopfüber auf der Unterseite sitzt. **J2 und das Kabel nicht
spiegeln.** Gespiegelt läge die Akkuspannung über das Kabel an PA4.

Am ESC steht die Beschriftung in dieser Ansicht auf dem Kopf. Mit der Öffnung von dir weg ist
sie lesbar, dann liest du dort NC · C · M4 · M3 · M2 · M1 · V · G. Die ESC-Buchse sitzt auf der
Oberseite, auf derselben Seite wie die Aufschrift „B-Cube 4in1 ESC“ und die Motorlötpads; das
hast du bestätigt.

- **NC (Pin 8)** ist auf dem FC unbeschaltet: kein Netz, keine Leiterbahn, nur das Lötpad.
  Vorher lag dort M4.
- **Strommessung C (Pin 7)** führt über R6 (1 k) und C15 (100 nF) an **PA4 = ADC12_IN4**
  (LQFP-Pin 20), derselbe ADC-Pin wie vorher.
- **5 V vom ESC:** Der B-Cube hat keinen 5-V-Pin. Die Lötbrücke **JP2** und das Netz ESC_5V
  sind **entfernt**. Am früheren 5-V-Pin (Pin 4) liegt jetzt M2.
- **Kabel:** Die Belegung setzt ein 1:1-Kabel voraus (Kontakt n auf Kontakt n). Bitte vor
  dem Bestellen prüfen.
- **Testpunkte** (Unterseite, runde 1,5-mm-Lötpads ohne Beschriftung; Lage wie überall hier
  von oben gesehen, in `docs/fc_bot.png` gespiegelt): TP3 VBAT an der Westkante südlich der CAM-Buchse (1,4 mm vom Rand),
  TP4 GND in der Nordhälfte rechts der Mitte, TP1 3V3 links daneben in derselben Reihe,
  TP2 5V unter der USB-C-Buchse, TP5 VIDEO südlich der Mitte. GND auch am Kupferring der
  Bohrung H2 (hinten rechts).

#### Prüfung vor dem ersten Akku (Pflicht)

**Bedingungen:** Kabel zwischen FC und ESC gesteckt · **kein Akku** angeschlossen · **vor dem
Verschrauben des Stacks**: noch keine Schrauben und Abstandshalter zwischen FC und ESC, weil
Metallschrauben G und H2 auch ohne Kabel verbinden würden.

1. **Durchgang:** Vom G-Pad des ESC (Akku-Minus) zum Kupferring der Massebohrung H2 (hinten
   rechts) oder zu TP4 (GND). **Soll: piept** bzw. nahe 0 Ω.
2. **Diodentest:** Rote Spitze auf das V-Pad des ESC (Akku-Plus), schwarze Spitze auf TP3
   (VBAT). **Soll: etwa 0,4–0,7 V.** Das ist die Body-Diode des Verpolschutz-FET. Der Wert
   kann kurz wandern, solange sich die Kondensatoren aufladen. Umgekehrt gemessen zeigt das
   Gerät OL oder einen deutlich höheren Wert (über 1,5 V); entscheidend ist der Vorwärtswert.

**Weicht eine der beiden Messungen ab:** Keinen Akku anstecken und den Stack nicht
verschrauben. Dann melden, welche Messung abweicht und was das Messgerät angezeigt hat (Wert
und Richtung). Wahrscheinliche Ursache ist ein Kabel, das nicht 1:1 ist oder verdreht steckt –
das wird erst geklärt, bevor Strom drauf kommt.

## Footprints

| Referenzen | Footprint | Bauteil |
|---|---|---|
| C1, C2, C3, C4, C5, C7, C8, C9, C10, C11, C12, C13, C14, C15, C16, C17, C18, C20, C23, C24, C25, C28, C30, C32, C34 | `Capacitor_SMD:C_0402_1005Metric` | 100nF (alle 50 V), 18pF, 1uF, 2.2uF |
| C6, C29, C33 | `Capacitor_SMD:C_0603_1608Metric` | 10uF/25V |
| C21 | `Capacitor_SMD:CP_Elec_6.3x5.4` | 47uF/35V |
| C22 | `Capacitor_SMD:C_1210_3225Metric` | 10uF/50V |
| C26, C27, C31 | `Capacitor_SMD:C_0805_2012Metric` | 22uF, 22uF/16V |
| D1, D6 | `Diode_SMD:D_SOD-323` | 1N4148WS, BZT52C15S |
| D2, D7 | `Diode_SMD:D_SMA` | SS14 (D7 auf der Unterseite) |
| D3, D4, D5 | `LED_SMD:LED_0603_1608Metric` | gelb, gruen, rot |
| FB1 | `Inductor_SMD:L_0402_1005Metric` | 600R@100MHz |
| FID1, FID2, FID3 | `Fiducial:Fiducial_1mm_Mask2mm` | Fiducial |
| H1, H3, H4 | `MountingHole:MountingHole_3.2mm_M3` | M3 |
| H2 | `MountingHole:MountingHole_3.2mm_M3_Pad` | M3 GND |
| J1 | `fc_custom:USB_C_Receptacle_HRO_TYPE-C-31-M-12_Kante` (KiCad-Footprint, Bestückungsdruck an der Kante gekürzt) | USB-C |
| J2, J10 | `Connector_JST:JST_SH_SM08B-SRSS-TB_1x08-1MP_P1.00mm_Horizontal` | ESC, GPIO |
| J3, J4, J5, J6, J9 | `Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal` | ELRS, I2C, GPS, VTX, UART |
| J7 | `Connector_JST:JST_SH_SM03B-SRSS-TB_1x03-1MP_P1.00mm_Horizontal` | CAM |
| J8 | `Connector_JST:JST_SH_SM05B-SRSS-TB_1x05-1MP_P1.00mm_Horizontal` | SWD |
| J11 | `Connector_JST:JST_SH_SM02B-SRSS-TB_1x02-1MP_P1.00mm_Horizontal` | BUZZER |
| JP1 | `Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm` | BOOT0 |
| L1 | `Inductor_SMD:L_Coilcraft_XAL4040-XXX` | 3.3uH |
| Q1, Q2 | `Package_TO_SOT_SMD:SOT-23` | Si2319CDS, AO3400A |
| R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16, R17, R18, R19, R20 | `Resistor_SMD:R_0402_1005Metric` | 0R, 470R, 1k, 5.1k, 8.2k, 10k, 33k, 100k |
| TP1, TP2, TP3, TP4, TP5 | `TestPoint:TestPoint_Pad_D1.5mm` | TP1 3V3, TP2 5V, TP3 VBAT, TP4 GND, TP5 VIDEO |
| U1 | `Package_QFP:LQFP-64_10x10mm_P0.5mm` | STM32F405RGT6 |
| U2 | `Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y` | ICM-42688-P |
| U3 | `Package_SO:SOIC-8_5.23x5.23mm_P1.27mm` | W25Q128JVSIQ |
| U4 | `fc_custom:Texas_HSOP-8-1EP_3.9x4.9mm_P1.27mm_ThermalVias_TopOnly` | LMR33630BDDA |
| U5 | `Package_TO_SOT_SMD:SOT-223-3_TabPin2` | AMS1117-3.3 |
| U6 | `Package_TO_SOT_SMD:SOT-23-6` | USBLC6-2SC6 |
| Y1 | `Crystal:Crystal_SMD_3225-4Pin_3.2x2.5mm` | 8MHz |

## Abweichungen von der Spezifikation (mit Grund)

| Punkt | Spezifikation | Umgesetzt | Grund |
|---|---|---|---|
| Verpolschutz Q1 | SI2301 | **Si2319CDS** (gleicher SOT-23-Footprint) | SI2301: V_DS max. −20 V, V_GS max. ±8 V. Bei 6S (25,2 V) und 15-V-Zener am Gate sind beide Grenzen überschritten. Si2319CDS: −40 V / ±20 V |
| 5-V-Regler | MP1584 oder TPS54331 | **LMR33630BDDA** (3,8–36 V, 3 A, synchron, 1,4 MHz) | beide Vorschläge haben nur 28 V Eingangsgrenze – bei vollem 6S-Akku plus Spannungsspitzen zu knapp. Ausgang 2 × 22 µF + 100 nF (statt 1 × 22 µF) für die interne Kompensation |
| Gyro Pin 2 (RESV) | 100 nF | **offen** | laut offenem Referenzdesign (ICM-42688-P-Modul) unbeschaltet; der Kondensator hätte INT1/MISO den Weg verbaut. RESV 7/10/11 an GND, Pin 3 offen |
| USBLC6 Pin 5 (VBUS) | – | **offen** | der Pin liegt zwischen den durchgeschleiften D+/D−-Leitungen und ist ohne Via im Pad nicht erreichbar. Der Schutz der Datenleitungen arbeitet über die interne Zener weiter |
| VREF+ | an VDDA mit 100 nF | entfällt als Pin | im LQFP64 intern mit VDDA verbunden; VDDA hat 100 nF + 1 µF hinter der Ferritperle FB1 |
| I²C extern | I2C1 oder I2C2 | I2C1 auf **PB8/PB9** | PB6 wird direkt neben dem Gyro als CS gebraucht – Firmware: nur die GPIO-Pins ändern (AF4 bleibt) |
| Status-LEDs | an GPIOs | **PC13–PC15, aktiv-low** | wie die bisherige PC13-LED; PC13–PC15 dürfen nur Strom ziehen (max. 3 mA). Die LEDs sitzen östlich des Gyros, die Leitungen zu PC13–PC15 (Westseite des Prozessors) sind 15–19 mm lang |
| Stiftleisten | Stiftleiste, Unterseite | **JST-SH-Buchsen 1,0 mm** (seitlich steckbar), ELRS/I²C/GPS oben, Rest unten | die Platine soll komplett bestückt geliefert werden: JLCPCB bestückt die SMD-Buchsen mit; Stiftleisten hätten von Hand gelötet werden müssen. JST-SH ist bei FPV-Komponenten (ELRS, GPS, VTX) üblich. Oft gesteckte Buchsen oben, weil die Unterseite im Stack nur ~5 mm über dem ESC liegt |
| Teilenummern | – | 100 nF alle 50 V, 10 µF/25 V statt 4,7 µF/16 V und 10 µF/10 V, Gate-Widerstand 1 k statt 100 Ω, I²C-Pull-ups 5,1 k statt 4,7 k | bei Standard-Bestückung kostet jede Teilenummer Rüstgebühr; nirgends weniger Spannungsfestigkeit oder Kapazität |
| Akku-Spannungsteiler | – | 100 k / 10 k statt 10 k / 1 k | gleiches Verhältnis 11:1; der obere 0402-Widerstand nimmt bei 6S nur noch 5 mW statt 52 mW (84 % der Nennleistung) auf |
| Piepser-FET | 2N7002 oder BSS138 | **AO3400A** | Schwellspannung max. 1,45 V: an 3,3 V sicher durchgeschaltet (2N7002 bis 2,5 V) |
| Akku+ Leiterbreite | ≥ 3 mm bzw. Fläche | Fläche auf Lage 3 (Südstreifen 2,2 mm, an einem GND-Via 1,55 mm; Weststreifen ≥ 2,15 mm), Zuleitung Stecker → Verpolschutz 0,6 mm (Padbreite) / 0,8 mm mit zwei Vias, Verpolschutz → Fläche 0,8 mm mit zwei Vias | der FC-Eingangsstrom ist durch den JST-SH-Kontakt (1 A) begrenzt; bei 2S höchstens ≈ 1,2 A an 5 V entnehmen (siehe unten) |
| ESC-Stecker | VBAT, GND, CURR, 5V, M1–M4 | **Reihenfolge des B-Cube 4in1**: 1 GND · 2 VBAT · 3–6 M1–M4 · 7 CURR · 8 NC; kein 5-V-Pin, **JP2 entfällt** | gemessen am ESC; 1:1-Kabel statt Umpinnen |
| USB-Versorgung | VBUS über SS14 auf +5V | **getrennte 4,5-V-Schiene** (wie Matek): VBUS → D2 → +4V5, +5V → D7 → +4V5; an +4V5 hängen AMS1117 (3,3 V), ELRS und GPS | an USB laufen VTX, Kamera und Piepser nicht mit (D7 sperrt); der USB-Anschluss wird nicht rückgespeist (D2 sperrt) |
| Eingangs-Elko C21 | 470 µF/35 V | **47 µF/35 V**, 6,3 × 5,4 mm, 850 mΩ | kleinerer Einschaltstrom beim Anstecken; die großen Elkos sitzen am ESC |
| Piepser | Treiber | zusätzlich 2-polige Buchse J11 (+5 V / BZ−) | in der Spezifikation fehlt ein Anschluss für den Summer |
| Kamera-Video | Pin am CAM-Stecker | zusätzlich Lötpad **TP5 VIDEO** | das Videosignal hat in der Spezifikation kein Ziel (kein OSD, VTX-Stecker ohne Video) |
| VTX-Versorgung | 5 V oder 9 V | nur 5 V | kein 9-V-Regler vorgesehen |
| PB2/BOOT1 | – | 10 k nach GND | sonst startet DFU (BOOT0 gebrückt) je nach Zufall aus dem SRAM |
| 5-V-Leitungen | 1 mm | Fläche auf Lage 3, Ausgang Spule → C26 **0,8 mm**, Stichleitungen Pad → Via **0,4 mm** | die Pads der JST-SH-Buchsen sind nur 0,6 mm breit; Ströme der Abgänge < 1 A |
| 5-V-Rückführung | – | R10/R11 = **33 k / 8,2 k** (5,02 V) statt 100 k / 24,9 k | 24,9 k gibt es nur als Extended-Teil; 33 k (Basic) und 8,2 k (Preferred) ergeben dieselbe Spannung |
| LED grün | – | Vorwiderstand R16 **470 Ω** statt 1 kΩ | die gewählte LED (C72043) ist InGaN (Uf ≈ 2,7 V); mit 1 kΩ an 3,3 V wäre sie zu dunkel. Strom ≈ 1,3 mA (PC15 darf max. 3 mA) |
| Quarz | – | 8 MHz, **CL 12 pF** (YXC, C20617566) mit 2 × 18 pF | passend zu den vorhandenen 18-pF-Lastkondensatoren |
| GPIO4 | – | **PC5** (Pin 25) statt PA1 | das PA1-Pad liegt zwischen BUZZER und DBG_TX eingeklemmt und ließ sich nicht entflechten |
| Kupfer außen | 70 µm (2 oz) | **35 µm (1 oz) bestellen** | JLCPCB verlangt bei 2 oz außen mind. 0,2 mm Breite/Abstand – der Gyro-Footprint hat 0,15 mm Padabstand. Die Motorströme fließen ohnehin nicht über den FC (nur ~1 A über das JST-SH-Kabel) |
| Kupfer innen | 35 µm (1 oz) | **17,5 µm (0,5 oz) bestellen** (Standardaufbau JLC04161H-7628); 1 oz bleibt möglich (JLC041611-7628) | 1 oz innen kostet rund 17 USD Aufpreis. Lage 2 ist reine GND-Fläche. Auf Lage 3 sind die engsten VBAT-Streifen ≥ 2,15 mm breit (an einem Via 1,55 mm, kurz); bei 4S–6S fließt dort < 1 A, nach IPC-2221 (Innenlage, konservativ) gibt das < 10 K Erwärmung. Nur bei 2S mit voller 5-V-Last (≈ 1,5 A) wären es etwa 15 K – dann lieber 1 oz |
| Massebohrung | eine | H2 (Südost) | direkt am ESC-GND; die Südwest-Ecke hätte mit ihrem Massering die 5-V-Fläche auf Lage 3 zerteilt |

## Fertigung (JLCPCB)

`fertigung/fc_jlcpcb_gerber.zip` hochladen. Bestellparameter:

| Parameter | Wert |
|---|---|
| Lagen | 4 |
| Abmessung | 42 × 41 mm |
| Dicke | 1,6 mm |
| Lagenaufbau | JLC04161H-7628 (Standard; das „H“ steht für 0,5 oz innen) |
| Kupfer außen / innen | 1 oz / **0,5 oz** (Standard). 1 oz innen wie in der Spezifikation geht auch: Aufbau dann **JLC041611-7628**, im Angebot von Sept. 2026 rund 17 USD Aufpreis (siehe Abweichungen) |
| Min. Leiterbahn / Abstand | 0,15 / 0,15 mm (eingehalten) |
| Min. Via | 0,3 mm Bohrung / 0,6 mm Pad |
| Oberfläche | ENIG empfohlen (0,5-mm-Raster, LGA-Gyro) |
| Via-Abdeckung | Tented |

Die Bohrdaten liegen getrennt als `fc-PTH.drl` und `fc-NPTH.drl` bei (Excellon, mm). Gerber,
Bohrdaten und Bestückkoordinaten beziehen sich alle auf den Bohr-/Bestückungsursprung in der
**linken unteren Platinenecke**.

### Bestückung bei JLCPCB (beide Seiten)

| Datei | Zweck |
|---|---|
| `fertigung/jlcpcb_bom.csv` | Stückliste aller bestückten Teile (oben 74, unten 8) im JLCPCB-Format mit **LCSC-Nummern**, 39 Teilenummern |
| `fertigung/jlcpcb_cpl.csv` | Bestückkoordinaten beider Seiten (Spalte Layer Top/Bottom), Drehung bereits für JLCPCB umgerechnet |
| `fertigung/jlcpcb_pruefliste.csv` | je Bauteil: Seite, LCSC-Typ, Bestand mit Datum/Quelle, Drehung KiCad → CPL mit Quelle der Korrektur, **Lage von Pin 1 bzw. Kathode/Pluspol** |

Nicht bestückt werden die Testpunkte TP1–TP5, die Lötbrücke JP1, Bohrungen und Fiducials.

**Bestellart: Standard PCBA, beidseitig.** Economic PCBA bestückt nur eine Seite. Folgen:
- Einrichtgebühr für zwei Seiten (JLCPCB-Preisliste, Stand 09/2026: 25,56 USD je Seite, also 51,12 USD)
  und zwei Schablonen (je 8,21 USD; die Schablonen bleiben bei JLCPCB).
- Rüstgebühr (Feeder Loading Fee) für **jede** Teilenummer, auch für Basic-Teile (laut JLCPCB-Preisliste
  ca. 1,53 USD je Teilenummer, bei 39 Teilenummern ≈ 60 USD). Deshalb sind gleichwertige Werte
  zusammengelegt (siehe Abweichungen).
- Standard PCBA verlangt **Nutzentrennränder (Edge Rails) und Fiducials** und eine
  Mindestgröße von 70 × 70 mm. Die Platine (42 × 41 mm) ist kleiner und hat Bauteile direkt an
  der Kante: beim Bestellen „Edge Rails/Fiducials: Added by JLCPCB“ wählen bzw. JLCPCB den
  Nutzen machen lassen (bei PCBA mit gefrästem Spalt und Stegen, kein V-Cut). JLCPCB vergrößert
  die Platine dafür auf etwa 70 × 71 mm und rechnet den Platinenpreis mit dieser Größe.
  Bemerkungen, getrennt nach Abschnitt, damit Platinenfertigung und Bestückung je ihren Hinweis
  bekommen:
  - **PCB Remark:** „USB-C connector overhangs the east edge by 0.5 mm, please keep a routed gap
    there.“
  - **Assembly remark:** „C21 is polarized: + (pad 1) must be on the west side (left, USB-C
    right). D7 on bottom side: cathode band toward the rear edge.“

  Die Ränder lässt man am besten von JLCPCB abtrennen („Depanel boards & edge rail before
  delivery“, im Sept. 2026 rund 2,30 USD bei 2 Platinen): Kondensatoren, USB-C und JST-Buchsen
  sitzen direkt an der Kante, Abbrechen von Hand belastet sie. Ohne diese Option kommen die
  Ränder mit und werden abgebrochen; die Stegreste an der Kante mit einer Feile glätten.

**Extended-Teile, 18 Teilenummern** (bei Standard-Bestückung kostet ohnehin jede Teilenummer
gleich viel; die Einteilung zählt nur für die Verfügbarkeit): STM32F405RGT6 (C15742), ICM-42688-P (C1850418),
LMR33630BDDAR (C544370), USBLC6-2SC6 (C7519), Si2319CDS (C146287), USB-C TYPE-C-31-M-12
(C165948), Spule XGL4020-332MEC (C6937839), Elko 47 µF/35 V (C48970990), die fünf JST-SH-
Buchsen ZX-SH1.0-2/3/4/5/8PWT (C7430444, C7430445, C7430446, C7430447, C7430450) – diese gibt
es gar nicht als Basic/Preferred. Nur wegen des Footprints Extended sind Quarz 8 MHz 3225
(C20617566), Ferrit 0402 (C7509833) und 10 µF/50 V 1210 (C17498688). Die gelbe und die grüne
LED (C72038, C72043) stehen seit 03/2025 nicht mehr auf der Basic/Preferred-Liste; aktuell sind
dort nur rote und weiße 0603-LEDs. Ihr Bestand ist unbekannt – beim BOM-Upload prüfen; fehlt
eine, ersatzweise die rote C2286 nehmen (keine neue Teilenummer). Ersetzt sie die grüne, R16
von 470 Ω auf 1 kΩ (C11702) ändern, sonst fließen an PC15 etwa 2,8 mA statt 1,3 mA. Für Quarz, Ferrit und C22 stehen
Alternativen mit Footprint-Änderung in `scripts/lcsc.py` (`ALTERNATIVES`). Die Auswahl steht in
`scripts/lcsc.py` (Stand 13.10.2026, mit Quellen).

**Lagerbestand:** lcsc.com und jlcpcb.com waren aus der Arbeitsumgebung gesperrt. Bestände
stammen aus dem JLCPCB-Katalogabzug von yaqwsx/jlcparts (mit Abrufdatum) bzw. für ältere
Nummern nur aus Websuche-Momentaufnahmen (in der Prüfliste als „unbestätigt“ markiert).
Kein gewähltes Teil war dort als nicht lagernd erkennbar – beim Hochladen der BOM zeigt
JLCPCB den aktuellen Bestand, das ist die maßgebliche Prüfung.

**Drehlagen:** Die CPL-Drehung bezieht sich bei JLCPCB auf die Lage des Footprints in der
JLC/EasyEDA-Bibliothek. Die Korrekturen sind deshalb durch Vergleich der Pad-Lagen der
KiCad-Footprints mit den EasyEDA-Footprints bestimmt (CDFER/JLCPCB-Kicad-Library und mit
easyeda2kicad umgewandelte Footprints der bestellten LCSC-Teile aus öffentlichen Projekten; der
Zusatz BL/BR/TL im Namen nennt die Ecke von Pin 1): LQFP und SOIC +270°, SOT-23-6 +270°,
**SOT-23 (3 Pins) +180°**, SOT-223 +180°; **0°** für LGA-14, SOD-323, SMA, LED 0603, Quarz,
**JST-SH (ZX-SH1.0, C7430446)** und **USB-C (C165948)**. Die verbreiteten Tabellen von
JLCKicadTools und KiBot nennen für JST und USB-C 180° – das passt nicht zu den heutigen
JLC-Footprints dieser Teile. Für den Elko C21 (C48970990) lag kein JLC-Footprint vor; die 0°
sind vom EasyEDA-Elko CAP-SMD_BD8.0 (C2983323) übernommen, die Tabellen nennen dort 180° –
deshalb unten ausdrücklich prüfen. Eine Simulation (EasyEDA-Footprint mit CPL-Drehung auf die
KiCad-Pads gelegt) passt für alle vergleichbaren Teile ohne Zusatzdrehung. Unterseite: KiCad
spiegelt Bauteile beim Umdrehen, JLCPCB nicht – die Drehung wird wie bei kicad-jlcpcb-tools
umgerechnet (180° − KiCad-Winkel, dann Korrektur). Die Koordinaten sind die Mitte der Lötpads.

In der JLCPCB-Vorschau **besonders prüfen**:
- **C21** Elko: Für dieses Teil lag kein JLC-Footprint vor; angenommen ist dieselbe Lage wie
  KiCad (Pluspol links). **Der Pluspol muss links liegen** (von oben, USB-C rechts) – verpolt
  an 25 V wäre der Elko zerstört.
- **U4** LMR33630 (HSOP-8): +270° angenommen (wie die JLC-Footprints ESOP-8 „BL“);
  Pin 1 muss rechts oben liegen, das Wärmepad auf dem großen Mittelpad.
- **J1** USB-C: Die Steckeröffnung muss über die Ostkante zeigen.
- **J2–J11** JST-SH: Öffnung jeder Buchse zur Platinenkante. Das Landemuster der
  Megastar-Buchse liegt 0,3 mm anders als das JST-Original; die Pads überlappen trotzdem.
- **D7** (Unterseite, neu): Kathodenstrich von unten gesehen (USB-C links) **unten**, also zur
  Hinterkante. Verdreht bekäme die 4,5-V-Schiene mit Akku keine Spannung (Prozessor aus), und
  an USB liefen VTX, Kamera und Piepser mit.

Außerdem für jedes IC, jede Diode und den Elko kontrollieren, dass Pin-1-Punkt bzw.
Kathodenstrich/Pluspol auf der Seite liegt, die in der Prüfliste steht (Oberseite von oben
mit USB-C rechts, Unterseite von unten mit USB-C links), z. B. U1 Pin 1 links oben, U2 Pin 1
rechts unten, U3/U4 Pin 1 rechts oben, U5 Pin 1 rechts unten, U6/Q1/Q2 Pin 1 links oben,
Y1 Pin 1 rechts oben, D1 Kathode rechts, D2–D6 Kathode unten, D7 (Unterseite) Kathode unten,
C21 Pluspol links.

### Bestellablauf (jlcpcb.com)

1. `fertigung/fc_jlcpcb_gerber.zip` unter jlcpcb.com/quote hochladen. JLCPCB muss „4 layer
   board of 41x42mm“ erkennen. Felder der Platine (Stand Okt. 2026, nicht genannte Felder auf
   dem Standardwert bzw. No):

   | Feld | Wert |
   |---|---|
   | Base Material / Layers / High Precision PCB | FR-4 / 4 / No |
   | PCB Qty, Product Type, Different Design | 5, Industrial/Consumer electronics, 1 |
   | Delivery Format | Single PCB (die Ränder kommen über die Bestückung, siehe oben) |
   | PCB Thickness / Color / Silkscreen | 1.6 / Green / White |
   | Surface Finish / Gold Thickness | ENIG / 1 U″ |
   | Outer / Inner Copper Weight | 1 oz / 0.5 oz |
   | Specify Layer Sequence / Specify Stackup / Layer Stackup / Impedance Control | No / Yes / JLC04161H-7628 / No |
   | Via Covering / Via Plating Method | Tented / Not Specified |
   | Min via hole size/diameter | 0.3mm/(0.4/0.45mm) (alle 205 Vias 0,3/0,6 mm) |
   | Board Outline Tolerance / Electrical Test | ±0.2mm (Regular) / Flying Probe Fully Test |
   | Confirm Production file | Yes |
   | Mark on PCB | Remove Mark (bis 6 Lagen kostenlos), sonst Standardwert |
   | Gold Fingers, Castellated Holes, Press-Fit Hole, Edge Plating, Blind Slots, UL Marking, Backdrill, Humidity Indicator Card | No |
   | Advanced Options (4-Wire Kelvin, Paper between PCBs, Appearance Quality, Silkscreen Technology, Package Box, Inspection Report) | No bzw. Standardwert (IPC Class 2) |
   | PCB Remark | siehe oben |
   | Stencil („Order together with PCB“) | aus – die Schablone ist in der Bestückung enthalten |

   Via Covering: Im Wärmepad von U4 liegen 12 Vias. Bei
   „Tented“ bleiben sie oben offen und ziehen etwas Lot, das ist für Wärmevias üblich. „Plugged“
   geht bei Vias im Pad nicht; „Epoxy Filled & Capped“ wäre sauberer, gilt aber für alle Vias und
   kostet bei 4 Lagen Aufpreis.
2. „PCB Assembly“ einschalten:

   | Feld | Wert |
   |---|---|
   | Assemble top side / bottom side | beide |
   | PCBA Type / Assembly Side | **Standard** / **Both Sides** |
   | PCBA Qty | 2–5 (mindestens 2; die übrigen Platinen kommen unbestückt) |
   | Edge Rails/Fiducials | Added by JLCPCB |
   | Confirm Parts Placement | **Yes** |
   | Stencil Storage / Fixture Storage | No / No |
   | Parts Selection | By Customer (Self-Service) – die BOM hat LCSC-Nummern |
   | Depanel boards & edge rail before delivery | Yes (siehe oben) |
   | Solder Paste | Standardwert (High temp., SAC305) |
   | Bake Components, Photo Confirmation, Board Cleaning, Conformal Coating, Special stencil, Flying Probe Test, Nitrogen reflow, Function test | No |
   | Packaging | Standardwert |
   | Assembly remark | siehe oben |

3. „Next“ (ab hier mit Konto), dann `fertigung/jlcpcb_bom.csv` und `fertigung/jlcpcb_cpl.csv`
   hochladen. Warnungen zu TP1–TP5 und JP1 (nicht bestückt) übergehen. Teilezuordnung und
   Bestand prüfen: alle 39 Positionen müssen ein Teil haben, keine Fehlmenge (gelbe und grüne
   LED besonders ansehen, siehe oben).
4. In der Platzierungsvorschau Ober- und Unterseite mit der Prüfliste abgleichen (Liste oben),
   falsch gedrehte Teile dort korrigieren.
5. „Save to Cart“: Erst hier steht der Gesamtpreis mit Bauteilen. Warenkorb und Entwurf sind
   unverbindlich, bezahlt wird erst an der Kasse.
6. Beim echten Bestellen „Review Before Payment“ wählen, und in den Dialogen zu Confirm
   Production file / Confirm Parts Placement **„Do not confirm automatically“** anhaken – sonst
   startet die Fertigung nach Ablauf der Frist ohne Freigabe. Beides dann in der Bestellhistorie
   selbst prüfen und freigeben.

## Routing

**Von Hand vorverlegt und gesperrt** (`scripts/prerouting.py`, 401 von 1115 Kupferobjekten):
alle Abblockkondensatoren mit kurzem Weg zum Pin und Via direkt daneben (STM32 VDD/VSS,
VCAP1/2, VDDA über FB1, Gyro VDD/VDDIO, Flash), Gyro-Masse ohne Vias unter dem Chip,
Schaltregler-Leistungskreis (C23 an VIN/GND, Schaltknoten 0,8 mm zur Spule, Bootstrap,
VCC, Rückführung, Wärmevias), 3,3-V-Ausgang des AMS1117, Akkueingang ESC → Verpolschutz,
USB D+/D− vom Prozessor zum USBLC6, VBUS (beide Buchsen-Pads, Diode, C33/C34), CC1/CC2,
das SPI-Bündel zwischen Gyro und Prozessor, Via-Fluchten für ELRS und GYRO_CS, der Knoten der
4,5-V-Schiene (Kathoden von D2 oben und D7 unten über zwei Vias) und von dort zum
AMS1117-Eingang mit C29/C30 (unter der USB-C-Buchse nur auf B.Cu, nicht unter dem
Blechgehäuse), das Gate des Verpolschutz-FET ohne Vias (der VBAT-Streifen auf Lage 3 bleibt
ungeteilt) und die Versorgungspins der JST-SH-Buchsen, die außerhalb ihrer Fläche auf Lage 3
liegen (I2C, UART, GPIO, Piepser, SWD). Den Ast AMS1117 → ELRS/GPS legt der Autorouter (0,4 mm).
Danach setzt `fanout.py` jedem übrigen Versorgungspad ein Via in die passende Fläche.

**Autorouter**: alle übrigen Signale (Freerouting, F.Cu und B.Cu; Lage 2 und 3 sind als
Flächen gesperrt). Kennzahlen (`docs/fc_routing_kennzahlen.txt`): 205 Vias, 0 Vias unter dem
Gyro, 0 Leiterbahnen auf Lage 2/3, kleinste Bahnbreite 0,2 mm, USB D+ 8,1 mm / D− 9,2 mm,
Motorleitungen M1–M4 4,7–8,7 mm mit je einem Via, nächster Motorleitungs-Endpunkt 13,9 mm vom
Gyro.

## Designregelprüfung (DRC)

KiCad 7.0.11, DRC über die Python-Schnittstelle (`scripts/run_drc.py`, gleiche Prüfung wie im
Editor, Zonen frisch gefüllt) – Bericht in `docs/fc_drc_bericht.txt`:

| Prüfung | Ergebnis |
|---|---|
| DRC-Verstöße (Abstände, Breiten, Bohrungen, Rand, Courtyards, Sperrflächen, Silk) | **0** |
| nicht verbundene Pads | **0** |
| Footprint-Fehler / Abweichung zur Bibliothek | **0** |
| Schaltplan ↔ `design.py` (`check_netlist.py`) | 291 Knoten, 0 Fehler |
| Platine ↔ Schaltplan-Netzliste (`report.py`) | 319 Pads, 0 Abweichungen |

**ERC** (KiCad 9.0.3, `kicad-cli sch erc --severity-all`, Bericht `docs/fc_erc_bericht.txt`):
**0 Fehler, 2 Warnungen**. Beide melden, dass beim Flash U3 die Pins IO2 (/WP) und IO3
(/HOLD) direkt an +3,3 V (Ausgang des AMS1117) liegen („bidirektional an Spannungsausgang“).
Das ist beabsichtigt: im normalen SPI-Betrieb müssen /WP und /HOLD dauerhaft high sein.

## Unsichere Stellen im Routing (bitte vor der Bestellung ansehen)

1. **Unter der USB-C-Buchse** liegen Kupfer und Vias: je ein VBUS-Via zwischen Pad-Ende und
   Führungsstift (Verbindung der beiden VBUS-Pads über B.Cu), die D+-Brücke zwischen den
   Pads A6/B6 auf F.Cu östlich der Pad-Enden und ein +5-V-Via für den Testpunkt TP2. Alles ist
   lackiert bzw. tented, das Blechgehäuse liegt aber direkt darauf (Kurzschlussgefahr bei
   beschädigtem Lötstopplack). Die 4,5-V-Schiene läuft unter der Buchse nur auf der Unterseite
   (B.Cu), nicht unter dem Gehäuse.
2. **CC1** läuft auf B.Cu in einem Bogen östlich um die USB-Bauteile (12 mm, zwei Vias) –
   elektrisch unkritisch, aber ein Umweg.
3. **30 Vias unter dem Prozessorgehäuse** (Versorgung und Signal-Fluchten, u. a. SPI-SCK,
   GYRO_CS, ELRS). Tented, Standard bei LQFP, aber nicht unter jedem Fertiger erwünscht.
4. **SPI-SCK** wechselt direkt unterhalb der Via-Sperrfläche des Gyros auf B.Cu (0,05 mm
   Reserve zur Sperrfläche) und kommt innerhalb des Prozessor-Pinrings wieder hoch; SCK,
   MOSI und MISO sind 19–24 mm lang (Flash liegt links), bei 24 MHz SPI unkritisch.
5. **Signale auf B.Cu über geteilter Lage 3**: B.Cu-Bahnen haben Lage 3 (VBAT/+5V/+3V3,
   getrennt) als nächste Ebene. Einige Autorouter-Bahnen kreuzen die Trennfugen; für
   UART/I²C/GPIO unkritisch, die schnellen Signale (USB, SPI zum Gyro) liegen auf F.Cu über
   der durchgehenden Massefläche (Ausnahme: das 3,9-mm-SCK-Stück). Die Abzweige von SCK, MISO
   und MOSI zum Flash liegen großteils auf B.Cu, aber innerhalb der +3V3-Fläche ohne Fugenkreuzung.
6. **Schaltregler**: Leistungskreis von Hand (C23 100 nF und C22 10 µF direkt an VIN/PGND,
   C22 mit der Masse auf F.Cu am PGND-Pin, VBAT über je zwei Vias), die Wärme des LMR33630
   geht über 12 Vias im Exposed Pad in die GND-Fläche. Die Vias sind ungefüllt; etwas Lot kann
   hineinlaufen. Bei 2 A Dauerlast an 6S Temperatur messen.
7. **VBAT-Ring auf Lage 3**: Südstreifen 2,2 mm (an einem GND-Via 1,55 mm), Weststreifen
   ≥ 2,15 mm (gemessen an der gefüllten Fläche). Zuleitungen: Stecker-Pad → Vias 0,6 mm,
   dann 0,8 mm zum Verpolschutz; Verpolschutz → Fläche 0,8 mm mit zwei Vias; Fläche → Regler
   U4 0,4 mm mit zwei Vias, → C22 0,6 mm mit zwei Vias. Für den Strom des FC (≤ 1,5 A bei 2S)
   reicht das; Motorstrom fließt nicht über den FC.
8. **USB D+/D−** ohne definierte Impedanz (Full-Speed 12 MBit/s, 10 mm – unkritisch).
9. **Autorouter-Bahnen** sind funktional, aber nicht schön: viele Vias und einige Umwege
   (Leiterbahnlänge F.Cu 694 mm, B.Cu 716 mm). Wer nachbessern will, kann im KiCad-Editor
   einzelne Netze neu verlegen; die gesperrte Handverdrahtung bleibt dabei liegen.
10. **ESC-Belegung (J2)**: J2 folgt der gemessenen Reihenfolge des B-Cube 4in1 und setzt ein
    **1:1-Kabel** voraus. Das Kabel ist noch nicht geprüft – **vorher nicht bestellen**, und vor
    dem ersten Akku die „Prüfung vor dem ersten Akku“ im Abschnitt „ESC-Stecker J2“ machen
    (bei Abweichung keinen Akku anstecken, sondern melden).
11. **Buchsen nahe den Bohrungen**: Zwischen den Randbuchsen und den Montagebohrungen bleiben
    ≥ 3,35 mm (Courtyard) – bei dicken Gummidämpfern oder Muttern prüfen, ob der Stecker frei
    bleibt.
12. **Strom über den ESC-Stecker**: Akku+ und GND laufen über je einen JST-SH-Kontakt
    (Nennstrom 1 A). Bei 4S–6S und üblicher Last passt das; bei 2S/3S die 5-V-Entnahme auf
    etwa 1,2 A (2S) bzw. 1,7 A (3S) begrenzen.
13. **4,5-V-Schiene als Leiterbahn**: +4V5 ist 87 mm lang (0,4 mm, 7 Vias). Sie läuft vom
    Diodenknoten D2/D7 (Südost) auf B.Cu unter der USB-C-Buchse nach Norden, zwischen den
    SWD-Pads unter dem AMS1117 hindurch zu dessen Eingang (Nordost) und von dort auf B.Cu quer
    über die Platine zu GPS und ELRS an der Westkante. Für ≤ 0,5 A reicht das (Spannungsabfall
    < 50 mV).
14. **Strommessung CURR_SENSE**: Die Leitung von R6/C15 zu PA4 macht einen Umweg (24 mm,
    zwei Vias). Unkritisch, weil C15 (100 nF) den Knoten niederohmig hält. Für ein sauberes
    Messsignal ein Mittelwert in der Firmware.
