#!/usr/bin/env python3
"""PCB base del ayModbusConverter MINI: esquema + placa ruteada en KiCad 8.

Placa de 95 x 72 mm atornillada al chasis. El modulo WT32-ETH01 va enchufado
en dos zocalos de 13 pines con el eje largo VERTICAL, de modo que el RJ45 sale
por la pared superior del chasis y la bornera por la inferior. La tapa solo
lleva el agujero del LED de enlace.

Todo se genera desde una unica tabla de componentes y redes, asi que esquema,
netlist, BOM y placa no se pueden desincronizar.
"""
import os, csv, uuid, math

import os as _o
HERE=_o.path.dirname(_o.path.abspath(__file__))
OUT=_o.path.normpath(_o.path.join(HERE,".."))
os.makedirs(OUT, exist_ok=True)
PROJ = "aymc_carrier_min"

def nu(): return str(uuid.uuid4())

# ===================================================================== cotas
BW, BH   = 70.0, 58.0        # placa reducida al minimo practico
HOLE_IN  = 5.0               # retranqueo de los taladros de fijacion
PITCH    = 2.54
ROW_GAP  = 22.86             # separacion REAL entre filas del WT32-ETH01
XA       = 8.0               # columna de la fila A (pines 1..13)
XB       = XA + ROW_GAP      # columna de la fila B (pines 14..26)
Y1       = 10.0              # pin 1, extremo del RJ45

def rowA(pin):               # pines 1..13, de arriba abajo
    return (XA, Y1 + (pin - 1) * PITCH)
def rowB(pin):               # pines 14..26, de abajo arriba
    return (XB, Y1 + 12 * PITCH - (pin - 14) * PITCH)
def modpad(pin):
    return rowA(pin) if pin <= 13 else rowB(pin)

# ============================================================= componentes
# (ref, valor, huella, [(pad, x, y, net)], descripcion)
TERM_X0, TERM_Y = 30.0, 64.0
TERM_P = 5.08

COMPS = []

def add(ref, val, fp, pads, descr=""):
    COMPS.append(dict(ref=ref, val=val, fp=fp, pads=pads, descr=descr))

# --- bornera de 4 terminales, abajo a la derecha, sale por la pared inferior
TY = 51.5
add("J1", "Bornera 4 vias 5.08", "TERM4",
    [(1, 40.0, TY, "RS485_A"), (2, 45.08, TY, "RS485_B"),
     (3, 50.16, TY, "+VIN"),   (4, 55.24, TY, "GND")],
    "A / B / +V(9-24Vdc) / GND")

MODNET = {1:None,2:"GND",3:None,4:None,5:None,6:"P1_DE",7:"P1_RO",8:"P1_DI",
          9:"GND",10:None,11:"GND",12:"+5V",13:None,14:"GND",15:"LED_D",
          16:None,17:None,18:None,19:None,20:None,21:None,22:None,23:"GND",
          24:None,25:None,26:None}
padsA=[(p,)+rowA(p)+(MODNET[p],) for p in range(1,14)]
padsB=[(p-13,)+rowB(p)+(MODNET[p],) for p in range(14,27)]
add("J2","Zocalo hembra 1x13","HDR13",padsA,"Fila A del WT32-ETH01, pines 1-13")
add("J3","Zocalo hembra 1x13","HDR13",padsB,"Fila B del WT32-ETH01, pines 14-26")

# --- alimentacion, arriba a la derecha
add("U1","7805 TO-220","TO220",
    [(1,40.0,12.0,"+VIN"),(2,42.54,12.0,"GND"),(3,45.08,12.0,"+5V")],
    "Regulador +V -> 5V (admite K7805-2000R3)")
add("C2","0.33uF","CER254",[(1,40.0,18.0,"+VIN"),(2,42.54,18.0,"GND")],"Entrada")
add("C1","100uF/25V","ELEC5",[(1,52.0,12.0,"+5V"),(2,57.0,12.0,"GND")],"Salida")
add("C3","100nF","CER254",[(1,56.0,30.0,"+5V"),(2,56.0,32.54,"GND")],"Desacoplo MAX485")

# --- MAX485: RO/DE/DI hacia el modulo, A/B hacia la bornera
add("U2","MAX485 DIP-8","DIP8",
    [(1,40.0,30.0,"RO_RAW"),(2,40.0,32.54,"P1_DE"),
     (3,40.0,35.08,"P1_DE"),(4,40.0,37.62,"P1_DI"),
     (5,47.62,37.62,"GND"),(6,47.62,35.08,"RS485_A"),
     (7,47.62,32.54,"RS485_B"),(8,47.62,30.0,"+5V")],
    "Transceptor RS-485 half duplex")

add("R2","1k","AXIAL254",[(1,40.0,28.0,"RO_RAW"),(2,40.0,25.46,"P1_RO")],"Divisor RO sup.")
add("R3","2k","AXIAL254",[(1,44.0,25.46,"P1_RO"),(2,44.0,23.0,"GND")],"Divisor RO inf.")
add("R1","120R","AXIAL254",
    [(1,52.0,32.54,"RS485_B"),(2,52.0,35.08,"RS485_A")],"Terminacion de linea")
add("R4","1k","AXIAL254",[(1,36.0,43.0,"LED_D"),(2,36.0,45.54,"LED_A")],"Limitador LED")
add("D1","LED 3mm","LED254",[(1,36.0,49.0,"LED_A"),(2,36.0,51.54,"GND")],"Enlace Modbus")

MOUNT=[(HOLE_IN,HOLE_IN),(BW-HOLE_IN,HOLE_IN),
       (HOLE_IN,BH-HOLE_IN),(BW-HOLE_IN,BH-HOLE_IN)]

# =================================================================== rutas
# (net, capa, [(x,y), ...])  -- el cobre de GND lo resuelve el plano
F, B = "F.Cu", "B.Cu"
TRACKS = [
    # alimentacion de entrada
    ("+VIN", B, [(50.16, TY), (50.16, 55.5), (61.0, 55.5), (61.0, 15.0), (40.0, 15.0)]),
    ("+VIN", B, [(40.0, 15.0), (40.0, 18.0)]),
    ("+VIN", B, [(40.0, 15.0), (40.0, 12.0)]),
    # 5 V: regulador, condensador, MAX485, desacoplo y modulo
    ("+5V",  F, [(45.08, 12.0), (52.0, 12.0)]),
    ("+5V",  F, [(52.0, 12.0), (52.0, 26.0), (47.62, 26.0), (47.62, 30.0)]),
    ("+5V",  F, [(52.0, 26.0), (56.0, 26.0), (56.0, 30.0)]),
    ("+5V",  F, [(52.0, 12.0), (52.0, 2.5), (2.5, 2.5), (2.5, 37.94), (XA, 37.94)]),
    # RS-485 hacia la bornera: A cruza a B, asi que A va por la cara inferior
    ("RS485_A", B, [(47.62, 35.08), (50.0, 35.08), (50.0, 44.0), (40.0, 44.0), (40.0, TY)]),
    ("RS485_B", F, [(47.62, 32.54), (45.08, 32.54), (45.08, TY)]),
    ("RS485_A", F, [(52.0, 35.08), (47.62, 35.08)]),
    ("RS485_B", F, [(52.0, 32.54), (47.62, 32.54)]),
    # senales al modulo, cruzando la fila B por los huecos entre pads
    ("P1_DE", F, [(40.0, 32.54), (40.0, 35.08)]),
    ("P1_DE", B, [(40.0, 32.54), (36.0, 32.54), (36.0, 21.43),
                  (29.0, 21.43), (29.0, 22.70), (XA, 22.70)]),
    ("RO_RAW", F, [(40.0, 30.0), (40.0, 28.0)]),
    ("P1_RO", F, [(40.0, 25.46), (44.0, 25.46)]),
    ("P1_RO", F, [(40.0, 25.46), (38.0, 25.46), (38.0, 26.51),
                  (28.0, 26.51), (28.0, 25.24), (XA, 25.24)]),
    ("P1_DI", F, [(40.0, 37.62), (35.0, 37.62), (35.0, 29.05),
                  (27.0, 29.05), (27.0, 27.78), (XA, 27.78)]),
    # LED
    ("LED_D", F, [(XB, 37.94), (28.0, 37.94), (28.0, 43.0), (36.0, 43.0)]),
    ("LED_A", F, [(36.0, 45.54), (36.0, 49.0)]),
]

# ================================================================ geometria
def pads_of(c):
    return c["pads"]

ALLPADS = []
for c in COMPS:
    for (n, x, y, net) in c["pads"]:
        ALLPADS.append((c["ref"], n, x, y, net))

NETS = sorted({p[4] for p in ALLPADS if p[4]} | {t[0] for t in TRACKS})
NETIDX = {n: i + 1 for i, n in enumerate(NETS)}

# huellas: (tipo, diametro de pad, broca, forma del pad 1)
FP = {
    "TERM4":   (2.8, 1.3), "HDR13": (1.6, 1.0), "TO220": (2.2, 1.2),
    "DIP8":    (1.6, 0.9), "ELEC5": (1.8, 1.0), "CER254": (1.6, 0.8),
    "AXIAL254":(1.6, 0.8), "LED254": (1.6, 0.8),
}

# ============================================================ verificacion
def seg_pts(tr):
    return [(tr[2][i], tr[2][i + 1]) for i in range(len(tr[2]) - 1)]

def dist_pt_seg(p, a, b):
    ax, ay = a; bx, by = b; px, py = p
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))

def segs_cross(a, b, c, d):
    def o(p, q, r):
        v = (q[1]-p[1])*(r[0]-q[0]) - (q[0]-p[0])*(r[1]-q[1])
        return 0 if abs(v) < 1e-9 else (1 if v > 0 else 2)
    o1, o2, o3, o4 = o(a,b,c), o(a,b,d), o(c,d,a), o(c,d,b)
    return o1 != o2 and o3 != o4

POWER = ("+5V", "+VIN", "GND")
def wof(net): return 0.8 if net in POWER else 0.25
CLEAR = 0.20
problemas = []

# pads solapados
for i in range(len(ALLPADS)):
    for j in range(i + 1, len(ALLPADS)):
        r1, n1, x1, y1_, net1 = ALLPADS[i]
        r2, n2, x2, y2_, net2 = ALLPADS[j]
        d = math.hypot(x1 - x2, y1_ - y2_)
        rr = (FP[next(c["fp"] for c in COMPS if c["ref"] == r1)][0] +
              FP[next(c["fp"] for c in COMPS if c["ref"] == r2)][0]) / 2.0
        if d < rr + 0.2:
            problemas.append(f"pads muy juntos: {r1}.{n1} y {r2}.{n2} a {d:.2f} mm")

# pistas que pasan sobre pads ajenos
for tr in TRACKS:
    net, layer, pts = tr
    for (ref, n, px, py, pnet) in ALLPADS:
        if pnet == net:
            continue
        rad = FP[next(c["fp"] for c in COMPS if c["ref"] == ref)][0] / 2.0
        for a, b in seg_pts(tr):
            if dist_pt_seg((px, py), a, b) < rad + wof(net) / 2 + CLEAR:
                problemas.append(
                    f"pista {net} ({layer}) roza el pad {ref}.{n} de {pnet}")
                break

# cruces entre pistas de distinta red en la misma capa
for i in range(len(TRACKS)):
    for j in range(i + 1, len(TRACKS)):
        if TRACKS[i][0] == TRACKS[j][0] or TRACKS[i][1] != TRACKS[j][1]:
            continue
        for a, b in seg_pts(TRACKS[i]):
            for c2, d2 in seg_pts(TRACKS[j]):
                if segs_cross(a, b, c2, d2):
                    problemas.append(
                        f"cruce en {TRACKS[i][1]}: {TRACKS[i][0]} con {TRACKS[j][0]}"
                        f" cerca de {a}")

# conectividad: cada pad con red debe tocar una pista o ser GND (plano)
for (ref, n, px, py, pnet) in ALLPADS:
    if not pnet or pnet == "GND":
        continue
    tocado = False
    for tr in TRACKS:
        if tr[0] != pnet:
            continue
        for a, b in seg_pts(tr):
            if dist_pt_seg((px, py), a, b) < 0.4:
                tocado = True; break
        if tocado: break
    if not tocado:
        problemas.append(f"pad sin pista: {ref}.{n} ({pnet})")

# componentes dentro de la placa
for (ref, n, px, py, pnet) in ALLPADS:
    if not (2 <= px <= BW - 2 and 2 <= py <= BH - 2):
        problemas.append(f"pad fuera de la placa: {ref}.{n} en ({px},{py})")

print("=== VERIFICACION DEL RUTEADO ===")
if problemas:
    for p in sorted(set(problemas)):
        print("  [X]", p)
else:
    print("  sin incidencias")
print(f"\ncomponentes {len(COMPS)}  pads {len(ALLPADS)}  redes {len(NETS)}"
      f"  pistas {len(TRACKS)}")
print("redes:", ", ".join(NETS))

# =============================================================== .kicad_pcb
def centroid(pads):
    return (sum(p[1] for p in pads) / len(pads), sum(p[2] for p in pads) / len(pads))

LAYERS = """	(layers
		(0 "F.Cu" signal)
		(31 "B.Cu" signal)
		(32 "B.Adhes" user "B.Adhesive")
		(33 "F.Adhes" user "F.Adhesive")
		(34 "B.Paste" user)
		(35 "F.Paste" user)
		(36 "B.SilkS" user "B.Silkscreen")
		(37 "F.SilkS" user "F.Silkscreen")
		(38 "B.Mask" user)
		(39 "F.Mask" user)
		(40 "Dwgs.User" user "User.Drawings")
		(41 "Cmts.User" user "User.Comments")
		(42 "Eco1.User" user "User.Eco1")
		(43 "Eco2.User" user "User.Eco2")
		(44 "Edge.Cuts" user)
		(45 "Margin" user)
		(46 "B.CrtYd" user "B.Courtyard")
		(47 "F.CrtYd" user "F.Courtyard")
		(48 "B.Fab" user)
		(49 "F.Fab" user)
	)"""

def fx(v): return f"{v:.4f}"

out = ['(kicad_pcb', '\t(version 20240108)', '\t(generator "pcbnew")',
       '\t(generator_version "8.0")',
       '\t(general (thickness 1.6) (legacy_teardrops no))', '\t(paper "A4")',
       LAYERS,
       '\t(setup (pad_to_mask_clearance 0) (allow_soldermask_bridges_in_footprints no))',
       '\t(net 0 "")']
for n in NETS:
    out.append(f'\t(net {NETIDX[n]} "{n}")')

TXT = '(effects (font (size 1 1) (thickness 0.15)))'
for c in COMPS:
    pads = c["pads"]
    cx, cy = centroid(pads)
    dia, drill = FP[c["fp"]]
    xs = [p[1] for p in pads]; ys = [p[2] for p in pads]
    w = max(xs) - min(xs) + dia + 1.0
    h = max(ys) - min(ys) + dia + 1.0
    out.append(f'\t(footprint "aymc:{c["fp"]}"')
    out.append(f'\t\t(layer "F.Cu")')
    out.append(f'\t\t(uuid "{nu()}")')
    out.append(f'\t\t(at {fx(cx)} {fx(cy)})')
    out.append(f'\t\t(descr "{c["descr"]}")')
    out.append(f'\t\t(attr through_hole)')
    out.append(f'\t\t(property "Reference" "{c["ref"]}" (at 0 {fx(-h/2-1.2)} 0) '
               f'(layer "F.SilkS") (uuid "{nu()}") {TXT})')
    out.append(f'\t\t(property "Value" "{c["val"]}" (at 0 {fx(h/2+1.2)} 0) '
               f'(layer "F.Fab") (uuid "{nu()}") {TXT})')
    # contorno de serigrafia
    x0, y0, x1, y1 = -w/2, -h/2, w/2, h/2
    for (ax, ay, bx, by) in ((x0,y0,x1,y0), (x1,y0,x1,y1), (x1,y1,x0,y1), (x0,y1,x0,y0)):
        out.append(f'\t\t(fp_line (start {fx(ax)} {fx(ay)}) (end {fx(bx)} {fx(by)}) '
                   f'(stroke (width 0.12) (type solid)) (layer "F.SilkS") (uuid "{nu()}"))')
    for (pn, px, py, net) in pads:
        shape = "rect" if pn == 1 else "circle"
        netref = (f'(net {NETIDX[net]} "{net}")' if net else "")
        out.append(f'\t\t(pad "{pn}" thru_hole {shape} (at {fx(px-cx)} {fx(py-cy)}) '
                   f'(size {fx(dia)} {fx(dia)}) (drill {fx(drill)}) '
                   f'(layers "*.Cu" "*.Mask") {netref} (uuid "{nu()}"))')
    out.append('\t)')

# taladros de fijacion
for (hx, hy) in MOUNT:
    out.append(f'\t(footprint "aymc:MountingHole_M3"')
    out.append(f'\t\t(layer "F.Cu") (uuid "{nu()}") (at {fx(hx)} {fx(hy)})')
    out.append(f'\t\t(attr through_hole exclude_from_pos_files exclude_from_bom)')
    out.append(f'\t\t(property "Reference" "" (at 0 -3 0) (layer "F.SilkS") '
               f'(uuid "{nu()}") {TXT})')
    out.append(f'\t\t(pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) '
               f'(layers "F&B.Cu" "*.Mask") (uuid "{nu()}"))')
    out.append('\t)')

# contorno de la placa
for (ax, ay, bx, by) in ((0,0,BW,0), (BW,0,BW,BH), (BW,BH,0,BH), (0,BH,0,0)):
    out.append(f'\t(gr_line (start {fx(ax)} {fx(ay)}) (end {fx(bx)} {fx(by)}) '
               f'(stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (uuid "{nu()}"))')

# pistas
for (net, layer, pts) in TRACKS:
    wdt = wof(net)
    for i in range(len(pts) - 1):
        (ax, ay), (bx, by) = pts[i], pts[i + 1]
        if (ax, ay) == (bx, by):
            continue
        out.append(f'\t(segment (start {fx(ax)} {fx(ay)}) (end {fx(bx)} {fx(by)}) '
                   f'(width {wdt}) (layer "{layer}") (net {NETIDX[net]}) (uuid "{nu()}"))')

# plano de masa en las dos capas
for lay in ("F.Cu", "B.Cu"):
    out.append(f'\t(zone (net {NETIDX["GND"]}) (net_name "GND") (layer "{lay}") '
               f'(uuid "{nu()}") (name "GND") (hatch edge 0.5)')
    out.append('\t\t(connect_pads (clearance 0.5))')
    out.append('\t\t(min_thickness 0.25) (filled_areas_thickness no)')
    out.append('\t\t(fill yes (thermal_gap 0.5) (thermal_bridge_width 0.5))')
    out.append('\t\t(polygon (pts (xy 0.3 0.3) (xy {} 0.3) (xy {} {}) (xy 0.3 {})))'
               .format(fx(BW-0.3), fx(BW-0.3), fx(BH-0.3), fx(BH-0.3)))
    out.append('\t)')
out.append(')')

pcb = "\n".join(out) + "\n"
open(os.path.join(OUT, f"{PROJ}.kicad_pcb"), "w").write(pcb)

# balance de parentesis
d = 0; mn = 0; ins = False; prev = ""
for ch in pcb:
    if ch == '"' and prev != "\\": ins = not ins
    if not ins:
        if ch == "(": d += 1
        elif ch == ")":
            d -= 1; mn = min(mn, d)
    prev = ch
print(f"\n.kicad_pcb: {len(pcb)} bytes, parentesis {d} (min {mn})")

# netlist y BOM
with open(os.path.join(OUT, "netlist.csv"), "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Red", "Nodos"])
    for n in NETS:
        nodos = [f"{r}.{p}" for (r, p, x, y, net) in ALLPADS if net == n]
        w.writerow([n, " / ".join(nodos)])

with open(os.path.join(OUT, "bom.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Ref", "Valor", "Huella", "Descripcion"])
    for c in COMPS:
        w.writerow([c["ref"], c["val"], c["fp"], c["descr"]])
print("netlist.csv y bom.csv regenerados")
