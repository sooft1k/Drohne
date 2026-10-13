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
    T('+3V3', [p19, (p19[0], p19[1] - 1.7)], 0.3)
    V('+3V3', p19[0], p19[1] - 1.7)
    T('GND', [p18, (p18[0], p18[1] - 0.7), (p18[0] - 0.45, p18[1] - 2.275)], 0.3)
    V('GND', p18[0] - 0.45, p18[1] - 2.275)

    # VDD32 -> C3, VCAP1 -> C9
    p32, p31 = P('U1', '32'), P('U1', '31')
    c3p1, c9p1 = P('C3', '1'), P('C9', '1')
    T('+3V3', [p32, (p32[0], c3p1[1] - 0.45), c3p1], 0.3)
    T('+3V3', [p32, (p32[0], p32[1] - 1.7)], 0.3)
    V('+3V3', p32[0], p32[1] - 1.7)
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
    vb = ((c7p1[0] + fb1p2[0]) / 2, c7p1[1])
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
    gv = ((p6[0] + p7[0]) / 2, p6[1] - 1.45)
    T('GND', [p7, (p7[0], p7[1] - 0.6), gv], 0.25)
    T('GND', [p6, (p6[0], p6[1] - 0.6), gv], 0.25)
    V('GND', *gv)
    gw = (p10[0] - 1.3, (p10[1] + p11[1]) / 2)
    T('GND', [p10, (p10[0] - 0.5, p10[1]), (gw[0] + 0.5, gw[1]), gw], 0.25)
    T('GND', [p11, (p11[0] - 0.5, p11[1]), (gw[0] + 0.5, gw[1])], 0.25)
    V('GND', *gw)

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
    T('VBAT', [(q3[0], q3[1] - 0.95), (q3[0] + 0.535, q3[1] - 1.5)], 0.4)
    V('VBAT', q3[0] + 0.535, q3[1] - 1.5)
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

    # ------------------------------------------------ 3,3 V an Stiftleisten ausserhalb der 3,3-V-Flaeche
    j10 = P('J10', '7')
    T('+3V3', [j10, (-13.0, j10[1]), (-11.9, 4.6)], 0.3, layer=B_CU)
    V('+3V3', -11.9, 4.6)
    j4 = P('J4', '3')
    tp1 = P('TP1', '1')
    T('+3V3', [j4, (j4[0], tp1[1] - 1.0), tp1], 0.3, layer=B_CU)
    j9 = P('J9', '3')
    T('+3V3', [j9, (j9[0], -13.0), (6.6, -11.6), (6.6, -9.4)], 0.3, layer=B_CU)
    V('+3V3', 6.6, -9.4)

    # ------------------------------------------------ 5 V fuer den Rueckkopplungsteiler
    r10p1, l1p2 = P('R10', '1'), P('L1', '2')
    T('+5V', [r10p1, (r10p1[0], -8.45), (l1p2[0], -8.45), (l1p2[0], -9.3)], 0.4)
