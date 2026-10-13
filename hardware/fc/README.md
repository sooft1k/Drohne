# Flight Controller – KiCad-Projekt

Eigene FC-Platine für die 5-Zoll-Drohne nach `fc-spezifikation.md`.
Erstellt mit KiCad 7.0 (öffnet auch in KiCad 8/9; dabei wird einmalig konvertiert).

| Datei | Inhalt |
|---|---|
| `fc.kicad_pro` | Projekt mit Designregeln und Netzklassen |
| `fc.kicad_sch` | Schaltplan (ein A1-Blatt, nach Funktionsgruppen, Netzlabels + Versorgungssymbole) |
| `fc.kicad_pcb` | Platine: 42 × 41 mm, 4 Lagen, platziert und vollständig geroutet |
| `fc_custom.kicad_sym` | Projektsymbole: ICM-42688-P, Versorgungssymbole VBAT und +3V3A |
| `fc_custom.pretty` | LMR33630-Footprint ohne Kupfer auf der Unterseite (dort sitzen Stiftleisten); USB-C-Buchse mit an der Platinenkante gekürztem Bestückungsdruck |
| `fertigung/` | Gerber + Bohrdateien (ZIP für JLCPCB), Stückliste, Bestückkoordinaten |
| `docs/` | Schaltplan als PDF, Lagenbilder (oben, unten gespiegelt, Lage 2, Lage 3), DRC-Bericht, Routing-Kennzahlen |
| `scripts/` | Generatoren – alles wird aus `scripts/design.py` erzeugt |

## Neu erzeugen

```
export KICAD7_FOOTPRINT_DIR=/usr/share/kicad/footprints
cd hardware/fc
python3 scripts/gen_lib.py      # Projektbibliothek
python3 scripts/gen_sch.py      # Schaltplan
kicad-cli sch export netlist -o build/fc.net fc.kicad_sch
python3 scripts/check_netlist.py build/fc.net   # Schaltplan == design.py, Pin für Pin
python3 scripts/gen_pcb.py      # Umriss, Bohrungen, Platzierung, Flächen, Handverdrahtung, Fanout
python3 scripts/route.py        # Autorouting (Freerouting 2.0.1, Java) + Nachrouter, Zonen füllen
python3 scripts/run_drc.py fc.kicad_pcb build/drc.rpt
python3 scripts/report.py fc.kicad_pcb build/fc.net   # Kennzahlen + Abgleich Platine/Schaltplan
python3 scripts/sync_fields.py  # nur Werte/LCSC-Nummern in die geroutete Platine uebernehmen
python3 scripts/fab.py          # Gerber, Bohrdaten, ZIP, JLCPCB-BOM/CPL, Pruefliste, Handliste
```

ERC mit KiCad 9 (kicad-cli 7 kann keine ERC):
`kicad-cli sch erc --format report --severity-all -o erc.rpt fc.kicad_sch`

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
| 3 In2.Cu | Versorgung: +3V3 (Mitte unter MCU/Gyro/Flash), +5V (Ost, Süd, West), VBAT (Ring am Rand: Nordband zum Regler, schmale Streifen West/Süd zum Verpolschutz) |
| 4 B.Cu | Steckverbinder (SMD-Stiftleisten, JST-SH), Signale, Restfläche GND |

Keine Leiterbahn auf Lage 2 und 3 – dort liegen nur Flächen. Montagebohrungen M3
(Ø 3,2 mm) auf 30,5 × 30,5 mm; nur **H2 (Südost, neben dem ESC-Stecker)** ist mit GND
verbunden, die anderen drei sind nicht durchkontaktiert und haben 3,3 mm Kupfer-Sperrkreis
für die Abstandshalter.

## Pinbelegung STM32F405RGT6 (für die Firmware)

| Pin | Port | Netz | Funktion |
|---|---|---|---|
| 26 | PB0 | M1 | TIM3_CH3 |
| 27 | PB1 | M2 | TIM3_CH4 |
| 22 | PA6 | M3 | TIM3_CH1 |
| 23 | PA7 | M4 | TIM3_CH2 – alle vier Motoren auf TIM3 (wie bisher), DShot über DMA1 möglich |
| 8 | PC0 | VBAT_SENSE | ADC1_IN10, Teiler 10 k / 1 k (11:1) |
| 20 | PA4 | CURR_SENSE | ADC12_IN4, Stromsignal vom ESC über 1 k / 100 nF |
| 55 | PB3 | SPI1_SCK | Gyro + Flash |
| 56 | PB4 | SPI1_MISO | Gyro + Flash |
| 57 | PB5 | SPI1_MOSI | Gyro + Flash |
| 58 | PB6 | GYRO_CS | 10 k Pull-up |
| 52 | PC11 | GYRO_INT1 | EXTI11 (Data-Ready) |
| 59 | PB7 | GYRO_INT2 | Gyro-Pin 9 (INT2/FSYNC/CLKIN), TIM4_CH2 → optional 32-kHz-CLKIN |
| 51 | PC10 | FLASH_CS | 10 k Pull-up |
| 42 | PA9 | ELRS_TX | USART1_TX (wie bisher) |
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
| 2 | PC13 | LED_ARM | LED rot, **aktiv-low** (Pin = 0 → LED an) |
| 3 | PC14 | LED_ERR | LED gelb, aktiv-low |
| 4 | PC15 | LED_LINK | LED grün, aktiv-low |
| 14 | PA0 | BUZZER | TIM2_CH1/TIM5_CH1, high = Piepser an (N-FET) |
| 9 | PC1 | GPIO1 | Stiftleiste GPIO Pin 1 |
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

Gyro-Einbaulage: Der ICM-42688-P sitzt nahe der Platinenmitte und ist um **180° gedreht**
(von oben gesehen, ESC-Stecker unten, USB-C rechts: Pin-1-Markierung unten rechts). Die
Sensorachsen X und Y zeigen damit entgegen der Gehäuse-Normallage – in der Firmware
„Ausrichtung CW180°" einstellen bzw. X und Y invertieren.

## Steckverbinder (alle auf der Unterseite)

| Ref | Name | Pin 1 → n |
|---|---|---|
| J2 | ESC, JST-SH 8 | 1 VBAT · 2 GND · 3 CURR · 4 5V (über JP2, offen) · 5 M1 · 6 M2 · 7 M3 · 8 M4 |
| J3 | ELRS | 1 TX · 2 RX · 3 5V · 4 GND |
| J4 | I2C | 1 SDA · 2 SCL · 3 3V3 · 4 GND |
| J5 | GPS | 1 TX · 2 RX · 3 5V · 4 GND |
| J6 | VTX | 1 TX · 2 RX · 3 5V · 4 GND |
| J7 | CAM | 1 5V · 2 VIDEO · 3 GND (VIDEO zusätzlich auf Lötpad TP5) |
| J8 | SWD | 1 SWDIO · 2 SWCLK · 3 NRST · 4 3V3 · 5 GND |
| J9 | UART (Debug) | 1 TX · 2 RX · 3 3V3 · 4 GND |
| J10 | GPIO | 1–6 GPIO1–6 · 7 3V3 · 8 GND |
| J11 | Piepser | 1 +5V · 2 BZ− (Drain) |
| J1 | USB-C | Oberseite |

TX/RX jeweils aus Sicht des FC (FC-TX an Empfänger-RX anschließen).

**J2 vor dem ersten Anstecken prüfen.** Die Spezifikation nennt nur die Signale des
ESC-Steckers, nicht ihre Reihenfolge; die Belegung oben ist gewählt, nicht vom ESC
übernommen. Für den UAngel 4in1 60A BLHeli_S war keine Pinbelegung auffindbar (Handbuch
verweist nur allgemein auf das FC-Handbuch), und die Reihenfolge der 8-poligen Stecker ist
herstellerabhängig. Vorgehen: Beschriftung am ESC-Stecker ablesen bzw. mit dem Multimeter
Durchgang messen (Akku+ und Akku− gegen die großen Lötpads, M1–M4 gegen die Signalpads),
dann mit der Tabelle vergleichen. Kritisch sind vor allem Pin 1/2 (Akku+/GND): vertauscht
liegt die Akkuspannung an der Masse des FC. Weicht die Reihenfolge ab, entweder das Kabel
umpinnen (JST-SH-Kontakte lassen sich aus dem Gehäuse ziehen) oder J2 im Projekt anpassen.
BLHeli_S liefert keine Telemetrie; ein TLM- oder 5-V-Pin des ESC landet an Pin 4, der über
die offene Lötbrücke JP2 nicht angeschlossen ist.

## Footprints

| Referenzen | Footprint | Bauteil |
|---|---|---|
| C1, C2, C3, C4, C5, C7, C8, C9, C10, C11, C12, C13, C14, C15, C16, C17, C18, C20, C23, C24, C25, C28, C30, C32, C34 | `Capacitor_SMD:C_0402_1005Metric` | 100nF, 100nF/50V, 18pF, 1uF, 2.2uF |
| C6, C29, C33 | `Capacitor_SMD:C_0603_1608Metric` | 10uF, 4.7uF |
| C21 | `Capacitor_SMD:CP_Elec_10x10.5` | 470uF/35V |
| C22 | `Capacitor_SMD:C_1210_3225Metric` | 10uF/50V |
| C26, C27, C31 | `Capacitor_SMD:C_0805_2012Metric` | 22uF, 22uF/16V |
| D1, D6 | `Diode_SMD:D_SOD-323` | 1N4148WS, BZT52C15S |
| D2 | `Diode_SMD:D_SMA` | SS14 |
| D3, D4, D5 | `LED_SMD:LED_0603_1608Metric` | gelb, gruen, rot |
| FB1 | `Inductor_SMD:L_0402_1005Metric` | 600R@100MHz |
| FID1, FID2, FID3 | `Fiducial:Fiducial_1mm_Mask2mm` | Fiducial |
| H1, H3, H4 | `MountingHole:MountingHole_3.2mm_M3` | M3 |
| H2 | `MountingHole:MountingHole_3.2mm_M3_Pad` | M3 GND |
| J1 | `fc_custom:USB_C_Receptacle_HRO_TYPE-C-31-M-12_Kante` (KiCad-Footprint, Bestückungsdruck an der Kante gekürzt) | USB-C |
| J2 | `Connector_JST:JST_SH_SM08B-SRSS-TB_1x08-1MP_P1.00mm_Horizontal` | ESC |
| J3, J4, J5, J6, J9 | `Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical_SMD_Pin1Left` | ELRS, GPS, I2C, UART, VTX |
| J7 | `Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical_SMD_Pin1Left` | CAM |
| J8 | `Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical_SMD_Pin1Left` | SWD |
| J10 | `Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical_SMD_Pin1Left` | GPIO |
| J11 | `Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical_SMD_Pin1Left` | BUZZER |
| JP1, JP2 | `Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm` | BOOT0, ESC_5V |
| L1 | `Inductor_SMD:L_Coilcraft_XAL4040-XXX` | 3.3uH |
| Q1, Q2 | `Package_TO_SOT_SMD:SOT-23` | 2N7002, Si2319CDS |
| R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16, R17, R18, R19, R20 | `Resistor_SMD:R_0402_1005Metric` | 0R, 100R, 470R, 1k, 4.7k, 5.1k, 8.2k, 10k, 33k, 100k |
| TP1, TP2, TP3, TP4, TP5 | `TestPoint:TestPoint_Pad_D1.5mm` | 3V3, 5V, GND, VBAT, VIDEO |
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
| Status-LEDs | an GPIOs | **PC13–PC15, aktiv-low** | einzige freie Pins auf der Seite der LEDs; PC13–PC15 dürfen nur Strom ziehen (wie die bisherige PC13-LED) |
| Stiftleisten | Stiftleiste, Unterseite | **SMD-Stiftleisten 2,54 mm** auf der Unterseite | THT-Leisten hätten auch die Oberseite blockiert; dafür mechanisch weniger robust – Kabel nicht am Draht ziehen |
| ESC-Pin „5 V" | 5 V | über **Lötbrücke JP2 (offen)** | verhindert, dass ein BEC im ESC gegen den FC-Regler arbeitet; nur schließen, wenn der ESC 5 V vom FC braucht |
| Piepser | Treiber | zusätzlich 2-polige Leiste J11 (+5 V / BZ−) | in der Spezifikation fehlt ein Anschluss für den Summer |
| Kamera-Video | Pin am CAM-Stecker | zusätzlich Lötpad **TP5 VIDEO** | das Videosignal hat in der Spezifikation kein Ziel (kein OSD, VTX-Stecker ohne Video) |
| VTX-Versorgung | 5 V oder 9 V | nur 5 V | kein 9-V-Regler vorgesehen |
| PB2/BOOT1 | – | 10 k nach GND | sonst startet DFU (BOOT0 gebrückt) je nach Zufall aus dem SRAM |
| 5-V-Leitungen | 1 mm | Fläche auf Lage 3, Ausgang Spule → C26 **0,8 mm**, Stichleitungen Pad → Via **0,4 mm** (≤ 2,4 mm lang) | mit 1 mm ließen sich die Abgänge zwischen den Stiftleisten nicht routen; Ströme der Abgänge < 1 A |
| 5-V-Rückführung | – | R10/R11 = **33 k / 8,2 k** (5,02 V) statt 100 k / 24,9 k | 24,9 k gibt es nur als Extended-Teil; 33 k (Basic) und 8,2 k (Preferred) ergeben dieselbe Spannung |
| LED grün | – | Vorwiderstand R16 **470 Ω** statt 1 kΩ | die gewählte Preferred-LED ist InGaN (Uf ≈ 2,7 V); mit 1 kΩ an 3,3 V wäre sie zu dunkel. Strom ≈ 1,3 mA (PC15 darf max. 3 mA) |
| Quarz | – | 8 MHz, **CL 12 pF** (YXC, C20617566) mit 2 × 18 pF | passend zu den vorhandenen 18-pF-Lastkondensatoren |
| GPIO4 | – | **PC5** (Pin 25) statt PA1 | das PA1-Pad liegt zwischen BUZZER und DBG_TX eingeklemmt und ließ sich nicht entflechten |
| Kupfer außen | 70 µm (2 oz) | **35 µm (1 oz) bestellen** | JLCPCB verlangt bei 2 oz außen mind. 0,2 mm Breite/Abstand – der Gyro-Footprint hat 0,15 mm Padabstand. Die Motorströme fließen ohnehin nicht über den FC (nur ~1 A über das JST-SH-Kabel) |
| Massebohrung | eine | H2 (Südost) | direkt am ESC-GND; die Südwest-Ecke hätte mit ihrem Massering die 5-V-Fläche auf Lage 3 zerteilt |

## Fertigung (JLCPCB)

`fertigung/fc_jlcpcb_gerber.zip` hochladen. Bestellparameter:

| Parameter | Wert |
|---|---|
| Lagen | 4 |
| Abmessung | 42 × 41 mm |
| Dicke | 1,6 mm |
| Lagenaufbau | JLC04161H-7628 (Standard) |
| Kupfer außen / innen | 1 oz / 1 oz (wie Spezifikation innen; 0,5 oz innen würde auch reichen) |
| Min. Leiterbahn / Abstand | 0,15 / 0,15 mm (eingehalten) |
| Min. Via | 0,3 mm Bohrung / 0,6 mm Pad |
| Oberfläche | ENIG empfohlen (0,5-mm-Raster, LGA-Gyro) |
| Via-Abdeckung | Tented |

Die Bohrdaten liegen getrennt als `fc-PTH.drl` und `fc-NPTH.drl` bei (Excellon, mm). Gerber,
Bohrdaten und Bestückkoordinaten beziehen sich alle auf den Bohr-/Bestückungsursprung in der
**linken unteren Platinenecke**.

### Bestückung bei JLCPCB (nur Oberseite)

| Datei | Zweck |
|---|---|
| `fertigung/jlcpcb_bom.csv` | Stückliste Oberseite im JLCPCB-Format mit **LCSC-Nummern** (71 Bauteile, 38 Teilenummern) |
| `fertigung/jlcpcb_cpl.csv` | Bestückkoordinaten Oberseite, Drehung bereits für JLCPCB korrigiert |
| `fertigung/jlcpcb_pruefliste.csv` | je Bauteil: LCSC-Typ, Bestand mit Datum/Quelle, Drehung KiCad → CPL, **Lage von Pin 1 bzw. Kathode/Pluspol** auf der Platine |
| `fertigung/handbestueckung_unten.csv` | Steckverbinder der Unterseite zum Selbstlöten (J2–J11) mit Bauteilvorschlägen |

Bestellung als **Economic PCBA, einseitig (Top)**: dann sind die Preferred-Teile wie Basic-Teile
ohne Rüstgebühr. Die Auswahl steht in `scripts/lcsc.py` (Stand 13.10.2026, mit Quellen).

**Extended-Teile (je 3 USD Rüstgebühr), 11 Stück:** STM32F405RGT6 (C15742), ICM-42688-P
(C1850418), LMR33630BDDAR (C544370), USBLC6-2SC6 (C7519), Si2319CDS (C146287), USB-C
TYPE-C-31-M-12 (C165948), Spule XGL4020-332MEC (C6937839), Elko 470 µF/35 V (C48615113) – diese
acht gibt es gar nicht als Basic/Preferred. Nur wegen des Footprints Extended sind Quarz
8 MHz 3225 (C20617566), Ferrit 0402 (C7509833) und 10 µF/50 V 1210 (C17498688); Alternativen
mit Footprint-Änderung stehen in `scripts/lcsc.py` (`ALTERNATIVES`).

**Lagerbestand:** lcsc.com und jlcpcb.com waren aus der Arbeitsumgebung gesperrt. Bestände
stammen aus dem JLCPCB-Katalogabzug von yaqwsx/jlcparts (mit Abrufdatum) bzw. für ältere
Nummern nur aus Websuche-Momentaufnahmen (in der Prüfliste als „unbestätigt“ markiert).
Kein gewähltes Teil war dort als nicht lagernd erkennbar – beim Hochladen der BOM zeigt
JLCPCB den aktuellen Bestand, das ist die maßgebliche Prüfung.

**Drehlagen:** Die CPL enthält die Korrekturen der verbreiteten Tabelle von JLCKicadTools
(LQFP und SOIC +270°, SOT-23/SOT-23-6 −90°, SOT-223 +180°, Elko 10×10 +180°, HRO-USB-C +180°).
Für **U2 (ICM-42688-P, LGA-14)** und **U4 (LMR33630, HSOP-8)** gibt es keinen Tabelleneintrag –
diese beiden in der JLCPCB-Vorschau besonders prüfen. Dort für jedes IC, jede Diode und den
Elko kontrollieren, dass Pin-1-Punkt bzw. Kathodenstrich/Pluspol auf der Seite liegt, die in
der Prüfliste steht (Platine von oben, USB-C rechts, ESC-Stecker unten), z. B. U1 Pin 1
links oben, U2 Pin 1 rechts unten, U3/U4 Pin 1 rechts oben, U5 Pin 1 rechts unten,
U6/Q1/Q2 Pin 1 links oben, Y1 Pin 1 rechts oben, D1 und LEDs Kathode links, D2/D6 Kathode
unten, C21 Pluspol links.

## Routing

**Von Hand vorverlegt und gesperrt** (`scripts/prerouting.py`, 363 von 926 Kupferobjekten):
alle Abblockkondensatoren mit kurzem Weg zum Pin und Via direkt daneben (STM32 VDD/VSS,
VCAP1/2, VDDA über FB1, Gyro VDD/VDDIO, Flash), Gyro-Masse ohne Vias unter dem Chip,
Schaltregler-Leistungskreis (C23 an VIN/GND, Schaltknoten 0,8 mm zur Spule, Bootstrap,
VCC, Rückführung, Wärmevias), 3,3-V-Ausgang des AMS1117, Akkueingang ESC → Verpolschutz,
USB D+/D− vom Prozessor zum USBLC6, VBUS (beide Buchsen-Pads, Diode, C33/C34), CC1/CC2,
das SPI-Bündel zwischen Gyro und Prozessor sowie Via-Fluchten für ELRS und GYRO_CS.
Danach setzt `fanout.py` jedem übrigen Versorgungspad ein Via in die passende Fläche.

**Autorouter**: alle übrigen Signale (Freerouting, F.Cu und B.Cu; Lage 2 und 3 sind als
Flächen gesperrt). Kennzahlen (`docs/fc_routing_kennzahlen.txt`): 180 Vias, 0 Vias unter dem
Gyro, 0 Leiterbahnen auf Lage 2/3, kleinste Bahnbreite 0,2 mm, USB D+ 9,6 mm / D− 9,7 mm,
nächster Motorleitungs-Endpunkt 14,6 mm vom Gyro.

## Designregelprüfung (DRC)

KiCad 7.0.11, DRC über die Python-Schnittstelle (`scripts/run_drc.py`, gleiche Prüfung wie im
Editor, Zonen frisch gefüllt) – Bericht in `docs/fc_drc_bericht.txt`:

| Prüfung | Ergebnis |
|---|---|
| DRC-Verstöße (Abstände, Breiten, Bohrungen, Rand, Courtyards, Sperrflächen, Silk) | **0** |
| nicht verbundene Pads | **0** |
| Footprint-Fehler / Abweichung zur Bibliothek | **0** |
| Schaltplan ↔ `design.py` (`check_netlist.py`) | 292 Knoten, 0 Fehler |
| Platine ↔ Schaltplan-Netzliste (`report.py`) | 310 Pads, 0 Abweichungen |

**ERC** (KiCad 9.0.3, `kicad-cli sch erc --severity-all`, Bericht `docs/fc_erc_bericht.txt`):
**0 Fehler, 2 Warnungen**. Beide melden, dass beim Flash U3 die Pins IO2 (/WP) und IO3
(/HOLD) direkt an +3,3 V (Ausgang des AMS1117) liegen („bidirektional an Spannungsausgang“).
Das ist beabsichtigt: im normalen SPI-Betrieb müssen /WP und /HOLD dauerhaft high sein.

## Unsichere Stellen im Routing (bitte vor der Bestellung ansehen)

1. **Unter der USB-C-Buchse** liegen Kupfer und Vias: je ein VBUS-Via zwischen Pad-Ende und
   Führungsstift (Verbindung der beiden VBUS-Pads über B.Cu), die D+-Brücke zwischen den
   Pads A6/B6 auf F.Cu östlich der Pad-Enden und ein +5-V-Via zum Testpunkt TP2. Alles ist
   lackiert bzw. tented, das Blechgehäuse liegt aber direkt darauf (Kurzschlussgefahr bei
   beschädigtem Lötstopplack).
2. **CC1** läuft auf B.Cu einmal um den VTX-Stecker herum (12 mm, zwei Vias) – elektrisch
   unkritisch, aber ein Umweg.
3. **27 Vias unter dem Prozessorgehäuse** (Versorgung und Signal-Fluchten, u. a. SPI-SCK,
   GYRO_CS, ELRS). Tented, Standard bei LQFP, aber nicht unter jedem Fertiger erwünscht.
4. **SPI-SCK** wechselt direkt unterhalb der Via-Sperrfläche des Gyros auf B.Cu (0,05 mm
   Reserve zur Sperrfläche) und kommt innerhalb des Prozessor-Pinrings wieder hoch; SCK,
   MOSI und MISO sind 20–24 mm lang (Flash liegt links), bei 24 MHz SPI unkritisch.
5. **Signale auf B.Cu über geteilter Lage 3**: B.Cu-Bahnen haben Lage 3 (VBAT/+5V/+3V3,
   getrennt) als nächste Ebene. Einige Autorouter-Bahnen kreuzen die Trennfugen; für
   UART/I²C/GPIO unkritisch, die schnellen Signale (USB, SPI zum Gyro) liegen auf F.Cu über
   der durchgehenden Massefläche (Ausnahme: das kurze SCK-Stück).
6. **Schaltregler**: Leistungskreis von Hand, aber die Wärme des LMR33630 geht nur über
   Vias in die GND-Fläche (Unterseite hat dort Steckverbinder). Bei 2 A Dauerlast an 6S
   Temperatur messen.
7. **VBAT-Ring auf Lage 3**: Der West- und der Südstreifen sind nur ~2 mm breit. Für den
   Strom des FC (< 1,5 A) reicht das; Motorstrom fließt nicht über den FC.
8. **USB D+/D−** ohne definierte Impedanz (Full-Speed 12 MBit/s, 10 mm – unkritisch).
9. **Autorouter-Bahnen** sind funktional, aber nicht schön: viele Vias und einige Umwege
   (Leiterbahnlänge F.Cu 611 mm, B.Cu 654 mm). Wer nachbessern will, kann im KiCad-Editor
   einzelne Netze neu verlegen; die gesperrte Handverdrahtung bleibt dabei liegen.
10. **ESC-Belegung (J2)** und **JST-SH-Ausrichtung** gegen den tatsächlichen ESC prüfen
    (es gibt keinen einheitlichen Standard für die 8-polige Belegung).
