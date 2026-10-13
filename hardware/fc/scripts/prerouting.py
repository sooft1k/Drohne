# Handverdrahtung der kritischen Stellen (vor dem Autorouter, alles gesperrt):
#  - Abblockkondensatoren direkt an die Versorgungspins, Vias unmittelbar daneben
#  - Gyro-Versorgung und -Masse (keine Vias unter dem Chip)
#  - Schaltregler: Eingangskondensator, Schaltknoten, Bootstrap, VCC, Rueckkopplung
#  - 3,3-V-Ausgang des Linearreglers in die 3,3-V-Flaeche
# Alle uebrigen Versorgungspads bekommen ihr Via automatisch in fanout.py.


import pcbnew
B_CU = pcbnew.B_Cu


def apply(bd):
    P = bd.pad_pos
    T = bd.track
    V = bd.via

    # ------------------------------------------------ STM32: VDD/VSS-Paare
    p64, p63 = P('U1', '64'), P('U1', '63')
    c5p1, c5p2 = P('C5', '1'), P('C5', '2')
    T('+3V3', [p64, (p64[0], c5p1[1]), c5p1], 0.3)
    T('GND', [p63, (p63[0], c5p2[1]), c5p2], 0.3)
    V('GND', c5p2[0] - 0.9, c5p2[1])
    T('GND', [c5p2, (c5p2[0] - 0.9, c5p2[1])], 0.3)
    # VDD64 und VBAT (Pin 1) teilen sich ein Via unter dem Gehaeuse
    p1 = P('U1', '1')
    v33 = (p64[0] - 0.35, p1[1] - 0.3)
    T('+3V3', [p64, (p64[0], p64[1] + 0.85), v33], 0.3)
    T('+3V3', [p1, (p1[0] + 0.9, p1[1]), v33], 0.3)
    V('+3V3', *v33)
    c1p1, c1p2 = P('C1', '1'), P('C1', '2')
    T('+3V3', [(p1[0] - 0.5, p1[1]), (c1p1[0], p1[1] - 0.6), c1p1], 0.3)
    T('GND', [c1p2, (c1p2[0] - 0.85, c1p2[1] - 0.5)], 0.3)
    V('GND', c1p2[0] - 0.85, c1p2[1] - 0.5)

    # VDD19 -> C2, eigenes Via nach innen; VSS18 nach innen
    p19, p18 = P('U1', '19'), P('U1', '18')
    c2p1 = P('C2', '1')
    T('+3V3', [p19, (p19[0], c2p1[1] - 0.4), (c2p1[0] + 0.15, c2p1[1])], 0.3)
    T('+3V3', [p19, (p19[0], p19[1] - 2.4)], 0.3)
    V('+3V3', p19[0], p19[1] - 2.4)
    T('GND', [p18, (p18[0], p18[1] - 0.7), (p18[0] - 0.45, p18[1] - 2.275)], 0.3)
    V('GND', p18[0] - 0.45, p18[1] - 2.275)

    # VDD32 -> C3, VCAP1 -> C9
    p32, p31 = P('U1', '32'), P('U1', '31')
    c3p1, c9p1 = P('C3', '1'), P('C9', '1')
    T('+3V3', [p32, (p32[0], c3p1[1] - 0.45), c3p1], 0.3)
    T('+3V3', [p32, (p32[0], p32[1] - 2.4)], 0.3)
    V('+3V3', p32[0], p32[1] - 2.4)
    T('VCAP1', [p31, (p31[0], c9p1[1] - 0.45), c9p1], 0.25)

    # VDD48 -> C4, VCAP2 -> C10
    p48, p47 = P('U1', '48'), P('U1', '47')
    c4p1, c10p1 = P('C4', '1'), P('C10', '1')
    T('+3V3', [p48, (p48[0] + 1.0, p48[1]), (c4p1[0], p48[1] - 0.35), c4p1], 0.3)
    T('+3V3', [p48, (p48[0] - 1.2, p48[1])], 0.3)
    V('+3V3', p48[0] - 1.2, p48[1])
    T('VCAP2', [p47, (p47[0] + 1.175, p47[1]), (c10p1[0] - 0.4, p47[1] - 0.85),
                (c10p1[0], p47[1] - 0.85), c10p1], 0.25)

    # VSSA (Pin 12) nach innen
    p12 = P('U1', '12')
    T('GND', [p12, (p12[0] + 1.6, p12[1])], 0.3)
    V('GND', p12[0] + 1.6, p12[1])
    # VDDA (Pin 13): nach innen, Via, auf der Unterseite zum Filter (FB1, C7, C8)
    p13 = P('U1', '13')
    va = (p13[0] + 1.325, p13[1] + 0.5)
    T('+3V3A', [p13, (p13[0] + 0.875, p13[1]), va], 0.3)
    V('+3V3A', *va)
    c7p1, c8p1, fb1p2 = P('C7', '1'), P('C8', '1'), P('FB1', '2')
    vb = (c7p1[0] - 0.82, c7p1[1] + 0.7)
    T('+3V3A', [va, (c7p1[0] - 0.12, va[1]), (vb[0], va[1] + 0.7), vb], 0.3, layer=B_CU)
    V('+3V3A', *vb)
    T('+3V3A', [vb, c7p1], 0.3)
    T('+3V3A', [vb, fb1p2], 0.3)
    T('+3V3A', [c7p1, c8p1], 0.3)

    # ------------------------------------------------ Gyro
    p8, p5, p6, p7 = P('U2', '8'), P('U2', '5'), P('U2', '6'), P('U2', '7')
    p10, p11 = P('U2', '10'), P('U2', '11')
    c16p1, c16p2 = P('C16', '1'), P('C16', '2')
    c17p1, c17p2 = P('C17', '1'), P('C17', '2')
    c18p1 = P('C18', '1')
    T('+3V3', [p8, c16p1], 0.25)
    T('+3V3', [c17p1, c16p1], 0.25)
    T('+3V3', [c17p1, (c17p1[0], c17p1[1] - 0.95)], 0.3)
    V('+3V3', c17p1[0], c17p1[1] - 0.95)
    T('GND', [c16p2, c17p2], 0.25)
    T('GND', [c17p2, (c17p2[0] - 0.9, c17p2[1] + 0.5)], 0.3)
    V('GND', c17p2[0] - 0.9, c17p2[1] + 0.5)
    T('+3V3', [p5, (p5[0], p5[1] - 0.65), (c18p1[0], c18p1[1] + 0.1)], 0.25)
    T('+3V3', [c18p1, (c18p1[0] + 0.95, c18p1[1])], 0.3)
    V('+3V3', c18p1[0] + 0.95, c18p1[1])
    gv = ((p6[0] + p7[0]) / 2, p6[1] - 1.45)
    T('GND', [p7, (p7[0], p7[1] - 0.6), gv], 0.25)
    T('GND', [p6, (p6[0], p6[1] - 0.6), gv], 0.25)
    V('GND', *gv)
    gw = (p10[0] - 1.3, (p10[1] + p11[1]) / 2)
    T('GND', [p10, (p10[0] - 0.5, p10[1]), (gw[0] + 0.5, gw[1]), gw], 0.25)
    T('GND', [p11, (p11[0] - 0.5, p11[1]), (gw[0] + 0.5, gw[1])], 0.25)
    V('GND', *gw)
    # SPI-Buendel Gyro -> Prozessor. Reihenfolge (West -> Ost) am Gyro: CS, SCK, MOSI, MISO,
    # am Prozessor: CS, MOSI, MISO, SCK. CS/MOSI/MISO laufen kreuzungsfrei auf F.Cu, SCK
    # wechselt direkt unterhalb der Via-Sperrflaeche auf B.Cu und kommt innerhalb des
    # Pinrings des Prozessors wieder hoch.
    g_cs, g_sck, g_mosi, g_miso = P('U2', '12'), P('U2', '13'), P('U2', '14'), P('U2', '1')
    m_cs, m_mosi, m_miso, m_sck = P('U1', '58'), P('U1', '57'), P('U1', '56'), P('U1', '55')
    T('GYRO_CS', [g_cs, (g_cs[0], -0.8), (g_cs[0] - 0.15, -0.65), (g_cs[0] - 0.15, 1.0),
                  (m_cs[0], 1.1), m_cs], 0.2)
    T('SPI1_MOSI', [g_mosi, (g_mosi[0], -0.8), (g_mosi[0] + 0.15, -0.65), (g_mosi[0] + 0.15, 0.1),
                    (g_mosi[0] - 0.15, 0.4), (m_mosi[0] + 0.2, 0.4), (m_mosi[0], 0.6), m_mosi], 0.2)
    T('SPI1_MISO', [g_miso, (g_miso[0], 0.45), (m_miso[0], 0.45 + (g_miso[0] - m_miso[0])), m_miso], 0.2)
    # CS zusaetzlich nach innen auf ein Via (Anschluss des Pull-ups R7 ueber B.Cu)
    T('GYRO_CS', [m_cs, (m_cs[0], 3.6)], 0.2)
    V('GYRO_CS', m_cs[0], 3.6)
    sv1, sv2 = (g_sck[0], -0.25), (m_sck[0], 3.6)
    T('SPI1_SCK', [g_sck, sv1], 0.2)
    V('SPI1_SCK', *sv1)
    T('SPI1_SCK', [sv1, sv2], 0.2, layer=B_CU)
    V('SPI1_SCK', *sv2)
    T('SPI1_SCK', [sv2, m_sck], 0.2)

    # ------------------------------------------------ Flash
    u3p8, c20p1 = P('U3', '8'), P('C20', '1')
    T('+3V3', [u3p8, c20p1], 0.3)
    T('+3V3', [c20p1, (c20p1[0], c20p1[1] - 0.85)], 0.3)
    V('+3V3', c20p1[0], c20p1[1] - 0.85)

    # ------------------------------------------------ 5-V-Schaltregler (heisse Schleife kurz)
    q1, q2, q3 = P('U4', '1'), P('U4', '2'), P('U4', '3')
    q5, q6, q7, q8 = P('U4', '5'), P('U4', '6'), P('U4', '7'), P('U4', '8')
    c23p1, c23p2 = P('C23', '1'), P('C23', '2')
    T('VBAT', [q2, (q2[0], c23p1[1] + 0.35), c23p1], 0.4)
    T('GND', [q1, (q1[0], c23p2[1] + 0.35), c23p2], 0.4)
    T('VBAT', [q3, (q3[0], q3[1] - 0.95), (q2[0], q2[1] - 0.95)], 0.4)
    # Via zwischen den Montagepads der Buchsen J9/J5 auf der Unterseite
    T('VBAT', [(q3[0], q3[1] - 0.95), (q3[0] + 0.185, q3[1] - 1.5)], 0.4)
    V('VBAT', q3[0] + 0.185, q3[1] - 1.5)
    T('GND', [c23p2, (c23p2[0] + 2.75, c23p2[1] + 0.2)], 0.4)
    V('GND', c23p2[0] + 2.75, c23p2[1] + 0.2)
    l1p1 = P('L1', '1')
    c24p1, c24p2 = P('C24', '1'), P('C24', '2')
    T('BUCK_SW', [q8, (q8[0], q8[1] + 0.7), (q8[0] + 0.9, l1p1[1] - 0.6), (l1p1[0], l1p1[1] - 0.6)], 0.8)
    T('BUCK_SW', [(q8[0], q8[1] + 0.7), c24p2], 0.4)
    T('BUCK_BOOT', [q7, (q7[0], q7[1] + 1.1), c24p1], 0.3)
    c25p1 = P('C25', '1')
    T('BUCK_VCC', [q6, (q6[0], c25p1[1])], 0.3)
    r11p1, r10p2 = P('R11', '1'), P('R10', '2')
    T('BUCK_FB', [q5, (q5[0], q5[1] + 1.1), (r11p1[0] + 0.1, r11p1[1] - 0.1)], 0.25)
    T('BUCK_FB', [r11p1, r10p2], 0.25)
    # Waermevias im Exposed Pad (nur dort, wo unten keine Steckverbinderpads liegen)
    import fanout
    fanout.thermal_vias(bd, 'U4', '9', 'GND')

    # ------------------------------------------------ 3,3-V-Ausgang AMS1117 in die Flaeche
    tab = [p for p in bd.fps['U5'].Pads() if p.GetNumber() == '2']
    tab_pos = max((bd.pad_pos_obj(p) for p in tab), key=lambda c: -c[0])  # westliches Pad
    T('+3V3', [tab_pos, (7.6, tab_pos[1] - 0.1)], 0.5)
    V('+3V3', 7.6, tab_pos[1] - 0.1)
    V('+3V3', 7.6, tab_pos[1] - 0.95)
    T('+3V3', [(7.6, tab_pos[1] - 0.1), (7.6, tab_pos[1] - 0.95)], 0.5)

    # ------------------------------------------------ Akkuteiler R4 direkt am Elko-Pluspol
    r4p1 = P('R4', '1')
    T('VBAT', [r4p1, (r4p1[0], r4p1[1] - 1.45)], 0.4)

    # ------------------------------------------------ LED-Vorwiderstaende: 3,3 V
    r14, r15, r16 = P('R14', '1'), P('R15', '1'), P('R16', '1')
    T('+3V3', [r14, r16], 0.3)
    T('+3V3', [r15, (-11.9, r15[1])], 0.3)
    V('+3V3', -11.9, r15[1])

    steckverbinder(bd)

    # ------------------------------------------------ 5 V fuer den Rueckkopplungsteiler
    r10p1, l1p2 = P('R10', '1'), P('L1', '2')
    T('+5V', [r10p1, (r10p1[0], -8.45), (l1p2[0], -8.45), (l1p2[0], -9.3)], 0.4)

    # ------------------------------------------------ Verpolschutz: Source an VBAT (Zenerdiode)
    q1s, d1k = P('Q1', '2'), P('D1', '1')
    T('VBAT', [q1s, (q1s[0] - 0.36, 18.7), (d1k[0], 18.7), d1k], 0.4)

    # ------------------------------------------------ Rueckkopplungsteiler: Masse ueber C25
    r11p2, c25p2 = P('R11', '2'), P('C25', '2')
    T('GND', [r11p2, (r11p2[0] + 0.5, r11p2[1] + 0.5), c25p2], 0.25)

    # ------------------------------------------------ AMS1117: Pin 2 mit der Kuehlfahne verbinden
    u5p = [bd.pad_pos_obj(p) for p in bd.fps['U5'].Pads() if p.GetNumber() == '2']
    u5p.sort()
    T('+3V3', [u5p[0], u5p[-1]], 0.6)

    # ------------------------------------------------ 5-V-Ausgang: L1 -> C26
    c26p1 = P('C26', '1')
    T('+5V', [l1p2, (l1p2[0] + 0.6, c26p1[1]), c26p1], 0.8)

    # ------------------------------------------------ USB D+/D-: Prozessor -> USBLC6
    # D- muss westlich am GND-Pin des USBLC6 vorbei (zwischen Pad-Enden des Prozessors und
    # USBLC6 bleiben 0,52 mm: 0,2-mm-Bahn mit je 0,16 mm Abstand)
    dp, dm = P('U1', '45'), P('U1', '44')
    u6p1, u6p3 = P('U6', '1'), P('U6', '3')
    T('USB_DP', [dp, (8.3, dp[1]), (8.6, u6p1[1] + 0.1), u6p1], 0.2)
    xg = u6p3[0] - 0.66 - 0.26
    T('USB_DM', [dm, (xg, dm[1]), (xg, u6p3[1] - 0.2), (xg + 0.2, u6p3[1]), u6p3], 0.2)

    # ELRS (USART1, Pin 42/43): oestlich liegt D-, daher nach innen auf Vias im Pinring
    e_tx, e_rx = P('U1', '42'), P('U1', '43')
    T('ELRS_RX', [e_rx, (5.85, e_rx[1])], 0.2)
    V('ELRS_RX', 5.85, e_rx[1])
    T('ELRS_TX', [e_tx, (6.15, e_tx[1]), (5.25, e_tx[1] + 0.3)], 0.2)
    V('ELRS_TX', 5.25, e_tx[1] + 0.3)

    # ------------------------------------------------ USBLC6: GND-Via mittig unter dem Gehaeuse
    u6p2, u6p5 = P('U6', '2'), P('U6', '5')
    xm = (u6p2[0] + u6p5[0]) / 2
    T('GND', [u6p2, (xm, u6p2[1])], 0.25)
    V('GND', xm, u6p2[1])
    # VBUS: Buchse (Pads A4/B9) -> SS14-Anode
    vb_pad = [bd.pad_pos_obj(p) for p in bd.fps['J1'].Pads() if p.GetNumber() == 'A4'][0]
    d2a = P('D2', '2')
    T('VBUS', [vb_pad, (11.3, vb_pad[1]), (d2a[0] + 0.6, d2a[1] - 0.6)], 0.5)
    # Zweites VBUS-Pad (B4/A9): je ein Via neben beiden VBUS-Pads (zwischen Pad-Ende und
    # Fuehrungsstift, unter dem Buchsengehaeuse), Verbindung auf B.Cu
    vn, vs = P('J1', 'A9'), P('J1', 'A4')
    via_n, via_s = (15.0, vn[1] + 0.45), (15.0, vs[1] - 0.45)
    T('VBUS', [vn, (vn[0] + 0.8, vn[1]), via_n], 0.4)
    T('VBUS', [vs, (vs[0] + 0.8, vs[1]), via_s], 0.4)
    V('VBUS', *via_n)
    V('VBUS', *via_s)
    T('VBUS', [via_n, via_s], 0.5, layer=B_CU)
    # VBUS-Kondensatoren C33/C34 direkt am noerdlichen VBUS-Pad (westlich, an R13 und dem
    # GND-Pad B1 vorbei)
    c33v, c34v = P('C33', '1'), P('C34', '1')
    T('VBUS', [vn, (13.0, vn[1]), (12.75, vn[1] - 0.35), (12.75, 1.75), (12.3, 1.3), c33v, c34v], 0.3)
    # CC1 (A5): Via zwischen USBLC6 und Buchse, auf B.Cu ostwaerts um den VTX-Stecker herum
    # nach Norden, Via neben R12
    cc1, r12 = P('J1', 'A5'), P('R12', '1')
    v1, v2 = (12.55, cc1[1] + 0.45), (r12[0] - 0.74, r12[1] + 0.1)
    T('USB_CC1', [cc1, (12.9, cc1[1]), v1], 0.2)
    V('USB_CC1', *v1)
    T('USB_CC1', [v1, (13.72, v1[1]), (13.72, v2[1]), v2], 0.2, layer=B_CU)
    V('USB_CC1', *v2)
    T('USB_CC1', [v2, r12], 0.2)
    # CC2 (B5): direkt nach Westen zu R13
    cc2, r13 = P('J1', 'B5'), P('R13', '1')
    T('USB_CC2', [cc2, (12.7, cc2[1]), r13], 0.2)

    # ------------------------------------------------ Akkuteiler: Masse von C14 ueber R5
    T('GND', [P('C14', '2'), P('R5', '2')], 0.3)

    # ------------------------------------------------ VDDA-Ferritperle: 3,3-V-Via
    fb1p1 = P('FB1', '1')
    T('+3V3', [fb1p1, (fb1p1[0], fb1p1[1] - 0.97)], 0.3)
    V('+3V3', fb1p1[0], fb1p1[1] - 0.97)

    # ------------------------------------------------ Motorsignale M3/M4 (kreuzen sich): kurz
    # senkrecht vom Prozessor nach Sueden, je ein Via, unten in den ESC-Stecker
    m3, m4 = P('U1', '22'), P('U1', '23')
    j2m3, j2m4 = P('J2', '7'), P('J2', '8')
    T('M4', [m4, (m4[0], 16.43)], 0.25)
    V('M4', m4[0], 16.43)
    T('M4', [(m4[0], 16.43), (m4[0], j2m4[1] + 0.25)], 0.25, layer=B_CU)
    T('M3', [m3, (m3[0], 16.11), (m3[0] - 0.08, 16.19), (m3[0] - 0.08, 17.2)], 0.25)
    V('M3', m3[0] - 0.08, 17.2)
    T('M3', [(m3[0] - 0.08, 17.2), (0.8, 17.2), (j2m3[0] - 0.17, 16.67),
             (j2m3[0] - 0.17, j2m3[1] + 0.17)], 0.25, layer=B_CU)

    # ------------------------------------------------ Akkueingang ESC -> Verpolschutz (Drain)
    j2p1, q1d = P('J2', '1'), P('Q1', '3')
    T('VBAT_IN', [j2p1, (j2p1[0], j2p1[1] - 1.45)], 0.6, layer=B_CU)
    V('VBAT_IN', j2p1[0], j2p1[1] - 1.45)
    T('VBAT_IN', [(j2p1[0], j2p1[1] - 1.45), (j2p1[0] + 0.5, 15.5), (q1d[0] + 0.36, 15.5),
                  (q1d[0] + 0.36, q1d[1] - 0.3), q1d], 0.8)


def steckverbinder(bd):
    """Versorgungspins der JST-SH-Buchsen (Unterseite), deren Pads ausserhalb der passenden
    Innenlagen-Flaeche liegen: kurze Bahn auf B.Cu zu einem Via im richtigen Bereich."""
    P = bd.pad_pos
    T = bd.track
    V = bd.via
    # I2C (Nordrand West), 3,3 V: zum Testpunkt TP1 (3,3 V, hat sein Via in der 3,3-V-Flaeche)
    j4, tp1 = P('J4', '3'), P('TP1', '1')
    T('+3V3', [j4, (j4[0], tp1[1] - 1.0), (tp1[0] - 0.7, tp1[1])], 0.3, layer=B_CU)
    # UART (Nordrand Mitte), 3,3 V: nach Sueden unter den Elko (Via in der 3,3-V-Flaeche)
    j9 = P('J9', '3')
    T('+3V3', [j9, (j9[0] - 0.2, j9[1] + 1.2), (j9[0] - 0.2, -11.2)], 0.3, layer=B_CU)
    V('+3V3', j9[0] - 0.2, -11.2)
    # UART, Masse: zum naechsten Waermevia des Schaltreglers
    j9g = P('J9', '4')
    gv = min((pcbnew.ToMM(t.GetPosition().x) - 100.0, pcbnew.ToMM(t.GetPosition().y) - 100.0)
             for t in bd.board.GetTracks()
             if t.Type() == pcbnew.PCB_VIA_T and t.GetNetname() == 'GND'
             and abs(pcbnew.ToMM(t.GetPosition().y) - 100.0 - j9g[1]) < 0.3
             and pcbnew.ToMM(t.GetPosition().x) - 100.0 > j9g[0])
    T('GND', [j9g, gv], 0.3, layer=B_CU)
    # GPS (Nordrand Ost), 5 V: unter dem Schaltregler-Ausgang nach Suedosten in die 5-V-Flaeche
    j5 = P('J5', '3')
    T('+5V', [j5, (j5[0], -12.6), (9.6, -11.6), (11.8, -11.6)], 0.4, layer=B_CU)
    V('+5V', 11.8, -11.6)
    # GPIO (Westrand), 3,3 V: nach Osten in die 3,3-V-Flaeche
    j10 = P('J10', '7')
    T('+3V3', [j10, (-12.8, j10[1]), (-12.0, j10[1] - 0.8)], 0.3, layer=B_CU)
    V('+3V3', -12.0, j10[1] - 0.8)
    # Piepser (innen West), 5 V: nach Westen in die 5-V-Flaeche
    j11 = P('J11', '1')
    T('+5V', [j11, (-13.5, j11[1]), (-14.0, j11[1] + 0.5)], 0.4, layer=B_CU)
    V('+5V', -14.0, j11[1] + 0.5)
