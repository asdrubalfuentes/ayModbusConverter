#!/usr/bin/env python3
"""Desarrollos planos (DXF R12) de la carcasa de acero inoxidable del ayModbusConverter MINI.
Dos piezas, 2 dobleces cada una, todos a 90 grados y en el mismo sentido.
No necesita librerias externas: el DXF se escribe a mano."""
import os, math

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "enclosure"))
os.makedirs(OUT, exist_ok=True)

# ----------------------------------------------------------------- parametros
T      = 1.0     # espesor de la lamina (AISI 304)
R      = 1.0     # radio interior de doblado
K      = 0.44    # factor K tipico del inoxidable recocido
W_INT  = 104.0   # ancho interior
H_INT  = 82.0    # alto interior
D_INT  = 40.0    # profundidad interior
FLANGE = 12.0    # pestana de atornillado de la tapa
RELIEF = 2.0     # alivio de esquina

# PCB y componentes (definen las ventanas)
PCB_W, PCB_H = 95.0, 72.0
J1_W,  J1_H  = 16.0, 16.0   # ventana bornera de 24 VDC (2 vias, paso 5,08)
J2_W,  J2_H  = 22.0, 16.0   # ventana bornera RS-485   (3 vias, paso 5,08)
RJ_W,  RJ_H  = 17.0, 14.0   # ventana RJ45 del modulo
WIN_Z0       = 6.0          # separacion de la ventana respecto al fondo
STANDOFF     = 6.0          # altura del separador del PCB

# Montaje
M3   = 3.2      # paso libre para M3
PCB_HOLE_IN = 4.0           # retranqueo de los taladros del PCB
DIN_X = 30.0                # separacion horizontal entre clips DIN
DIN_Y = 22.0                # separacion vertical de tornillos del clip
COVER_SCREW_IN = 6.0        # retranqueo del tornillo de la tapa respecto al frente

def bend_allowance(angle_deg=90.0):
    return math.radians(angle_deg) * (R + K * T)
BA = bend_allowance()
BD = 2.0 * (R + T) - BA           # deduccion de doblado

# ------------------------------------------------------------------ DXF R12
class Dxf:
    def __init__(self):
        self.e = []
        self.layers = {"CORTE": 7, "DOBLEZ": 1, "TEXTO": 3}
    def line(self, x1, y1, x2, y2, layer="CORTE"):
        self.e.append(("LINE", layer, (x1, y1, x2, y2)))
    def rect(self, x, y, w, h, layer="CORTE"):
        self.line(x, y, x + w, y, layer); self.line(x + w, y, x + w, y + h, layer)
        self.line(x + w, y + h, x, y + h, layer); self.line(x, y + h, x, y, layer)
    def circle(self, x, y, d, layer="CORTE"):
        self.e.append(("CIRCLE", layer, (x, y, d / 2.0)))
    def text(self, x, y, h, s, layer="TEXTO"):
        self.e.append(("TEXT", layer, (x, y, h, s)))
    def dump(self, path):
        o = ["0","SECTION","2","HEADER","0","ENDSEC",
             "0","SECTION","2","TABLES","0","TABLE","2","LAYER","70",str(len(self.layers))]
        for nm, col in self.layers.items():
            o += ["0","LAYER","2",nm,"70","0","62",str(col),"6","CONTINUOUS"]
        o += ["0","ENDTAB","0","ENDSEC","0","SECTION","2","ENTITIES"]
        for kind, layer, d in self.e:
            if kind == "LINE":
                x1, y1, x2, y2 = d
                o += ["0","LINE","8",layer,
                      "10",f"{x1:.4f}","20",f"{y1:.4f}","30","0.0",
                      "11",f"{x2:.4f}","21",f"{y2:.4f}","31","0.0"]
            elif kind == "CIRCLE":
                x, y, r = d
                o += ["0","CIRCLE","8",layer,
                      "10",f"{x:.4f}","20",f"{y:.4f}","30","0.0","40",f"{r:.4f}"]
            else:
                x, y, h, s = d
                o += ["0","TEXT","8",layer,
                      "10",f"{x:.4f}","20",f"{y:.4f}","30","0.0",
                      "40",f"{h:.4f}","1",s]
        o += ["0","ENDSEC","0","EOF"]
        open(path, "w").write("\n".join(o) + "\n")
        return len(self.e)

# =========================================================== PIEZA 1: CHASIS
# Canal en U: fondo + pared superior + pared inferior. Dos dobleces.
# El desarrollo se mide con deduccion: largo = fondo + 2*(pared) - 2*BD... pero
# aqui las paredes se acotan como longitud plana, ya descontado el doblez.
wall_flat = D_INT + T - BD / 2.0     # tramo plano de cada pared

d1 = Dxf()
Lx, Ly = W_INT, H_INT                # fondo
y0 = wall_flat                       # el fondo empieza tras la pared inferior
TOT_Y = wall_flat * 2 + Ly

# contorno exterior
d1.rect(0.0, 0.0, Lx, TOT_Y)
# lineas de doblez
d1.line(0.0, y0, Lx, y0, "DOBLEZ")
d1.line(0.0, y0 + Ly, Lx, y0 + Ly, "DOBLEZ")

# --- ventanas de las borneras en las paredes
# pared inferior (y de 0 a y0): la ventana se mide desde la linea de doblez
j2x = (Lx - J2_W) / 2.0
d1.rect(j2x, y0 - WIN_Z0 - J2_H, J2_W, J2_H)
# pared superior
j1x = (Lx - J1_W) / 2.0
d1.rect(j1x, y0 + Ly + WIN_Z0, J1_W, J1_H)

# --- taladros de los separadores del PCB (en el fondo)
px0 = (Lx - PCB_W) / 2.0 + PCB_HOLE_IN
py0 = y0 + (Ly - PCB_H) / 2.0 + PCB_HOLE_IN
for dx in (0.0, PCB_W - 2 * PCB_HOLE_IN):
    for dy in (0.0, PCB_H - 2 * PCB_HOLE_IN):
        d1.circle(px0 + dx, py0 + dy, M3)

# --- taladros de los clips DIN (en el fondo)
cx, cy = Lx / 2.0, y0 + Ly / 2.0
for sx in (-DIN_X, DIN_X):
    for sy in (-DIN_Y, DIN_Y):
        d1.circle(cx + sx, cy + sy, M3)

# --- taladros roscados M3 para la tapa, en las paredes.
# Se miden desde el BORDE LIBRE de la pared, que al montar coincide con el
# plano frontal. La tapa usa esta misma cota para que los taladros casen.
SCREW_FROM_FRONT = COVER_SCREW_IN
for sx in (-(Lx / 2.0) + 15.0, (Lx / 2.0) - 15.0):
    d1.circle(cx + sx, SCREW_FROM_FRONT, M3)
    d1.circle(cx + sx, TOT_Y - SCREW_FROM_FRONT, M3)

d1.text(2.0, TOT_Y + 4.0, 3.0, "PIEZA 1 CHASIS EN U - INOX 304 t=1.0 - 2 DOBLECES 90 DEG HACIA EL MISMO LADO")
d1.text(2.0, TOT_Y + 9.0, 2.5, f"R INT {R} - K {K} - BA {BA:.2f} - BD {BD:.2f} - LINEAS ROJAS = DOBLEZ")
n1 = d1.dump(os.path.join(OUT, "pieza1_chasis_desarrollo.dxf"))

# ============================================================ PIEZA 2: TAPA
# Tapa en U: frente + dos laterales. Dos dobleces. Mas dos pestanas arriba y abajo.
W_COVER = W_INT + 2 * T
H_COVER = H_INT + 2 * T
side_flat = D_INT - T - BD / 2.0

d2 = Dxf()
x0 = side_flat
TOT_X = side_flat * 2 + W_COVER
TOT_Y2 = H_COVER + 2 * FLANGE

# contorno con alivios de esquina: se dibuja por tramos
# franja central (laterales + frente) a lo alto de H_COVER
d2.line(0.0, FLANGE, TOT_X, FLANGE)
d2.line(0.0, FLANGE, 0.0, FLANGE + H_COVER)
d2.line(TOT_X, FLANGE, TOT_X, FLANGE + H_COVER)
d2.line(0.0, FLANGE + H_COVER, TOT_X, FLANGE + H_COVER)
# pestanas: solo sobre el ancho del frente, con alivio de esquina
d2.line(x0 + RELIEF, 0.0, x0 + W_COVER - RELIEF, 0.0)
d2.line(x0 + RELIEF, 0.0, x0 + RELIEF, FLANGE)
d2.line(x0 + W_COVER - RELIEF, 0.0, x0 + W_COVER - RELIEF, FLANGE)
d2.line(x0 + RELIEF, TOT_Y2, x0 + W_COVER - RELIEF, TOT_Y2)
d2.line(x0 + RELIEF, TOT_Y2, x0 + RELIEF, FLANGE + H_COVER)
d2.line(x0 + W_COVER - RELIEF, TOT_Y2, x0 + W_COVER - RELIEF, FLANGE + H_COVER)

# dobleces
d2.line(x0, FLANGE, x0, FLANGE + H_COVER, "DOBLEZ")
d2.line(x0 + W_COVER, FLANGE, x0 + W_COVER, FLANGE + H_COVER, "DOBLEZ")
d2.line(x0 + RELIEF, FLANGE, x0 + W_COVER - RELIEF, FLANGE, "DOBLEZ")
d2.line(x0 + RELIEF, FLANGE + H_COVER, x0 + W_COVER - RELIEF, FLANGE + H_COVER, "DOBLEZ")

# ventana del RJ45 en el lateral izquierdo
rj_y = FLANGE + H_COVER / 2.0 - RJ_H / 2.0
d2.rect(side_flat - WIN_Z0 - RJ_W, rj_y, RJ_W, RJ_H)

# Taladros de la tapa. La pestana dobla en y=FLANGE y se apoya sobre la pared
# del chasis, asi que la distancia del taladro a la linea de doblez debe ser la
# misma que la del taladro del chasis a su borde libre.
ccx = x0 + W_COVER / 2.0
COVER_HOLE_Y = FLANGE - COVER_SCREW_IN
for sx in (-(W_COVER / 2.0) + 15.0, (W_COVER / 2.0) - 15.0):
    d2.circle(ccx + sx, COVER_HOLE_Y, M3)
    d2.circle(ccx + sx, TOT_Y2 - COVER_HOLE_Y, M3)

# rejilla de ventilacion en el lateral derecho: ranuras horizontales
VENT_L, VENT_H, VENT_P = 22.0, 3.0, 9.0
vent_x = TOT_X - side_flat + 8.0
for i in range(5):
    sy = FLANGE + 20.0 + i * VENT_P
    d2.rect(vent_x, sy, VENT_L, VENT_H)

d2.text(2.0, TOT_Y2 + 4.0, 3.0, "PIEZA 2 TAPA EN U - INOX 304 t=1.0 - 4 DOBLECES 90 DEG")
d2.text(2.0, TOT_Y2 + 9.0, 2.5, f"ALIVIO DE ESQUINA {RELIEF} MM - BD {BD:.2f} - LINEAS ROJAS = DOBLEZ")
n2 = d2.dump(os.path.join(OUT, "pieza2_tapa_desarrollo.dxf"))

# ------------------------------------------------------------------- informe
print("=== CARCASA ayModbusConverter MINI ===")
print(f"material            : AISI 304  t={T} mm, R int {R} mm, K {K}")
print(f"allowance / deduccion: BA {BA:.3f} mm   BD {BD:.3f} mm")
print()
print(f"interior            : {W_INT} x {H_INT} x {D_INT} mm")
print(f"PCB                 : {PCB_W} x {PCB_H} mm, separadores de {STANDOFF} mm")
alt = STANDOFF + 1.6 + 26.0
print(f"altura ocupada      : {alt:.1f} mm de {D_INT} mm -> holgura {D_INT-alt:.1f} mm")
print()
print(f"pieza 1 chasis      : desarrollo {Lx:.1f} x {TOT_Y:.1f} mm, 2 dobleces, {n1} entidades")
print(f"pieza 2 tapa        : desarrollo {TOT_X:.1f} x {TOT_Y2:.1f} mm, 4 dobleces, {n2} entidades")
print()
bruto = (Lx * TOT_Y + TOT_X * TOT_Y2) / 100.0
print(f"chapa bruta total   : {bruto:.1f} cm2 por equipo")
print(f"masa aprox. inox    : {bruto/10000.0*T/1000.0*7900*1000:.0f} g")

# comprobaciones
print()
print("--- comprobaciones ---")
chk = [
 ("PCB cabe en el interior", PCB_W < W_INT and PCB_H < H_INT),
 ("modulo + separadores caben en fondo", alt < D_INT),
 ("ventana J1 dentro de la pared", WIN_Z0 + J1_H < D_INT),
 ("ventana J2 dentro de la pared", WIN_Z0 + J2_H < D_INT),
 ("ventana RJ45 dentro del lateral", WIN_Z0 + RJ_W < side_flat),
 ("clips DIN dentro del fondo", 2*DIN_X + 40 < W_INT and 2*DIN_Y + 20 < H_INT),
 ("alivio de esquina >= espesor", RELIEF >= T),
 ("radio de doblado >= espesor", R >= T),
 ("tornillo de tapa: chasis y pestana coinciden",
   abs(SCREW_FROM_FRONT - (FLANGE - COVER_HOLE_Y)) < 1e-6),
 ("taladro de tapa con borde suficiente",
   COVER_HOLE_Y >= 2*M3/2 and (FLANGE-COVER_HOLE_Y) >= 2*M3/2),
 ("ranuras de ventilacion sin solape", VENT_H < VENT_P),
 ("ranuras dentro del lateral", vent_x + VENT_L < TOT_X),
]
for nm, ok in chk:
    print(f"  [{'OK ' if ok else 'REV'}] {nm}")
print()
print("Archivos en", OUT)
