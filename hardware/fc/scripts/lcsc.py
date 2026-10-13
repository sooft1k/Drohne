# LCSC-/JLCPCB-Teileauswahl fuer die maschinelle Bestueckung der Oberseite.
#
# Stand: 13.10.2026. Quellen der Angaben (die Kataloge lcsc.com / jlcpcb.com selbst waren aus
# der Arbeitsumgebung nicht erreichbar):
#   Typ         Basic/Preferred-Liste von JLCPCB, automatisch abgerufen am 13.10.2026 06:00 UTC
#               (github.com/CDFER/jlcpcb-parts-database, scraped/assembly-details.csv).
#               Alles, was dort fehlt, ist Extended.
#   Werte       Basic-Teileliste mit Beschreibungen (github.com/ole00/kicad_bom_jlcpcb_auto,
#               tools/basic_parts.csv) bzw. JLCPCB-Katalog (github.com/yaqwsx/jlcparts,
#               Datenbank vom 13.10.2026)
#   Bestand     JLCPCB-Katalog (jlcparts) mit Abrufdatum je Teil; fuer aeltere Nummern, die dort
#               fehlen, nur Momentaufnahmen aus der Websuche (ohne Datum, "unbestaetigt").
#
# Preferred = "Preferred Extended": bei Economic-Bestueckung ohne Ruestgebuehr (wie Basic).
# Extended  = 3 USD Ruestgebuehr je Teilenummer.

PARTS = {
    # LCSC: (Hersteller-Nr., Hersteller, Kurzbeschreibung, Typ, Bestand, Bestand-Stand/Quelle)
    'C1525':    ('CL05B104KO5NNNC', 'Samsung', '100 nF 16 V X7R 0402', 'Basic', None, ''),
    'C307331':  ('CL05B104KB54PNC', 'Samsung', '100 nF 50 V X7R 0402', 'Basic', None, ''),
    'C52923':   ('CL05A105KA5NQNC', 'Samsung', '1 uF 25 V X5R 0402', 'Basic', None, ''),
    'C12530':   ('CL05A225MQ5NSNC', 'Samsung', '2,2 uF 6,3 V X5R 0402', 'Basic', None, ''),
    'C1549':    ('0402CG180J500NT', 'FH', '18 pF 50 V C0G 0402', 'Basic', None, ''),
    'C19666':   ('CL10A475KO8NNNC', 'Samsung', '4,7 uF 16 V X5R 0603', 'Basic', None, ''),
    'C19702':   ('CL10A106KP8NNNC', 'Samsung', '10 uF 10 V X5R 0603', 'Basic', None, ''),
    'C45783':   ('CL21A226MAQNNNE', 'Samsung', '22 uF 25 V X5R 0805', 'Basic', None, ''),
    'C17498688': ('CL32B106KBJZW6E', 'Samsung', '10 uF 50 V X7R 1210', 'Extended', 483,
                  'jlcparts 25.09.2026'),
    'C48615113': ('JVE35V470M10X10', 'jieerrui', '470 uF 35 V Elko 10x10,5, 150 mOhm/670 mA @100 kHz',
                  'Extended', 11128, 'jlcparts 10.10.2026'),
    'C25744':   ('0402WGF1002TCE', 'UNI-ROYAL', '10 kOhm 1 % 0402', 'Basic', None, ''),
    'C11702':   ('0402WGF1001TCE', 'UNI-ROYAL', '1 kOhm 1 % 0402', 'Basic', None, ''),
    'C25117':   ('0402WGF4700TCE', 'UNI-ROYAL', '470 Ohm 1 % 0402', 'Basic', None, ''),
    'C25741':   ('0402WGF1003TCE', 'UNI-ROYAL', '100 kOhm 1 % 0402', 'Basic', None, ''),
    'C25779':   ('0402WGF3302TCE', 'UNI-ROYAL', '33 kOhm 1 % 0402', 'Basic', None, ''),
    'C25924':   ('0402WGF8201TCE', 'UNI-ROYAL', '8,2 kOhm 1 % 0402', 'Preferred', None, ''),
    'C25900':   ('0402WGF4701TCE', 'UNI-ROYAL', '4,7 kOhm 1 % 0402', 'Basic', None, ''),
    'C25905':   ('0402WGF5101TCE', 'UNI-ROYAL', '5,1 kOhm 1 % 0402', 'Basic', None, ''),
    'C25076':   ('0402WGF1000TCE', 'UNI-ROYAL', '100 Ohm 1 % 0402', 'Basic', None, ''),
    'C17168':   ('0402WGF0000TCE', 'UNI-ROYAL', '0 Ohm 0402', 'Basic', None, ''),
    'C2286':    ('KT-0603R', 'Hubei KENTO', 'LED rot 0603', 'Basic', None, ''),
    'C72038':   ('19-213/Y2C-CQ2R2L/3T(CY)', 'Everlight', 'LED gelb 0603 (AlGaInP)', 'Preferred', None, ''),
    'C72043':   ('19-217/GHC-YR1S2/3T', 'Everlight', 'LED gruen 0603 (InGaN, Uf ca. 2,7 V)', 'Preferred',
                 None, ''),
    'C2128':    ('1N4148WS', 'CJ (Jiangsu Changjing)', 'Schaltdiode SOD-323', 'Basic', None, ''),
    'C19077448': ('BZT52C15S', 'hongjiacheng', 'Zener 15 V SOD-323', 'Preferred', 406382,
                  'jlcparts 29.09.2026'),
    'C2480':    ('SS14', 'MDD', 'Schottky 40 V 1 A SMA', 'Basic', None, ''),
    'C8545':    ('2N7002', 'CJ (Jiangsu Changjing)', 'N-MOSFET 60 V SOT-23', 'Basic', None, ''),
    'C146287':  ('SI2319CDS-T1-GE3', 'Vishay', 'P-MOSFET -40 V SOT-23', 'Extended', 39375,
                 'Websuche LCSC, Momentaufnahme ohne Datum (unbestaetigt)'),
    'C15742':   ('STM32F405RGT6', 'ST', 'Mikrocontroller LQFP-64', 'Extended', None,
                 'Websuche LCSC: 2 877 bis 47 940 je nach Momentaufnahme (unbestaetigt)'),
    'C1850418': ('ICM-42688-P', 'TDK InvenSense', 'IMU LGA-14', 'Extended', 5562,
                 'Websuche LCSC, Momentaufnahme ohne Datum (unbestaetigt)'),
    'C97521':   ('W25Q128JVSIQ', 'Winbond', 'SPI-Flash 128 Mbit SOIC-8 208 mil', 'Basic', None, ''),
    'C544370':  ('LMR33630BDDAR', 'TI', 'Schaltregler 3 A HSOP-8', 'Extended', 1840,
                 'Websuche LCSC, Momentaufnahme ohne Datum (unbestaetigt)'),
    'C6186':    ('AMS1117-3.3', 'AMS', 'LDO 3,3 V SOT-223', 'Basic', None, ''),
    'C7519':    ('USBLC6-2SC6', 'ST', 'USB-ESD-Schutz SOT-23-6', 'Extended', None,
                 'Websuche LCSC: 29 960 bis 54 100 je nach Momentaufnahme (unbestaetigt)'),
    'C20617566': ('XL2OI-111-8M', 'YXC', 'Quarz 8 MHz 12 pF +-10 ppm 3225-4P', 'Extended', 23519,
                  'jlcparts 06.10.2026'),
    'C7509833': ('BLM15PX601SH1D', 'Murata', 'Ferrit 600 Ohm@100 MHz 0402, 0,23 Ohm', 'Extended', 9512,
                 'jlcparts 22.09.2026'),
    'C6937839': ('XGL4020-332MEC', 'Coilcraft', '3,3 uH 34 mOhm 4,8/6,6 A, XAL/XGL-40xx-Landemuster',
                 'Extended', 1490, 'jlcparts 17.09.2026'),
    'C165948':  ('TYPE-C-31-M-12', 'HRO', 'USB-C-Buchse 16 Pin', 'Extended', None,
                 'Websuche LCSC/jlcsearch: 71 350 bis 349 576 je nach Momentaufnahme (unbestaetigt)'),
}

# Referenz -> LCSC-Nummer (nur Oberseite; Unterseite wird von Hand geloetet)
REF = {}
for lcsc, refs in {
    'C1525': 'C1 C2 C3 C4 C5 C7 C11 C14 C15 C16 C18 C20 C24 C28 C30 C32 C34',
    'C307331': 'C23', 'C52923': 'C8 C25', 'C12530': 'C9 C10 C17', 'C1549': 'C12 C13',
    'C19666': 'C6', 'C19702': 'C29 C33', 'C45783': 'C26 C27 C31', 'C17498688': 'C22',
    'C48615113': 'C21',
    'C25744': 'R1 R2 R4 R7 R8 R18', 'C11702': 'R5 R6 R14 R15', 'C25117': 'R16', 'C25741': 'R9',
    'C25779': 'R10', 'C25924': 'R11', 'C25900': 'R19 R20', 'C25905': 'R12 R13', 'C25076': 'R17',
    'C17168': 'R3',
    'C2286': 'D3', 'C72038': 'D4', 'C72043': 'D5', 'C2128': 'D6', 'C19077448': 'D1', 'C2480': 'D2',
    'C8545': 'Q2', 'C146287': 'Q1',
    'C15742': 'U1', 'C1850418': 'U2', 'C97521': 'U3', 'C544370': 'U4', 'C6186': 'U5', 'C7519': 'U6',
    'C20617566': 'Y1', 'C7509833': 'FB1', 'C6937839': 'L1', 'C165948': 'J1',
}.items():
    for r in refs.split():
        REF[r] = lcsc

# Alternativen, die nur mit Footprint-Aenderung Basic waeren (Ruestgebuehr sparen)
ALTERNATIVES = {
    'Y1': 'C115962 X50328MSB2GI 8 MHz Basic, aber Gehaeuse 5032 statt 3225; oder C13738 16 MHz '
          '3225 Basic (Firmware-Takt anpassen)',
    'FB1': 'C1002 GZ1608D601TF Basic, aber 0603 statt 0402',
    'C22': 'C13585 CL31A106KBHNNNE 10 uF 50 V Basic, aber 1206 statt 1210 und X5R: bei 25 V '
           'bleibt deutlich weniger Kapazitaet uebrig',
}

# Von Hand zu loetende Steckverbinder (Unterseite) - Vorschlaege zum Selbstbestellen
HAND = {
    'J2': 'JST SM08B-SRSS-TB (Original) oder kompatibel ZX-SH1.0-8PWT (LCSC C7430450, '
          '100 476 Stueck lt. jlcparts 21.09.2026)',
    'J3 J4 J5 J6 J9': '4-polige SMD-Stiftleiste 2,54 mm, Pins abwechselnd links/rechts '
                      '(KiCad PinHeader_1x04_P2.54mm_Vertical_SMD_Pin1Left)',
    'J7': '3-polige SMD-Stiftleiste 2,54 mm, Pin 1 links',
    'J8': '5-polige SMD-Stiftleiste 2,54 mm, Pin 1 links',
    'J10': '8-polige SMD-Stiftleiste 2,54 mm, Pin 1 links',
    'J11': '2-polige SMD-Stiftleiste 2,54 mm, Pin 1 links',
}
