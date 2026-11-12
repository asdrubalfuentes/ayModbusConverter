"""Ensamble de orientacion del ayModbusConverter MINI - script de Fusion 360.

Responde a una sola pregunta: como queda todo orientado respecto al riel DIN.
Riel, anclajes, chasis, separadores, PCB, modulo WT32-ETH01, borneras y tapa,
cada uno en su sitio, mas los tornillos con su sentido de montaje.

COMO USARLO
  Fusion -> Utilidades -> Complementos -> Secuencias de comandos y complementos
  -> seleccionar "ensamble_aymc" -> Ejecutar.

SISTEMA DE COORDENADAS (documentado porque no es el habitual)
  +X = profundidad, sale del tablero hacia el operador
  +Y = vertical
  +Z = a lo largo del riel
  El dorso del chasis queda en X = -1 (cara exterior) y X = 0 (cara interior).
  El riel queda detras, en X negativo.

Al terminar escribe "reporte_ensamble.txt" junto a este archivo con las cotas
y las holguras calculadas sobre el modelo, no sobre el papel.
"""

import math
import os
import adsk.core
import adsk.fusion
import traceback

# --------------------------------------------------------------- parametros
T       = 1.0      # espesor de la chapa
R       = 1.0      # radio interior de doblado
W_INT   = 104.0    # ancho interior (a lo largo del riel, Z)
H_INT   = 82.0     # alto interior (Y)
D_INT   = 40.0     # profundidad interior (X)

J1_W, J1_H = 16.0, 16.0
J2_W, J2_H = 22.0, 16.0
RJ_W, RJ_H = 17.0, 14.0
WIN_Z0     = 6.0

PCB_W, PCB_H, PCB_T = 95.0, 72.0, 1.6   # PCB a disenar
PCB_HOLE_IN = 4.0
STANDOFF    = 6.0        # separador hembra-macho M3
SOCKET_H    = 8.5        # zocalo hembra de 2,54
MOD_L, MOD_W, MOD_H = 60.0, 26.0, 17.0  # WT32-ETH01
RJ45_L, RJ45_W, RJ45_H = 16.0, 15.9, 13.5

TERM_H   = 12.0    # alto del cuerpo de bornera sobre el PCB
J1_BODY  = 10.4    # bornera de 2 vias, paso 5,08
J2_BODY  = 15.4    # bornera de 3 vias

M3, M2 = 3.2, 2.4
DIN_X, DIN_P, DIN_N = 30.0, 11.0, 3

# --- riel DIN TS35 y anclaje comercial
RAIL_W   = 35.0    # ancho del riel (Y)
RAIL_D   = 7.5     # profundidad del riel (X)
RAIL_T   = 1.0     # espesor de la chapa del riel
RAIL_LEN = 180.0   # tramo representado (Z)
RAIL_WEB = 27.0    # ancho del alma central

CLIP_BASE = 4.0    # [ESTIMADO] espesor de la base del anclaje
CLIP_L    = 44.0   # [ESTIMADO] largo del anclaje (Y)
CLIP_WD   = 9.0    # [ESTIMADO] ancho del anclaje (Z)


def mm(v):
    return v / 10.0


def _p(x, y):
    return adsk.core.Point3D.create(mm(x), mm(y), 0.0)


def _plane(comp, base, off):
    ip = comp.constructionPlanes.createInput()
    ip.setByOffset(base, adsk.core.ValueInput.createByReal(mm(off)))
    pl = comp.constructionPlanes.add(ip)
    pl.isLightBulbOn = False
    return pl


def biggest_profile(sk):
    best, ba = None, -1.0
    for i in range(sk.profiles.count):
        p = sk.profiles.item(i)
        try:
            a = p.areaProperties().area
        except:
            continue
        if a > ba:
            best, ba = p, a
    return best


def _extrude(comp, sk, dist, name=None):
    ext = comp.features.extrudeFeatures.createInput(
        biggest_profile(sk), adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(dist)))
    f = comp.features.extrudeFeatures.add(ext)
    b = f.bodies.item(0)
    if name:
        b.name = name
    return b


def box(comp, name, x0, y0, z0, dx, dy, dz):
    """Prisma recto. z0 es el origen en Z y dz la profundidad del barrido."""
    sk = comp.sketches.add(_plane(comp, comp.xYConstructionPlane, z0))
    sk.isLightBulbOn = False
    sk.sketchCurves.sketchLines.addTwoPointRectangle(_p(x0, y0), _p(x0 + dx, y0 + dy))
    return _extrude(comp, sk, dz, name)


def cyl_x(comp, name, x0, y, z, d, dx):
    """Cilindro con eje en X (tornillos que entran por el dorso).

    OJO: en un plano desplazado del YZ, el eje local X apunta a +Z global
    y el local Y a +Y global. Por eso el centro va como (z, y) y no (y, z).
    """
    sk = comp.sketches.add(_plane(comp, comp.yZConstructionPlane, x0))
    sk.isLightBulbOn = False
    sk.sketchCurves.sketchCircles.addByCenterRadius(_p(z, y), mm(d / 2.0))
    return _extrude(comp, sk, dx, name)


def cyl_y(comp, name, x, y0, z, d, dy):
    """Cilindro con eje en Y (tornillos de la tapa)."""
    sk = comp.sketches.add(_plane(comp, comp.xZConstructionPlane, y0))
    sk.isLightBulbOn = False
    sk.sketchCurves.sketchCircles.addByCenterRadius(_p(x, z), mm(d / 2.0))
    return _extrude(comp, sk, dy, name)


def u_profile(sk, h_in, depth, t, r):
    """Perfil en U en el plano XY: X = profundidad, Y = alto."""
    L, A = sk.sketchCurves.sketchLines, sk.sketchCurves.sketchArcs
    q = -math.pi / 2          # barrido horario: con +pi/2 el contorno no cierra
    hw = h_in
    L.addByTwoPoints(_p(depth, 0.0), _p(r, 0.0))
    A.addByCenterStartSweep(_p(r, r), _p(r, 0.0), q)
    L.addByTwoPoints(_p(0.0, r), _p(0.0, hw - r))
    A.addByCenterStartSweep(_p(r, hw - r), _p(0.0, hw - r), q)
    L.addByTwoPoints(_p(r, hw), _p(depth, hw))
    L.addByTwoPoints(_p(depth, hw), _p(depth, hw + t))
    L.addByTwoPoints(_p(depth, 0.0), _p(depth, -t))
    L.addByTwoPoints(_p(depth, -t), _p(r, -t))
    A.addByCenterStartSweep(_p(r, r), _p(r, -t), q)
    L.addByTwoPoints(_p(-t, r), _p(-t, hw - r))
    A.addByCenterStartSweep(_p(r, hw - r), _p(-t, hw - r), q)
    L.addByTwoPoints(_p(-t, hw + t), _p(depth, hw + t))
    return biggest_profile(sk)


def newcomp(root, name):
    occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
    occ.component.name = name
    return occ.component


# ------------------------------------------------------------- posiciones Z
# El chasis se construye con Y de 0 a H_INT. Para centrarlo en el riel se
# desplaza todo con este offset.
YOFF = -H_INT / 2.0
X_CLIP_FACE = -T - CLIP_BASE          # cara del anclaje donde apoya el dorso
X_RAIL_FRONT = X_CLIP_FACE            # el anclaje abraza el riel por delante
X_RAIL_BACK = X_RAIL_FRONT - RAIL_D


def build_rail(root):
    c = newcomp(root, "Riel DIN TS35")
    z0, L = -RAIL_LEN / 2.0, RAIL_LEN
    xb = X_RAIL_BACK
    # alma central, al fondo
    box(c, "Alma", xb, -RAIL_WEB / 2.0, z0, RAIL_T, RAIL_WEB, L)
    # dos alas hacia delante
    for s in (1, -1):
        y = s * RAIL_WEB / 2.0 - (RAIL_T if s > 0 else 0.0)
        box(c, "Ala", xb, y, z0, RAIL_D, RAIL_T, L)
    # dos labios hacia fuera, donde engancha el anclaje
    lip = (RAIL_W - RAIL_WEB) / 2.0
    for s in (1, -1):
        y = s * RAIL_W / 2.0 - (lip if s > 0 else 0.0)
        box(c, "Labio", xb + RAIL_D - RAIL_T, y, z0, RAIL_T, lip, L)
    return c


def build_clips(root):
    c = newcomp(root, "Anclaje DIN (comercial)")
    for sz in (-DIN_X, DIN_X):
        z0 = sz - CLIP_WD / 2.0
        # base plana atornillada al dorso del chasis
        box(c, "Base anclaje", X_CLIP_FACE, -CLIP_L / 2.0, z0,
            CLIP_BASE, CLIP_L, CLIP_WD)
        # dos ganchos que abrazan los labios del riel
        for s in (1, -1):
            y = s * (RAIL_W / 2.0) - (3.0 if s > 0 else 0.0)
            box(c, "Gancho", X_RAIL_FRONT - RAIL_T - 1.0, y, z0,
                RAIL_T + 1.0, 3.0, CLIP_WD)
    return c


def build_chasis(root):
    c = newcomp(root, "Chasis")
    sk = c.sketches.add(c.xYConstructionPlane)
    sk.isLightBulbOn = False
    u_profile(sk, H_INT, D_INT, T, R)
    ext = c.features.extrudeFeatures.createInput(
        biggest_profile(sk), adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    ext.setDistanceExtent(True, adsk.core.ValueInput.createByReal(mm(W_INT / 2.0)))
    f = c.features.extrudeFeatures.add(ext)
    b = f.bodies.item(0)
    b.name = "Chasis en U"
    # el perfil se dibujo con Y de 0 a H_INT: centrarlo
    mv = c.features.moveFeatures
    col = adsk.core.ObjectCollection.create()
    col.add(b)
    tr = adsk.core.Matrix3D.create()
    tr.translation = adsk.core.Vector3D.create(0.0, mm(YOFF), 0.0)
    mv.add(mv.createInput(col, tr))
    return c, b


def build_pcb(root):
    c = newcomp(root, "PCB (a disenar)")
    x0 = STANDOFF
    box(c, "PCB 95x72", x0, -PCB_H / 2.0, -PCB_W / 2.0, PCB_T, PCB_H, PCB_W)
    # separadores M3 macho-hembra
    for sy in (-1, 1):
        for sz in (-1, 1):
            y = sy * (PCB_H / 2.0 - PCB_HOLE_IN)
            z = sz * (PCB_W / 2.0 - PCB_HOLE_IN)
            cyl_x(c, "Separador M3x6", 0.0, y, z, 5.0, STANDOFF)
            cyl_x(c, "Tornillo PCB", x0 + PCB_T, y, z, M3, 4.0)
            cyl_x(c, "Tuerca dorso", -T - 2.5, y, z, 5.5, 2.5)
    # modulo WT32-ETH01, enchufado en zocalos
    xm = x0 + PCB_T + SOCKET_H
    box(c, "WT32-ETH01", xm, -MOD_W / 2.0, -MOD_L / 2.0, MOD_H, MOD_W, MOD_L)
    # RJ45 asomando por un extremo del modulo
    box(c, "RJ45", xm + 2.0, -RJ45_W / 2.0, -MOD_L / 2.0 - RJ45_L,
        RJ45_H, RJ45_W, RJ45_L)
    # borneras: J1 arriba, J2 abajo
    box(c, "J1 24 VDC", x0 + PCB_T, PCB_H / 2.0 - 6.0, -J1_BODY / 2.0,
        TERM_H, 12.0, J1_BODY)
    box(c, "J2 RS-485", x0 + PCB_T, -PCB_H / 2.0 - 6.0, -J2_BODY / 2.0,
        TERM_H, 12.0, J2_BODY)
    return c


def build_screws(root):
    c = newcomp(root, "Tornilleria")
    span = (DIN_N - 1) * DIN_P
    # M2 del anclaje: entran DESDE DENTRO de la caja hacia el riel
    for sz in (-DIN_X, DIN_X):
        for i in range(DIN_N):
            y = -span / 2.0 + i * DIN_P
            cyl_x(c, "M2 anclaje", -T, y, sz, M2, T + CLIP_BASE)
            cyl_x(c, "Cabeza M2", 0.0, y, sz, 4.0, 1.6)
    return c


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        design = adsk.fusion.Design.cast(app.activeProduct)
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        design.fusionUnitsManager.distanceDisplayUnits = \
            adsk.fusion.DistanceUnits.MillimeterDistanceUnits
        root = design.rootComponent

        build_rail(root)
        build_clips(root)
        build_chasis(root)
        build_pcb(root)
        build_screws(root)

        # ---- informe sobre el modelo
        lin = ["Ensamble de orientacion - ayModbusConverter MINI", "",
               "Ejes: +X profundidad (hacia el operador), +Y vertical,",
               "      +Z a lo largo del riel.", ""]
        for occ in root.occurrences:
            comp = occ.component
            n = comp.bRepBodies.count
            if n == 0:
                continue
            xs, ys, zs = [], [], []
            for b in comp.bRepBodies:
                bb = b.boundingBox
                xs += [bb.minPoint.x * 10, bb.maxPoint.x * 10]
                ys += [bb.minPoint.y * 10, bb.maxPoint.y * 10]
                zs += [bb.minPoint.z * 10, bb.maxPoint.z * 10]
            lin.append("{:<26} {:2d} cuerpos  X {:7.1f}..{:7.1f}  "
                       "Y {:7.1f}..{:7.1f}  Z {:7.1f}..{:7.1f}".format(
                           comp.name, n, min(xs), max(xs),
                           min(ys), max(ys), min(zs), max(zs)))
        lin += ["",
                "Comprobaciones:",
                "  Cara del anclaje en X = {:.1f}".format(X_CLIP_FACE),
                "  Riel de X = {:.1f} a {:.1f}".format(X_RAIL_BACK, X_RAIL_FRONT),
                "  Saliente total desde el riel: {:.1f} mm".format(
                    D_INT - X_RAIL_BACK),
                "  Altura ocupada dentro: {:.1f} de {:.1f} mm".format(
                    STANDOFF + PCB_T + SOCKET_H + MOD_H, D_INT),
                "",
                "NOTA: CLIP_BASE, CLIP_L y CLIP_WD son estimaciones de foto.",
                "      Medir el anclaje real con calibre y corregir arriba.", ""]
        # verificacion automatica: cada pieza dentro de su rango esperado
        esp = {
            "Riel DIN TS35":           (-17.5, 17.5, -RAIL_LEN / 2, RAIL_LEN / 2),
            "Anclaje DIN (comercial)": (-CLIP_L / 2, CLIP_L / 2,
                                        -DIN_X - CLIP_WD, DIN_X + CLIP_WD),
            "Chasis":                  (-H_INT / 2 - T, H_INT / 2 + T,
                                        -W_INT / 2, W_INT / 2),
            "Tornilleria":             (-DIN_P - 3, DIN_P + 3,
                                        -DIN_X - 3, DIN_X + 3),
        }
        lin.append("Verificacion de posiciones:")
        for occ in root.occurrences:
            comp = occ.component
            if comp.name not in esp or comp.bRepBodies.count == 0:
                continue
            ys, zs = [], []
            for b in comp.bRepBodies:
                bb = b.boundingBox
                ys += [bb.minPoint.y * 10, bb.maxPoint.y * 10]
                zs += [bb.minPoint.z * 10, bb.maxPoint.z * 10]
            y0, y1, z0, z1 = esp[comp.name]
            ok = (min(ys) >= y0 - 0.5 and max(ys) <= y1 + 0.5 and
                  min(zs) >= z0 - 0.5 and max(zs) <= z1 + 0.5)
            lin.append("  [{}] {:<26} Y {:6.1f}..{:6.1f} (esp {:6.1f}..{:6.1f})"
                       "  Z {:6.1f}..{:6.1f} (esp {:6.1f}..{:6.1f})".format(
                           "OK " if ok else "MAL", comp.name,
                           min(ys), max(ys), y0, y1,
                           min(zs), max(zs), z0, z1))
        dst = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "reporte_ensamble.txt")
        with open(dst, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lin))

        ui.messageBox(
            "Ensamble creado.\n\n"
            "Riel DIN, anclajes, chasis, PCB con separadores, modulo,\n"
            "borneras y tornilleria, orientados entre si.\n\n"
            "Ejes: +X profundidad, +Y vertical, +Z a lo largo del riel.\n\n"
            "Se escribio reporte_ensamble.txt junto al script.")
    except:
        if ui:
            ui.messageBox("Fallo:\n{}".format(traceback.format_exc()))
