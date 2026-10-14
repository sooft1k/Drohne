# Bauteilplatzierung: Referenz -> (u, v, Drehung, Seite)
# u/v in mm relativ zur Platinenmitte (v nach unten), Drehung in Grad (KiCad, CCW).
T, B = 'top', 'bottom'

PLACE = {
    # ---------------------------------------------------------- Prozessor
    'U1': (1.5, 8.0, 0, T),
    'C1': (-5.3, 2.9, 90, T),      # VBAT Pin 1
    'C5': (-3.4, 1.3, 90, T),      # VDD Pin 64
    'C2': (-2.3, 15.45, 180, T),   # VDD Pin 19
    'C3': (5.6, 15.65, 270, T),    # VDD Pin 32
    'C9': (4.5, 15.65, 270, T),    # VCAP1
    'C4': (8.5, 2.6, 90, T),        # VDD Pin 48
    'C10': (9.6, 2.6, 90, T),      # VCAP2
    'C6': (5.6, -6.3, 0, T),     # 10 uF/25 V
    'C7': (-5.85, 10.0, 90, T),    # VDDA 100 nF direkt an Pin 13 (VDDA) / 12 (VSSA)
    'C8': (-6.3, 12.9, 0, T),      # VDDA 1 uF
    'FB1': (-8.5, 12.4, 270, T),
    'C11': (-7.3, 9.4, 0, T),      # NRST
    'R1': (-4.2, -1.0, 0, T),      # BOOT0 pull-down
    'JP1': (-4.6, -2.9, 0, T),     # BOOT0 Loetbruecke
    'R2': (3.3, 16.5, 90, T),      # BOOT1
    'Y1': (-8.6, 5.0, 180, T),
    'C12': (-7.3, 2.35, 90, T),
    'C13': (-11.65, 5.85, 180, T),
    'R3': (-8.6, 7.45, 180, T),
    # ---------------------------------------------------------- Messung
    'R4': (-11.1, -11.3, 270, T),
    'R5': (-9.3, 10.0, 0, T),
    'C14': (-9.3, 9.0, 0, T),
    'R6': (-0.6, 18.2, 90, T),
    'C15': (-0.6, 16.3, 90, T),
    # ---------------------------------------------------------- Gyro
    'U2': (1.5, -2.2, 180, T),
    'C16': (-1.25, -2.95, 180, T),
    'C17': (-1.25, -3.95, 180, T),
    'C18': (2.4, -4.75, 90, T),
    'R7': (-5.2, -4.7, 0, T),
    # ---------------------------------------------------------- Flash
    'U3': (-9.5, -3.5, 270, T),
    'C20': (-5.7, 0.1, 0, T),
    'R8': (-5.6, -7.6, 0, T),
    # ---------------------------------------------------------- Eingang
    'Q1': (9.6, 17.6, 0, T),
    'R9': (5.0, 17.5, 180, T),
    'D1': (6.0, 19.05, 0, T),      # Koerper >= 0,8 mm von der Suedkante (Nutzentrennrand)
    'C21': (-7.0, -14.2, 0, T),    # 47 uF 6,3x5,4; Pluspol (West) am Akkuteiler R4
    'C22': (9.45, -18.0, 180, T),  # Eingangs-Keramik: Masse-Pad direkt an U4 Pin 1 (PGND)
    # ---------------------------------------------------------- 5-V-Regler
    'U4': (4.4, -15.2, 270, T),
    'C23': (5.67, -19.5, 0, T),
    'C24': (5.67, -10.9, 0, T),
    'C25': (3.77, -10.2, 270, T),
    'L1': (9.6, -10.6, 0, T),
    'R10': (2.0, -9.6, 180, T),
    'R11': (2.0, -10.6, 0, T),
    'C26': (13.0, -9.8, 90, T),
    'C27': (15.0, -9.5, 90, T),
    'C28': (16.95, -9.6, 90, T),
    # ---------------------------------------------------------- 3,3-V-Regler
    'U5': (16.1, -4.2, 180, T),
    'C29': (18.95, -9.0, 0, T),
    'C30': (18.95, -10.5, 0, T),
    'C31': (10.6, -3.4, 270, T),
    'C32': (9.0, -3.4, 270, T),
    # ---------------------------------------------------------- USB
    'J1': (17.85, 6.0, 90, T),
    'U6': (10.27, 6.3, 0, T),
    'D2': (10.0, 11.6, 90, T),     # USB -> 4,5 V (Kathode Sued)
    'D7': (10.0, 10.4, 90, B),     # 5 V -> 4,5 V, Unterseite unter D2 (Kathode Sued, frei von J2)
    'R12': (11.55, 2.45, 0, T),     # CC1: Pad 1 west (Via), Pad 2 GND
    'R13': (11.63, 3.65, 180, T),   # CC2: Pad 1 ost (zur Buchse), Pad 2 GND
    'C33': (11.1, 0.9, 180, T),     # VBUS-Pad ost (zur Buchse)
    'C34': (10.75, -0.6, 180, T),
    # ---------------------------------------------------------- LEDs
    # Reihe oestlich des Gyros (die Westkante oben gehoert den Buchsen ELRS/I2C/GPS):
    # LED senkrecht, Kathode nach Sueden; Vorwiderstand direkt noerdlich, +3,3 V an der
    # 3,3-V-Leitung vom AMS1117; Beschriftung ARM/ERR/LINK noerdlich der Widerstaende
    'D3': (4.2, -0.85, 90, T),
    'D4': (6.1, -0.85, 90, T),
    'D5': (8.0, -0.85, 90, T),
    'R14': (4.2, -3.3, 270, T),
    'R15': (6.1, -3.3, 270, T),
    'R16': (8.0, -3.3, 270, T),
    # ---------------------------------------------------------- Piepser
    'Q2': (-9.6, 15.6, 0, T),
    'R17': (-8.0, 18.3, 0, T),
    'R18': (-10.0, 18.3, 0, T),
    'D6': (-12.8, 9.0, 90, T),
    # ---------------------------------------------------------- I2C
    'R19': (-3.0, -5.6, 0, T),
    'R20': (-3.0, -6.6, 0, T),
    # ---------------------------------------------------------- Steckverbinder
    # JST-SH seitlich. Unterseite: Drehung 0 = Oeffnung nach Sueden, 90 = Westen, 180 = Norden,
    # 270 = Osten. Oberseite: 0 = Sueden, 90 = Osten, 180 = Norden, 270 = Westen.
    # Randbuchsen: Ursprung 3,3 mm vom Platinenrand, Montagepads 0,5 mm vom Rand.
    # Oberseite, Westkante, Pin 1 jeweils im Norden. Die Courtyards (7,8 mm) muessen 3,45 mm
    # Abstand zur Bohrungsmitte halten (Courtyard der M3-Bohrung): 3 x 7,8 mm in 23,6 mm.
    'J5': (-17.7, -7.875, 270, T),    # GPS
    'J4': (-17.7, 0.0, 270, T),       # I2C
    'J3': (-17.7, 7.875, 270, T),     # ELRS
    # Unterseite
    'J2': (4.0, 17.2, 0, B),          # ESC, Suedrand
    'J6': (-6.9, 17.2, 0, B),         # VTX, Suedrand West
    'J11': (-9.0, -17.2, 180, B),     # Piepser, Nordrand West
    'J9': (-2.1, -17.2, 180, B),      # UART, Nordrand Mitte (westlich der Waermevias von U4)
    'J10': (-17.7, -6.0, 90, B),      # GPIO, Westrand
    'J7': (-17.7, 3.8, 90, B),        # CAM, Westrand
    'J8': (17.7, -5.0, 270, B),       # SWD, Ostrand
    'TP5': (-4.6, 12.0, 0, B),
    'TP1': (-5.08, -11.6, 0, B),
    'TP2': (16.8, 5.5, 0, B),
    'TP3': (-19.6, 9.6, 0, B),
    'TP4': (2.5, -11.6, 0, B),
    # ---------------------------------------------------------- Mechanik
    'H1': (-15.25, 15.25, 0, T),
    'H2': (15.25, 15.25, 0, T),
    'H3': (15.25, -15.25, 0, T),
    'H4': (-15.25, -15.25, 0, T),
    'FID1': (-19.2, 18.7, 0, T),       # Suedwest-Ecke
    'FID2': (19.4, 12.6, 0, T),        # Ostrand, suedlich der USB-C-Buchse
    'FID3': (-19.2, -18.7, 0, T),      # Nordwest-Ecke
}

FIDUCIALS = []

# (Text, u, v, Groesse, Winkel); Groesse = Hoehe oder (Breite, Hoehe)
TOP_TEXT = [
    ('FC F405 v1.0', 1.0, -7.9, 1.0, 0),
    ('A', 4.2, -4.9, 1.0, 0),          # LED rot ARM
    ('E', 6.1, -4.9, 1.0, 0),          # LED gelb ERR
    ('L', 8.0, -4.9, 1.0, 0),          # LED gruen LINK
    ('GPS', -13.75, -7.875, 1.0, 90),
    ('I2C', -13.75, 0.0, 1.0, 90),
    ('ELRS', -13.75, 5.9, 1.0, 90),
]
# Flugrichtung: Nordkante = vorn. Pfeile (gefuellte Dreiecke mit Schaft) links und rechts der
# Aufschrift VORNE an der Nordkante der Oberseite: (Spitze u, Spitze v, Laenge, Kopfbreite)
FRONT_ARROWS = [(-8.3, -19.9, 1.9, 1.4), (0.3, -19.9, 1.9, 1.4)]
TOP_TEXT.append(('VORNE', -4.0, -18.95, 1.2, 0))

BOTTOM_TEXT = [
    ('BUZ', -9.0, -13.2, 1.0, 0),
    ('UART', -2.1, -13.2, 1.0, 0),
    ('GPIO', -13.6, -6.0, 1.0, 90),
    ('CAM', -13.6, 3.8, 1.0, 90),
    ('SWD', 13.6, -5.0, 1.0, 90),
    ('VTX', -6.9, 13.2, 1.0, 0),
    ('ESC', 4.0, 13.2, 1.0, 0),
]
