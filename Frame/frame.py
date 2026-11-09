"""Parametrischer 5-Zoll-Frame (True-X) fuer die Drohne.

Erzeugt die Teile als STL (Druck) und STEP (Ansicht im CAD). Mit MATERIAL = "carbon"
werden die Platten duenner und zusaetzlich als DXF zum Fraesen ausgegeben. Die Lage
aller Bohrungen ist in beiden Varianten gleich.

Koordinaten in mm: x nach vorne, y nach links, z nach oben, Ursprung in der Mitte
des Stacks auf der Unterseite der Bodenplatte.

Aufruf: python frame.py   (braucht cadquery, fuer DXF zusaetzlich ezdxf)
"""
import math
from pathlib import Path

import cadquery as cq

MATERIAL = "petg"  # "petg" oder "carbon"

# Plattenstaerken je Material. PETG ist weicher, deshalb dicker.
THICKNESS = {
    "petg": {"bottom": 6.0, "top": 4.0},
    "carbon": {"bottom": 5.0, "top": 2.0},
}
# Gedruckte Loecher fallen etwas kleiner aus als konstruiert.
HOLE_EXTRA = {"petg": 0.2, "carbon": 0.0}

# Motoren und Propeller: iFlight XING-E 2207, Nazgul 5140
WHEELBASE = 225.0  # Motor zu Motor diagonal
MOTOR_HOLE_SPACING = 16.0  # Lochbild 16 x 16, M3
MOTOR_D = 28.5
MOTOR_H = 19.7
MOTOR_SHAFT_HOLE = 9.0  # Freiraum fuer Welle und Sicherungsring unter dem Motor
PROP_R = 65.0  # 5,1 Zoll
PROP_THICK = 8.0  # Hoehe des Propellers ueber der Motorglocke
PROP_GAP = 3.0  # Mindestabstand Propellerspitze zu Rahmenteilen ueber den Armen

# Arme
PAD_D = 32.0  # Auflage unter dem Motor
ARM_ROOT_W = 24.0
ARM_TIP_W = 16.0
WIRE_SLOT_R = 55.0  # Kabelbinderschlitze fuer die Motorkabel, Abstand von der Mitte

# Stack: ESC B-Cube 4in1 und eigene Platine, beide 42 x 41 mm, 30,5 x 30,5 M3
STACK_HOLE_SPACING = 30.5
STACK_L, STACK_W = 42.0, 41.0

# Mittelteil der Bodenplatte
BODY_FRONT, BODY_REAR, BODY_HALF_W = 56.0, -66.0, 23.0
BODY_CORNER_R = 6.0

# Deckplatte, wird ueber den Propellern zusaetzlich freigeschnitten
TOP_FRONT, TOP_REAR, TOP_HALF_W = 50.0, -76.0, 33.0
STANDOFF_H = 30.0  # Abstandsbolzen M3 x 30 (Alu kaufen, nicht drucken)
STANDOFFS = [(30.0, 19.0), (30.0, -19.0), (-56.0, 12.0), (-56.0, -12.0)]

# Akku CNHL 6S 1300 mAh, laengs auf der Deckplatte, zwei 20-mm-Gurte
BATTERY_L, BATTERY_W, BATTERY_H = 77.0, 48.0, 33.0
STRAP_SLOT_X = 18.0
STRAP_SLOT_Y = 27.0
STRAP_SLOT_LEN = 25.0
STRAP_SLOT_W = 3.5

# VTX hinten auf der Bodenplatte, Lochbild 20 x 20, M2
VTX_X = -36.0
VTX_HOLE_SPACING = 20.0

# Fenster fuer den nach unten schauenden VL53L1X (x, y, Laenge, Breite)
SENSOR_WINDOW = (28.5, 0.0, 10.0, 14.0)

# Kabelbinderschlitze fuer den ELRS-Empfaenger hinten
ELRS_SLOTS = [(-62.0, 8.0), (-62.0, -8.0)]

# Kamerahalterung vorne fuer 19-mm-Kameras
CAM_W = 19.0
CAM_H = 19.0
CAGE_X = 46.0  # Mitte der Halterung
CAGE_L = 20.0
CAGE_WALL = 3.0
CAGE_TAB = 8.0  # seitliche Laschen mit den M3-Schrauben
CAGE_BASE_T = 3.0
CAGE_H = 26.0
CAGE_PIVOT_X = 1.0  # relativ zur Mitte der Halterung
CAGE_PIVOT_Z = CAGE_BASE_T + 13.5  # hoch genug, damit die gekippte Kamera nicht aufsetzt

# GPS-Mast hinten auf der Deckplatte, Plattform fuer M10-Module bis 25 x 25 mm
GPS_X = -68.0
GPS_HOLE_Y = 6.0
GPS_BASE = 24.0
GPS_BASE_T = 3.0
GPS_WALL = 3.0
GPS_MAST_H = 20.0
GPS_PLATFORM = 27.0
GPS_PLATFORM_T = 2.5

OUT_DIR = Path(__file__).resolve().parent
PETG_DENSITY = 1.27  # g/cm3, gilt fuer 100 % Fuellung


def motor_positions():
    r = WHEELBASE / 2
    return [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a)), a) for a in (45, 135, 225, 315)]


def rotate(x, y, angle_deg):
    a = math.radians(angle_deg)
    return x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)


def cylinders(points, d, h=100.0, z0=-50.0):
    return cq.Workplane().workplane(offset=z0).pushPoints(points).circle(d / 2).extrude(h)


def slots(points, length, width, angle=0.0, h=100.0, z0=-50.0):
    wp = cq.Workplane().workplane(offset=z0)
    result = None
    for x, y in points:
        s = wp.center(x, y).slot2D(length, width, angle).extrude(h)
        result = s if result is None else result.union(s)
    return result


def bottom_plate(t, m3, m2):
    r = WHEELBASE / 2
    body = (
        cq.Workplane()
        .center((BODY_FRONT + BODY_REAR) / 2, 0)
        .rect(BODY_FRONT - BODY_REAR, 2 * BODY_HALF_W)
        .extrude(t)
        .edges("|Z")
        .fillet(BODY_CORNER_R)
    )
    plate = body
    motor_holes, shaft_holes, wire_slots = [], [], []
    for mx, my, angle in motor_positions():
        arm = (
            cq.Workplane()
            .polyline([(0, -ARM_ROOT_W / 2), (r, -ARM_TIP_W / 2), (r, ARM_TIP_W / 2), (0, ARM_ROOT_W / 2)])
            .close()
            .extrude(t)
            .union(cq.Workplane().center(r, 0).circle(PAD_D / 2).extrude(t))
            .rotate((0, 0, 0), (0, 0, 1), angle)
        )
        plate = plate.union(arm)
        half = MOTOR_HOLE_SPACING / 2
        motor_holes += [rotate(r + dx, dy, angle) for dx in (-half, half) for dy in (-half, half)]
        shaft_holes.append((mx, my))
        arm_w = ARM_ROOT_W + (ARM_TIP_W - ARM_ROOT_W) * WIRE_SLOT_R / r
        wire_slots.append((angle, [rotate(WIRE_SLOT_R, side * (arm_w / 2 - 3.5), angle) for side in (-1, 1)]))

    half_stack = STACK_HOLE_SPACING / 2
    stack_holes = [(sx * half_stack, sy * half_stack) for sx in (-1, 1) for sy in (-1, 1)]
    half_vtx = VTX_HOLE_SPACING / 2
    vtx_holes = [(VTX_X + sx * half_vtx, sy * half_vtx) for sx in (-1, 1) for sy in (-1, 1)]
    cage_tab_y = CAM_W / 2 + 0.2 + CAGE_WALL + CAGE_TAB / 2
    cage_holes = [(CAGE_X, cage_tab_y), (CAGE_X, -cage_tab_y)]
    wx, wy, wl, ww = SENSOR_WINDOW

    plate = (
        plate.cut(cylinders(motor_holes + stack_holes + STANDOFFS + cage_holes, m3))
        .cut(cylinders(shaft_holes, MOTOR_SHAFT_HOLE))
        .cut(cylinders(vtx_holes, m2))
        .cut(cq.Workplane().workplane(offset=-50).center(wx, wy).rect(wl, ww).extrude(100))
        .cut(slots(ELRS_SLOTS, 4.5, 2.0))
    )
    for angle, points in wire_slots:
        plate = plate.cut(slots(points, 4.5, 2.0, angle))
    return plate


def top_plate(t, m3):
    plate = (
        cq.Workplane()
        .center((TOP_FRONT + TOP_REAR) / 2, 0)
        .rect(TOP_FRONT - TOP_REAR, 2 * TOP_HALF_W)
        .extrude(t)
        .edges("|Z")
        .fillet(6.0)
    )
    prop_clearance = cylinders([(mx, my) for mx, my, _ in motor_positions()], 2 * (PROP_R + PROP_GAP))
    strap_points = [(sx * STRAP_SLOT_X, sy * STRAP_SLOT_Y) for sx in (-1, 1) for sy in (-1, 1)]
    gps_holes = [(GPS_X, GPS_HOLE_Y), (GPS_X, -GPS_HOLE_Y)]
    return (
        plate.cut(prop_clearance)
        .cut(cylinders(STANDOFFS + gps_holes, m3))
        .cut(slots(strap_points, STRAP_SLOT_LEN, STRAP_SLOT_W))
    )


def camera_mount(m2, m3):
    inner = CAM_W + 0.4
    base_w = inner + 2 * CAGE_WALL + 2 * CAGE_TAB
    mount = cq.Workplane().rect(CAGE_L, base_w).extrude(CAGE_BASE_T)
    for side in (-1, 1):
        wall = cq.Workplane().center(0, side * (inner + CAGE_WALL) / 2).rect(CAGE_L, CAGE_WALL).extrude(CAGE_H)
        mount = mount.union(wall)
    pivot = cq.Workplane("XZ").center(CAGE_PIVOT_X, CAGE_PIVOT_Z).circle(m2 / 2).extrude(base_w, both=True)
    tab_y = inner / 2 + CAGE_WALL + CAGE_TAB / 2
    return mount.cut(pivot).cut(cylinders([(0, tab_y), (0, -tab_y)], m3))


def gps_mount(m3):
    mount = cq.Workplane().rect(GPS_BASE, GPS_BASE).extrude(GPS_BASE_T)
    for side in (-1, 1):
        wall = (
            cq.Workplane()
            .center(0, side * (GPS_BASE - GPS_WALL) / 2)
            .rect(GPS_BASE, GPS_WALL)
            .extrude(GPS_BASE_T + GPS_MAST_H)
        )
        mount = mount.union(wall)
    platform = (
        cq.Workplane()
        .workplane(offset=GPS_BASE_T + GPS_MAST_H)
        .rect(GPS_PLATFORM, GPS_PLATFORM)
        .extrude(GPS_PLATFORM_T)
        .edges("|Z")
        .fillet(3.0)
    )
    zip_x = GPS_PLATFORM / 2 - 2.5
    # Schrauben von unten durch die Deckplatte, Muttern seitlich durch den offenen Mast einlegen.
    return (
        mount.union(platform)
        .cut(slots([(zip_x, 0), (-zip_x, 0)], 8.0, 2.2, 90))
        .cut(cylinders([(0, GPS_HOLE_Y), (0, -GPS_HOLE_Y)], m3, h=GPS_BASE_T + 2, z0=-1))
    )


def placeholders(t_bottom, top_z):
    """Kaufteile als einfache Koerper, nur fuer die Kollisionspruefung und die Ansicht."""
    motors = cylinders([(mx, my) for mx, my, _ in motor_positions()], MOTOR_D, MOTOR_H, t_bottom)
    props = cylinders([(mx, my) for mx, my, _ in motor_positions()], 2 * PROP_R, PROP_THICK, t_bottom + MOTOR_H)
    stack = cq.Workplane().workplane(offset=t_bottom + 4).rect(STACK_L, STACK_W).extrude(16)
    battery = cq.Workplane().workplane(offset=top_z).rect(BATTERY_L, BATTERY_W).extrude(BATTERY_H)
    camera = (
        cq.Workplane()
        .workplane(offset=t_bottom + CAGE_PIVOT_Z - CAM_H / 2)
        .center(CAGE_X + CAGE_PIVOT_X, 0)
        .rect(CAM_W, CAM_W)
        .extrude(CAM_H)
    )
    return {"motors": motors, "props": props, "stack": stack, "battery": battery, "camera": camera}


def overlap(a, b):
    try:
        return a.intersect(b).val().Volume()
    except Exception:
        return 0.0


def main():
    t = THICKNESS[MATERIAL]
    m3 = 3.2 + HOLE_EXTRA[MATERIAL]
    m2 = 2.2 + HOLE_EXTRA[MATERIAL]

    parts = {
        "bottom_plate": bottom_plate(t["bottom"], m3, m2),
        "top_plate": top_plate(t["top"], m3),
        "camera_mount": camera_mount(m2, m3),
        "gps_mount": gps_mount(m3),
    }

    top_z = t["bottom"] + STANDOFF_H
    placed = {
        "bottom_plate": parts["bottom_plate"],
        "top_plate": parts["top_plate"].translate((0, 0, top_z)),
        "camera_mount": parts["camera_mount"].translate((CAGE_X, 0, t["bottom"])),
        "gps_mount": parts["gps_mount"].translate((GPS_X, 0, top_z + t["top"])),
    }
    dummies = placeholders(t["bottom"], top_z + t["top"])

    print(f"Material: {MATERIAL}")
    for name, part in parts.items():
        bb = part.val().BoundingBox()
        line = f"  {name:13s} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:5.1f} mm"
        if MATERIAL == "petg":
            line += f", ca. {part.val().Volume() / 1000 * PETG_DENSITY:5.1f} g bei 100 % Fuellung"
        print(line)
        cq.exporters.export(part, str(OUT_DIR / f"{name}.stl"), tolerance=0.05, angularTolerance=0.1)

    problems = []
    for name in ("top_plate", "camera_mount", "gps_mount"):
        if overlap(placed[name], dummies["props"]) > 0.01:
            problems.append(f"Propeller beruehren {name}")
    for name in ("battery", "camera", "stack"):
        if overlap(dummies[name], dummies["props"]) > 0.01:
            problems.append(f"Propeller beruehren {name}")
    if overlap(dummies["stack"], placed["top_plate"]) > 0.01:
        problems.append("Stack beruehrt die Deckplatte")
    if overlap(dummies["camera"], placed["top_plate"]) > 0.01:
        problems.append("Kamera beruehrt die Deckplatte")
    print("Kollisionspruefung:", "; ".join(problems) if problems else "keine Beruehrungen")

    assembly = cq.Assembly(name="frame")
    for name, part in placed.items():
        assembly.add(part, name=name, color=cq.Color(0.15, 0.15, 0.15))
    for name, part in dummies.items():
        assembly.add(part, name=name, color=cq.Color(0.8, 0.3, 0.2, 0.4))
    assembly.export(str(OUT_DIR / "frame_assembly.step"))

    if MATERIAL == "carbon":
        for name in ("bottom_plate", "top_plate"):
            section = parts[name].section(t["bottom" if name == "bottom_plate" else "top"] / 2)
            # Neue Workplane im Ursprung, sonst verschiebt center() die DXF-Koordinaten.
            cq.exporters.export(cq.Workplane().add(section.val()), str(OUT_DIR / f"{name}.dxf"))


if __name__ == "__main__":
    main()
