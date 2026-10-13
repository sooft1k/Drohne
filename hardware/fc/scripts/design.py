# Flight Controller - zentrale Datenquelle fuer Schaltplan und Platine.
#
# Jedes Bauteil: Referenz, Symbol (Bibliothek:Name), Wert, Footprint,
# Zuordnung Pinnummer -> Netz (None = bewusst unbeschaltet), Schaltplan-Gruppe.
# Aus dieser Datei werden fc.kicad_sch (gen_sch.py) und fc.kicad_pcb
# (gen_pcb.py) erzeugt; die aus dem Schaltplan exportierte Netzliste wird
# gegen diese Datei geprueft (check_netlist.py).

R0402 = 'Resistor_SMD:R_0402_1005Metric'
C0402 = 'Capacitor_SMD:C_0402_1005Metric'
C0603 = 'Capacitor_SMD:C_0603_1608Metric'
C0805 = 'Capacitor_SMD:C_0805_2012Metric'
C1210 = 'Capacitor_SMD:C_1210_3225Metric'
LED0603 = 'LED_SMD:LED_0603_1608Metric'
SOT23 = 'Package_TO_SOT_SMD:SOT-23'
SOD323 = 'Diode_SMD:D_SOD-323'
SH = 'Connector_JST:JST_SH_SM%02dB-SRSS-TB_1x%02d-1MP_P1.00mm_Horizontal'   # JST-SH, seitlich steckbar
SJ_OPEN = 'Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm'
TP_PAD = 'TestPoint:TestPoint_Pad_D1.5mm'

PARTS = []


def part(ref, sym, value, fp, pins, group, desc='', mpn='', side='top'):
    PARTS.append(dict(ref=ref, sym=sym, value=value, fp=fp, pins=pins,
                      group=group, desc=desc, mpn=mpn, side=side))


def R(ref, value, a, b, group, desc='', fp=R0402):
    part(ref, 'Device:R', value, fp, {'1': a, '2': b}, group, desc)


def C(ref, value, a, b, group, desc='', fp=C0402):
    part(ref, 'Device:C', value, fp, {'1': a, '2': b}, group, desc)


# ---------------------------------------------------------------- Prozessor
MCU_PINS = {
    '1': '+3V3',        # VBAT (kein RTC-Akku) an 3,3 V
    '2': 'LED_ARM',     # PC13 (LED aktiv-low, Pin zieht nur Strom)
    '3': 'LED_ERR',     # PC14 (LED aktiv-low)
    '4': 'LED_LINK',    # PC15 (LED aktiv-low)
    '5': 'OSC_IN',      # PH0
    '6': 'OSC_OUT',     # PH1
    '7': 'NRST',
    '8': 'VBAT_SENSE',  # PC0  ADC1_IN10 (fest)
    '9': 'GPIO1',       # PC1
    '10': 'GPIO2',      # PC2
    '11': 'GPIO3',      # PC3
    '12': 'GND',        # VSSA
    '13': '+3V3A',      # VDDA (VREF+ intern, LQFP64)
    '14': 'BUZZER',     # PA0  TIM2_CH1 / TIM5_CH1
    '15': None,         # PA1  (frei; Pad laesst sich zwischen BUZZER/DBG_TX nicht entflechten)
    '16': 'DBG_TX',     # PA2  USART2_TX
    '17': 'DBG_RX',     # PA3  USART2_RX
    '18': 'GND',        # VSS
    '19': '+3V3',       # VDD
    '20': 'CURR_SENSE', # PA4  ADC12_IN4
    '21': 'GPIO5',      # PA5
    '22': 'M3',         # PA6  TIM3_CH1
    '23': 'M4',         # PA7  TIM3_CH2
    '24': 'GPIO6',      # PC4
    '25': 'GPIO4',      # PC5
    '26': 'M1',         # PB0  TIM3_CH3
    '27': 'M2',         # PB1  TIM3_CH4
    '28': 'BOOT1',      # PB2 / BOOT1 -> 10k nach GND
    '29': None,         # PB10
    '30': None,         # PB11
    '31': 'VCAP1',
    '32': '+3V3',       # VDD
    '33': None,         # PB12
    '34': None,         # PB13
    '35': None,         # PB14
    '36': None,         # PB15
    '37': 'VTX_TX',     # PC6  USART6_TX
    '38': 'VTX_RX',     # PC7  USART6_RX
    '39': None,         # PC8
    '40': None,         # PC9
    '41': None,         # PA8
    '42': 'ELRS_TX',    # PA9  USART1_TX
    '43': 'ELRS_RX',    # PA10 USART1_RX
    '44': 'USB_DM',     # PA11
    '45': 'USB_DP',     # PA12
    '46': 'SWDIO',      # PA13
    '47': 'VCAP2',
    '48': '+3V3',       # VDD
    '49': 'SWCLK',      # PA14
    '50': None,         # PA15
    '51': 'FLASH_CS',   # PC10
    '52': 'GYRO_INT1',  # PC11 EXTI11
    '53': 'GPS_TX',     # PC12 UART5_TX
    '54': 'GPS_RX',     # PD2  UART5_RX
    '55': 'SPI1_SCK',   # PB3
    '56': 'SPI1_MISO',  # PB4
    '57': 'SPI1_MOSI',  # PB5
    '58': 'GYRO_CS',    # PB6
    '59': 'GYRO_INT2',  # PB7  TIM4_CH2 (CLKIN moeglich)
    '60': 'BOOT0',
    '61': 'I2C_SCL',    # PB8  I2C1_SCL
    '62': 'I2C_SDA',    # PB9  I2C1_SDA
    '63': 'GND',        # VSS
    '64': '+3V3',       # VDD
}
part('U1', 'MCU_ST_STM32F4:STM32F405RGTx', 'STM32F405RGT6',
     'Package_QFP:LQFP-64_10x10mm_P0.5mm', MCU_PINS, 'mcu',
     'Mikrocontroller', 'STM32F405RGT6')

C('C1', '100nF', '+3V3', 'GND', 'mcu', 'VBAT (Pin 1)')
C('C2', '100nF', '+3V3', 'GND', 'mcu', 'VDD Pin 19')
C('C3', '100nF', '+3V3', 'GND', 'mcu', 'VDD Pin 32')
C('C4', '100nF', '+3V3', 'GND', 'mcu', 'VDD Pin 48')
C('C5', '100nF', '+3V3', 'GND', 'mcu', 'VDD Pin 64')
C('C6', '4.7uF', '+3V3', 'GND', 'mcu', 'VDD Eingang, Keramik', fp=C0603)
C('C7', '100nF', '+3V3A', 'GND', 'mcu', 'VDDA')
C('C8', '1uF', '+3V3A', 'GND', 'mcu', 'VDDA')
part('FB1', 'Device:FerriteBead_Small', '600R@100MHz',
     'Inductor_SMD:L_0402_1005Metric', {'1': '+3V3', '2': '+3V3A'}, 'mcu',
     'VDDA-Filter', 'BLM15PX601SH1D')
C('C9', '2.2uF', 'VCAP1', 'GND', 'mcu', 'VCAP_1, X5R/X7R')
C('C10', '2.2uF', 'VCAP2', 'GND', 'mcu', 'VCAP_2, X5R/X7R')
C('C11', '100nF', 'NRST', 'GND', 'mcu', 'NRST')
R('R1', '10k', 'BOOT0', 'GND', 'mcu', 'BOOT0 Pull-down')
part('JP1', 'Jumper:SolderJumper_2_Open', 'BOOT0', SJ_OPEN,
     {'1': 'BOOT0', '2': '+3V3'}, 'mcu', 'Loetbruecke: geschlossen = DFU-Bootloader')
R('R2', '10k', 'BOOT1', 'GND', 'mcu', 'PB2/BOOT1 Pull-down (fuer DFU)')
part('Y1', 'Device:Crystal_GND24', '8MHz', 'Crystal:Crystal_SMD_3225-4Pin_3.2x2.5mm',
     {'1': 'OSC_IN', '2': 'GND', '3': 'XTAL_OUT', '4': 'GND'}, 'mcu',
     'Quarz 8 MHz, CL 12 pF', 'XL2OI-111-8M')
C('C12', '18pF', 'OSC_IN', 'GND', 'mcu', 'Lastkondensator fuer CL 12 pF')
C('C13', '18pF', 'XTAL_OUT', 'GND', 'mcu', 'Lastkondensator fuer CL 12 pF')
R('R3', '0R', 'OSC_OUT', 'XTAL_OUT', 'mcu', 'Quarz-Serienwiderstand (Platzhalter)')

# ---------------------------------------------------------------- Messung
R('R4', '10k', 'VBAT', 'VBAT_SENSE', 'meas', 'Spannungsteiler oben')
R('R5', '1k', 'VBAT_SENSE', 'GND', 'meas', 'Spannungsteiler unten')
C('C14', '100nF', 'VBAT_SENSE', 'GND', 'meas', 'parallel zu R5')
R('R6', '1k', 'CURR_IN', 'CURR_SENSE', 'meas', 'Schutzwiderstand Strommessung')
C('C15', '100nF', 'CURR_SENSE', 'GND', 'meas', 'Tiefpass Strommessung')

# ---------------------------------------------------------------- Gyro
part('U2', 'fc_custom:ICM-42688-P', 'ICM-42688-P',
     'Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y',
     {'1': 'SPI1_MISO', '2': None, '3': None, '4': 'GYRO_INT1',
      '5': '+3V3', '6': 'GND', '7': 'GND', '8': '+3V3', '9': 'GYRO_INT2',
      '10': 'GND', '11': 'GND', '12': 'GYRO_CS', '13': 'SPI1_SCK',
      '14': 'SPI1_MOSI'}, 'gyro', 'IMU, SPI', 'ICM-42688-P')
C('C16', '100nF', '+3V3', 'GND', 'gyro', 'Gyro VDD')
C('C17', '2.2uF', '+3V3', 'GND', 'gyro', 'Gyro VDD')
C('C18', '100nF', '+3V3', 'GND', 'gyro', 'Gyro VDDIO')
R('R7', '10k', 'GYRO_CS', '+3V3', 'gyro', 'CS Pull-up')

# ---------------------------------------------------------------- Flash
part('U3', 'Memory_Flash:W25Q128JVS', 'W25Q128JVSIQ',
     'Package_SO:SOIC-8_5.23x5.23mm_P1.27mm',
     {'1': 'FLASH_CS', '2': 'SPI1_MISO', '3': '+3V3', '4': 'GND',
      '5': 'SPI1_MOSI', '6': 'SPI1_SCK', '7': '+3V3', '8': '+3V3'},
     'flash', 'Blackbox-Flash 16 MB', 'W25Q128JVSIQ')
C('C20', '100nF', '+3V3', 'GND', 'flash', 'Flash VCC')
R('R8', '10k', 'FLASH_CS', '+3V3', 'flash', 'CS Pull-up')

# ---------------------------------------------------------------- Eingang / Verpolschutz
part('Q1', 'Transistor_FET:Si2319CDS', 'Si2319CDS', SOT23,
     {'1': 'Q1_G', '2': 'VBAT', '3': 'VBAT_IN'}, 'pwr_in',
     'Verpolschutz P-FET 40 V (statt SI2301, s. Doku)', 'SI2319CDS-T1-GE3')
R('R9', '100k', 'Q1_G', 'GND', 'pwr_in', 'Gate nach GND')
part('D1', 'Device:D_Zener', 'BZT52C15S', SOD323, {'1': 'VBAT', '2': 'Q1_G'},
     'pwr_in', 'Zener 15 V Gate-Schutz', 'BZT52C15S')
part('C21', 'Device:C_Polarized', '470uF/35V', 'Capacitor_SMD:CP_Elec_10x10.5',
     {'1': 'VBAT', '2': 'GND'}, 'pwr_in', 'Eingangs-Elko, low ESR', 'JVE35V470M10X10')
C('C22', '10uF/50V', 'VBAT', 'GND', 'pwr_in', 'Eingangs-Keramik X7R (1210 wegen DC-Bias)', fp=C1210)

# ---------------------------------------------------------------- 5-V-Schaltregler
part('U4', 'Regulator_Switching:LMR33630BDDA', 'LMR33630BDDA',
     'fc_custom:Texas_HSOP-8-1EP_3.9x4.9mm_P1.27mm_ThermalVias_TopOnly',
     {'1': 'GND', '2': 'VBAT', '3': 'VBAT', '4': None, '5': 'BUCK_FB',
      '6': 'BUCK_VCC', '7': 'BUCK_BOOT', '8': 'BUCK_SW', '9': 'GND'},
     'buck', '5 V / 3 A, 3,8-36 V Eingang, 1,4 MHz', 'LMR33630BDDAR')
C('C23', '100nF/50V', 'VBAT', 'GND', 'buck', 'VIN HF-Abblockung')
C('C24', '100nF', 'BUCK_BOOT', 'BUCK_SW', 'buck', 'Bootstrap')
C('C25', '1uF', 'BUCK_VCC', 'GND', 'buck', 'VCC intern')
part('L1', 'Device:L', '3.3uH', 'Inductor_SMD:L_Coilcraft_XAL4040-XXX',
     {'1': 'BUCK_SW', '2': '+5V'}, 'buck', 'Isat >= 4 A, 4x4 mm', 'XGL4020-332MEC')
R('R10', '33k', '+5V', 'BUCK_FB', 'buck', 'FB oben (Vout = 1 V * (1 + R10/R11))')
R('R11', '8.2k', 'BUCK_FB', 'GND', 'buck', 'FB unten: 33k/8,2k -> 5,02 V')
C('C26', '22uF/16V', '+5V', 'GND', 'buck', '5-V-Ausgang', fp=C0805)
C('C27', '22uF/16V', '+5V', 'GND', 'buck', '5-V-Ausgang', fp=C0805)
C('C28', '100nF', '+5V', 'GND', 'buck', '5-V-Ausgang')

# ---------------------------------------------------------------- 3,3-V-Regler
part('U5', 'Regulator_Linear:AMS1117-3.3', 'AMS1117-3.3',
     'Package_TO_SOT_SMD:SOT-223-3_TabPin2',
     {'1': 'GND', '2': '+3V3', '3': '+5V'}, 'ldo', '3,3-V-Linearregler', 'AMS1117-3.3')
C('C29', '10uF', '+5V', 'GND', 'ldo', 'AMS1117 Eingang', fp=C0603)
C('C30', '100nF', '+5V', 'GND', 'ldo', 'AMS1117 Eingang')
C('C31', '22uF', '+3V3', 'GND', 'ldo', 'AMS1117 Ausgang', fp=C0805)
C('C32', '100nF', '+3V3', 'GND', 'ldo', 'AMS1117 Ausgang')

# ---------------------------------------------------------------- USB
part('J1', 'Connector:USB_C_Receptacle_USB2.0_16P', 'USB-C',
     'fc_custom:USB_C_Receptacle_HRO_TYPE-C-31-M-12_Kante',
     {'A1': 'GND', 'A4': 'VBUS', 'A5': 'USB_CC1', 'A6': 'USB_DP', 'A7': 'USB_DM',
      'A8': None, 'A9': 'VBUS', 'A12': 'GND', 'B1': 'GND', 'B4': 'VBUS',
      'B5': 'USB_CC2', 'B6': 'USB_DP', 'B7': 'USB_DM', 'B8': None, 'B9': 'VBUS',
      'B12': 'GND', 'S1': 'GND'}, 'usb', 'USB-C Buchse', 'TYPE-C-31-M-12')
part('U6', 'Power_Protection:USBLC6-2SC6', 'USBLC6-2SC6', 'Package_TO_SOT_SMD:SOT-23-6',
     {'1': 'USB_DP', '2': 'GND', '3': 'USB_DM', '4': 'USB_DM', '5': None,
      '6': 'USB_DP'}, 'usb', 'ESD-Schutz USB', 'USBLC6-2SC6')
part('D2', 'Device:D_Schottky', 'SS14', 'Diode_SMD:D_SMA', {'1': '+5V', '2': 'VBUS'},
     'usb', 'USB-VBUS -> 5 V, verhindert Rueckspeisung', 'SS14')
R('R12', '5.1k', 'USB_CC1', 'GND', 'usb', 'CC1')
R('R13', '5.1k', 'USB_CC2', 'GND', 'usb', 'CC2')
C('C33', '10uF', 'VBUS', 'GND', 'usb', 'VBUS', fp=C0603)
C('C34', '100nF', 'VBUS', 'GND', 'usb', 'VBUS')

# ---------------------------------------------------------------- LEDs
part('D3', 'Device:LED', 'rot', LED0603, {'1': 'LED_ARM', '2': 'LED_ARM_A'}, 'led', 'ARMED (aktiv-low)')
part('D4', 'Device:LED', 'gelb', LED0603, {'1': 'LED_ERR', '2': 'LED_ERR_A'}, 'led', 'FEHLER (aktiv-low)')
part('D5', 'Device:LED', 'gruen', LED0603, {'1': 'LED_LINK', '2': 'LED_LINK_A'}, 'led', 'FUNK (aktiv-low)')
R('R14', '1k', '+3V3', 'LED_ARM_A', 'led', 'Vorwiderstand')
R('R15', '1k', '+3V3', 'LED_ERR_A', 'led', 'Vorwiderstand')
R('R16', '470R', '+3V3', 'LED_LINK_A', 'led', 'Vorwiderstand (gruene InGaN-LED, Uf ca. 2,7 V)')

# ---------------------------------------------------------------- Piepser
part('Q2', 'Transistor_FET:2N7002', '2N7002', SOT23,
     {'1': 'BUZ_G', '2': 'GND', '3': 'BUZ_N'}, 'buz', 'Piepser-Treiber')
R('R17', '100R', 'BUZZER', 'BUZ_G', 'buz', 'Gate-Serienwiderstand')
R('R18', '10k', 'BUZ_G', 'GND', 'buz', 'Gate Pull-down')
part('D6', 'Device:D', '1N4148WS', SOD323, {'1': '+5V', '2': 'BUZ_N'}, 'buz',
     'Freilaufdiode')
part('J11', 'Connector_Generic:Conn_01x02', 'BUZZER', SH % (2, 2),
     {'1': '+5V', '2': 'BUZ_N'}, 'buz', 'Piezo-Summer (+ / -)', side='bottom')

# ---------------------------------------------------------------- I2C
R('R19', '4.7k', 'I2C_SDA', '+3V3', 'i2c', 'Pull-up SDA')
R('R20', '4.7k', 'I2C_SCL', '+3V3', 'i2c', 'Pull-up SCL')

# ---------------------------------------------------------------- Steckverbinder (Unterseite)
part('J2', 'Connector_Generic:Conn_01x08', 'ESC', SH % (8, 8),
     {'1': 'VBAT_IN', '2': 'GND', '3': 'CURR_IN', '4': 'ESC_5V',
      '5': 'M1', '6': 'M2', '7': 'M3', '8': 'M4'}, 'conn',
     'ESC, JST-SH 8-polig (Pinfolge pruefen!)', 'SM08B-SRSS-TB', side='bottom')
part('JP2', 'Jumper:SolderJumper_2_Open', 'ESC_5V', SJ_OPEN,
     {'1': 'ESC_5V', '2': '+5V'}, 'conn', 'nur schliessen, wenn der ESC 5 V vom FC braucht')
part('J3', 'Connector_Generic:Conn_01x04', 'ELRS', SH % (4, 4),
     {'1': 'ELRS_TX', '2': 'ELRS_RX', '3': '+5V', '4': 'GND'}, 'conn',
     'ELRS-Empfaenger', side='bottom')
part('J4', 'Connector_Generic:Conn_01x04', 'I2C', SH % (4, 4),
     {'1': 'I2C_SDA', '2': 'I2C_SCL', '3': '+3V3', '4': 'GND'}, 'conn',
     'I2C extern', side='bottom')
part('J5', 'Connector_Generic:Conn_01x04', 'GPS', SH % (4, 4),
     {'1': 'GPS_TX', '2': 'GPS_RX', '3': '+5V', '4': 'GND'}, 'conn',
     'GPS', side='bottom')
part('J6', 'Connector_Generic:Conn_01x04', 'VTX', SH % (4, 4),
     {'1': 'VTX_TX', '2': 'VTX_RX', '3': '+5V', '4': 'GND'}, 'conn',
     'Videosender', side='bottom')
part('J7', 'Connector_Generic:Conn_01x03', 'CAM', SH % (3, 3),
     {'1': '+5V', '2': 'CAM_VIDEO', '3': 'GND'}, 'conn', 'Kamera', side='bottom')
part('J8', 'Connector_Generic:Conn_01x05', 'SWD', SH % (5, 5),
     {'1': 'SWDIO', '2': 'SWCLK', '3': 'NRST', '4': '+3V3', '5': 'GND'}, 'conn',
     'SWD', side='bottom')
part('J9', 'Connector_Generic:Conn_01x04', 'UART', SH % (4, 4),
     {'1': 'DBG_TX', '2': 'DBG_RX', '3': '+3V3', '4': 'GND'}, 'conn',
     'Debug-UART', side='bottom')
part('J10', 'Connector_Generic:Conn_01x08', 'GPIO', SH % (8, 8),
     {'1': 'GPIO1', '2': 'GPIO2', '3': 'GPIO3', '4': 'GPIO4', '5': 'GPIO5',
      '6': 'GPIO6', '7': '+3V3', '8': 'GND'}, 'conn', 'Freie GPIOs (XSHUT)',
     side='bottom')
part('TP5', 'Connector:TestPoint', 'VIDEO', TP_PAD, {'1': 'CAM_VIDEO'}, 'conn',
     'Video-Durchleitung zum analogen VTX', side='bottom')

# Hersteller-Nr. der JST-SH-Buchsen (Original JST; bestueckt wird ein kompatibler Typ, lcsc.py)
for _p in PARTS:
    if _p['fp'].startswith('Connector_JST:JST_SH_SM') and not _p['mpn']:
        _p['mpn'] = 'SM%sB-SRSS-TB' % _p['fp'].split('_SM')[1][:2]

# ---------------------------------------------------------------- Testpunkte, Mechanik
part('TP1', 'Connector:TestPoint', '3V3', TP_PAD, {'1': '+3V3'}, 'mech', 'Testpunkt', side='bottom')
part('TP2', 'Connector:TestPoint', '5V', TP_PAD, {'1': '+5V'}, 'mech', 'Testpunkt', side='bottom')
part('TP3', 'Connector:TestPoint', 'VBAT', TP_PAD, {'1': 'VBAT'}, 'mech', 'Testpunkt', side='bottom')
part('TP4', 'Connector:TestPoint', 'GND', TP_PAD, {'1': 'GND'}, 'mech', 'Testpunkt', side='bottom')
part('H1', 'Mechanical:MountingHole', 'M3', 'MountingHole:MountingHole_3.2mm_M3', {}, 'mech')
part('H2', 'Mechanical:MountingHole_Pad', 'M3 GND', 'MountingHole:MountingHole_3.2mm_M3_Pad',
     {'1': 'GND'}, 'mech', 'einzige Bohrung mit Masse (neben dem ESC-Stecker)')
part('H3', 'Mechanical:MountingHole', 'M3', 'MountingHole:MountingHole_3.2mm_M3', {}, 'mech')
part('H4', 'Mechanical:MountingHole', 'M3', 'MountingHole:MountingHole_3.2mm_M3', {}, 'mech')
part('FID1', 'Mechanical:Fiducial', 'Fiducial', 'Fiducial:Fiducial_1mm_Mask2mm', {}, 'mech')
part('FID2', 'Mechanical:Fiducial', 'Fiducial', 'Fiducial:Fiducial_1mm_Mask2mm', {}, 'mech')
part('FID3', 'Mechanical:Fiducial', 'Fiducial', 'Fiducial:Fiducial_1mm_Mask2mm', {}, 'mech')

# Netze, die als Stromversorgungssymbol gezeichnet werden (Symbol, Netzname)
POWER_SYMBOLS = {
    'GND': 'power:GND',
    '+3V3': 'power:+3V3',
    '+5V': 'power:+5V',
    'VBUS': 'power:VBUS',
    'VBAT': 'fc_custom:VBAT',
    '+3V3A': 'fc_custom:+3V3A',
}
# Netze, die einen PWR_FLAG brauchen (nicht von einem power_out-Pin getrieben)
PWR_FLAG_NETS = ['GND', '+5V', 'VBAT', 'VBUS', '+3V3A', 'VBAT_IN']

GROUP_TITLES = {
    'mcu': 'Prozessor STM32F405RGT6',
    'meas': 'Messung Akkuspannung / Strom',
    'gyro': 'Gyro ICM-42688-P (SPI1)',
    'flash': 'Blackbox-Flash W25Q128 (SPI1)',
    'pwr_in': 'Eingang 2-6S, Verpolschutz',
    'buck': '5-V-Schaltregler',
    'ldo': '3,3-V-Regler',
    'usb': 'USB-C',
    'led': 'Status-LEDs',
    'buz': 'Piepser',
    'i2c': 'I2C Pull-ups',
    'conn': 'Steckverbinder (Unterseite)',
    'mech': 'Testpunkte / Mechanik',
}


def nets():
    out = {}
    for p in PARTS:
        for pin, n in p['pins'].items():
            if n:
                out.setdefault(n, []).append((p['ref'], pin))
    return out


if __name__ == '__main__':
    ns = nets()
    for n in sorted(ns):
        print('%-12s %s' % (n, ' '.join('%s.%s' % x for x in ns[n])))
    single = [n for n in ns if len(ns[n]) < 2]
    print('Teile:', len(PARTS), 'Netze:', len(ns), 'Einzelpin-Netze:', single)
