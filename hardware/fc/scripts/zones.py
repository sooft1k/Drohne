# Kupferflaechen
#   Lage 2 (In1.Cu): durchgehend GND
#   Lage 3 (In2.Cu): getrennte Versorgungsflaechen
#       VBAT : Ring am Platinenrand (Nordband, schmaler West- und Suedstreifen),
#              verbindet den Verpolschutz (Suedost) mit Elko und Schaltregler (Nord)
#       +5V  : Ostband, Suedband (unter dem MCU-Suedrand) und Westbereich
#       +3V3 : Mitte (Prozessor, Gyro, Flash)
import pcbnew

W2, H2 = 21.0, 20.5
GAP = 0.4
VB_N = -12.5    # Nordband VBAT bis v = -12.5
VB_NE = 12.2    # ... nach Osten bis u = 12.2
VB_WS = -18.5   # Weststreifen VBAT bis u = -18.5
VB_SS = 18.5    # Suedstreifen VBAT ab v = 18.5
C33_W = -12.8   # 3,3-V-Bereich: u von -12.8 ...
C33_E = 8.4     # ... bis 8.4
C33_S = 11.9    # ... und v bis 11.9 (darunter 5-V-Korridor Sued)


def polys():
    g = GAP / 2
    vbat = [(-W2, -H2), (VB_NE, -H2), (VB_NE, VB_N - g), (VB_WS + g, VB_N - g),
            (VB_WS + g, VB_SS - g), (W2, VB_SS - g), (W2, H2), (-W2, H2)]
    p5 = [(VB_NE + g, -H2), (W2, -H2), (W2, VB_SS - 3 * g), (VB_WS + 3 * g, VB_SS - 3 * g),
          (VB_WS + 3 * g, VB_N + g), (C33_W - g, VB_N + g), (C33_W - g, C33_S + g),
          (C33_E + g, C33_S + g), (C33_E + g, VB_N + g), (VB_NE + g, VB_N + g)]
    p33 = [(C33_W + g, VB_N + g), (C33_E - g, VB_N + g), (C33_E - g, C33_S - g),
           (C33_W + g, C33_S - g)]
    return {'VBAT': vbat, '+5V': p5, '+3V3': p33}


def apply(bd):
    full = [(-W2, -H2), (W2, -H2), (W2, H2), (-W2, H2)]
    bd.zone('GND', pcbnew.In1_Cu, full, priority=0, name='GND L2')
    for net, poly in polys().items():
        bd.zone(net, pcbnew.In2_Cu, poly, priority=1, name=net + ' L3')


def outer_gnd(bd):
    """Masseflaechen auf den Aussenlagen (fuellen Restflaechen, nach dem Routing)."""
    full = [(-W2, -H2), (W2, -H2), (W2, H2), (-W2, H2)]
    bd.zone('GND', pcbnew.F_Cu, full, priority=0, name='GND oben', clearance=0.25)
    bd.zone('GND', pcbnew.B_Cu, full, priority=0, name='GND unten', clearance=0.25)
