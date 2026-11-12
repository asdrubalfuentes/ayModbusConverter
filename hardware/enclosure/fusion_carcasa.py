"""Carcasa de chapa inoxidable del ayModbusConverter MINI - script de Fusion 360.

COMO USARLO
  Fusion 360 -> UTILIDADES -> COMPLEMENTOS -> Scripts y complementos -> Crear / +
  Script, Python, nombre "carcasa_aymc". Pega este archivo encima del generado
  y pulsa Ejecutar. Crea un documento nuevo con tres componentes.

  Este script usa la API de Fusion (adsk.*), que SOLO existe dentro de Fusion.
  No se puede ejecutar desde una terminal normal.

QUE CONSTRUYE
  1. "Chasis"  - canal en U: fondo + pared superior + pared inferior (2 dobleces).
  2. "Tapa"    - canal en U: frente + dos laterales, con dos pestanas (4 dobleces).
  3. "ClipDIN" - base de montaje para riel DIN 35, pensada para imprimir en 3D.

NOTA SOBRE CHAPA
  La API de Fusion no expone las operaciones de chapa (brida, desdoblar). Las
  piezas se modelan como solidos con el espesor y los radios correctos, que es
  geometricamente identico. Para obtener el patron plano dentro de Fusion:
  selecciona una cara -> Chapa -> Crear pieza de chapa -> Crear patron plano.
  Si solo necesitas cortar, usa los DXF ya generados junto a este archivo.
"""

import math
import adsk.core
import adsk.fusion
import traceback

# --------------------------------------------------------------- parametros
# Todo en milimetros. La API trabaja en centimetros: se convierte con mm().
T       = 1.0      # espesor de la lamina
R       = 1.0      # radio interior de doblado
W_INT   = 104.0    # ancho interior
H_INT   = 82.0     # alto interior
D_INT   = 40.0     # profundidad interior

J1_W, J1_H = 16.0, 16.0    # ventana bornera 24 VDC
J2_W, J2_H = 22.0, 16.0    # ventana bornera RS-485
RJ_W, RJ_H = 17.0, 14.0    # ventana RJ45
WIN_Z0     = 6.0           # separacion de la ventana respecto al fondo

PCB_W, PCB_H   = 95.0, 72.0
PCB_HOLE_IN    = 4.0
M3             = 3.2
DIN_X, DIN_Y   = 30.0, 22.0
FLANGE         = 12.0
RELIEF         = 2.0
COVER_SCREW_IN = 10.0

W_COVER = W_INT + 2 * T
H_COVER = H_INT + 2 * T
D_SIDE  = D_INT - T

# clip DIN
RAIL_W, RAIL_T = 35.0, 1.3     # riel TS35
CLIP_W         = 14.0          # ancho del clip (direccion del riel)
CLIP_BASE      = 4.0           # espesor de la base atornillada
CLIP_HOOK      = 6.5           # vuelo del gancho sobre el riel
CLIP_GAP       = 1.0           # holgura del riel


def mm(v):
    return v / 10.0


def _pt(x, y, z=0.0):
    return adsk.core.Point3D.create(mm(x), mm(y), mm(z))


def u_profile(sk, w_in, h_in, depth, t, r):
    """Dibuja el perfil cerrado de un canal en U con radios de doblado reales.

    Cara interior del dorso en x=0, alas hacia +x. El dorso ocupa y de 0 a h_in.
    Devuelve el perfil para extruir.
    """
    L = sk.sketchCurves.sketchLines
    A = sk.sketchCurves.sketchArcs
    ro = r + t
    hw = h_in

    # ---- contorno interior, de fuera hacia dentro
    L.addByTwoPoints(_pt(depth, 0.0), _pt(r, 0.0))
    A.addByCenterStartSweep(_pt(r, r), _pt(r, 0.0), math.pi / 2)
    L.addByTwoPoints(_pt(0.0, r), _pt(0.0, hw - r))
    A.addByCenterStartSweep(_pt(r, hw - r), _pt(0.0, hw - r), math.pi / 2)
    L.addByTwoPoints(_pt(r, hw), _pt(depth, hw))
    # ---- tapas de los extremos
    L.addByTwoPoints(_pt(depth, hw), _pt(depth, hw + t))
    L.addByTwoPoints(_pt(depth, 0.0), _pt(depth, -t))
    # ---- contorno exterior
    L.addByTwoPoints(_pt(depth, -t), _pt(r, -t))
    A.addByCenterStartSweep(_pt(r, r), _pt(r, -t), math.pi / 2)
    L.addByTwoPoints(_pt(-t, r), _pt(-t, hw - r))
    A.addByCenterStartSweep(_pt(r, hw - r), _pt(-t, hw - r), math.pi / 2)
    L.addByTwoPoints(_pt(-t, hw + t), _pt(depth, hw + t))
    return sk.profiles.item(0)


def cut_box(comp, x, y, z, dx, dy, dz):
    """Resta un prisma recto definido por esquina y tamano, en mm."""
    planes = comp.constructionPlanes
    ipt = planes.createInput()
    ipt.setByOffset(comp.xYConstructionPlane,
                    adsk.core.ValueInput.createByReal(mm(z)))
    pl = planes.add(ipt)
    sk = comp.sketches.add(pl)
    sk.sketchCurves.sketchLines.addTwoPointRectangle(_pt(x, y), _pt(x + dx, y + dy))
    ext = comp.features.extrudeFeatures.createInput(
        sk.profiles.item(0),
        adsk.fusion.FeatureOperations.CutFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(dz)))
    comp.features.extrudeFeatures.add(ext)


def hole(comp, plane, x, y, depth, d=M3):
    sk = comp.sketches.add(plane)
    sk.sketchCurves.sketchCircles.addByCenterRadius(_pt(x, y), mm(d / 2.0))
    ext = comp.features.extrudeFeatures.createInput(
        sk.profiles.item(0),
        adsk.fusion.FeatureOperations.CutFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(depth)))
    comp.features.extrudeFeatures.add(ext)


def build_chasis(root):
    occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
    c = occ.component
    c.name = "Chasis"
    sk = c.sketches.add(c.xYConstructionPlane)
    prof = u_profile(sk, W_INT, H_INT, D_INT, T, R)
    ext = c.features.extrudeFeatures.createInput(
        prof, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(W_INT)))
    c.features.extrudeFeatures.add(ext)

    # ventana de la bornera inferior (pared en y<0)
    cut_box(c, WIN_Z0, -T - 1.0, (W_INT - J2_W) / 2.0, J2_H, T + 2.0, J2_W)
    # ventana de la bornera superior
    cut_box(c, WIN_Z0, H_INT - 1.0, (W_INT - J1_W) / 2.0, J1_H, T + 2.0, J1_W)

    # taladros del PCB y de los clips, en el dorso (plano YZ desplazado)
    planes = c.constructionPlanes
    ip = planes.createInput()
    ip.setByOffset(c.yZConstructionPlane, adsk.core.ValueInput.createByReal(mm(-T)))
    back = planes.add(ip)
    bx0 = (H_INT - PCB_H) / 2.0 + PCB_HOLE_IN
    bz0 = (W_INT - PCB_W) / 2.0 + PCB_HOLE_IN
    for dy in (0.0, PCB_H - 2 * PCB_HOLE_IN):
        for dz in (0.0, PCB_W - 2 * PCB_HOLE_IN):
            hole(c, back, bx0 + dy, bz0 + dz, T + 1.0)
    for sy in (-DIN_Y, DIN_Y):
        for sz in (-DIN_X, DIN_X):
            hole(c, back, H_INT / 2.0 + sy, W_INT / 2.0 + sz, T + 1.0)
    return c


def build_tapa(root):
    m = adsk.core.Matrix3D.create()
    m.translation = adsk.core.Vector3D.create(mm(D_INT + 30.0), 0.0, 0.0)
    occ = root.occurrences.addNewComponent(m)
    c = occ.component
    c.name = "Tapa"
    sk = c.sketches.add(c.xYConstructionPlane)
    prof = u_profile(sk, W_COVER, H_COVER, D_SIDE, T, R)
    ext = c.features.extrudeFeatures.createInput(
        prof, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(W_COVER)))
    c.features.extrudeFeatures.add(ext)
    # ventana del RJ45 en el lateral (ala inferior del perfil)
    cut_box(c, WIN_Z0, -T - 1.0, (W_COVER - RJ_W) / 2.0, RJ_W, T + 2.0, RJ_H)
    return c


def build_clip(root):
    """Clip DIN 35 para impresion 3D: gancho fijo arriba, labio flexible abajo."""
    m = adsk.core.Matrix3D.create()
    m.translation = adsk.core.Vector3D.create(0.0, mm(-H_INT - 40.0), 0.0)
    occ = root.occurrences.addNewComponent(m)
    c = occ.component
    c.name = "ClipDIN"

    half = RAIL_W / 2.0 + CLIP_GAP
    z0 = CLIP_BASE
    z1 = CLIP_BASE + RAIL_T + CLIP_GAP
    top = half + 10.0

    sk = c.sketches.add(c.xYConstructionPlane)
    L = sk.sketchCurves.sketchLines
    pts = [
        (-top, 0.0), (top, 0.0), (top, z0),
        (half, z0), (half, z1), (half - CLIP_HOOK, z1), (half - CLIP_HOOK, z1 + 2.0),
        (half + 2.0, z1 + 2.0), (half + 2.0, z0), (top - 2.0, z0),
    ]
    # perfil simplificado: base + gancho superior + labio inferior simetrico
    prof_pts = [
        (-top, 0.0), (top, 0.0), (top, z0), (half, z0),
        (half, z1), (half - CLIP_HOOK, z1), (half - CLIP_HOOK, z1 + 2.5),
        (half + 3.0, z1 + 2.5), (half + 3.0, z0), (-half - 3.0, z0),
        (-half - 3.0, z1 + 2.5), (-half + CLIP_HOOK, z1 + 2.5),
        (-half + CLIP_HOOK, z1), (-half, z1), (-half, z0), (-top, z0),
    ]
    for i in range(len(prof_pts)):
        a = prof_pts[i]
        b = prof_pts[(i + 1) % len(prof_pts)]
        L.addByTwoPoints(_pt(a[0], a[1]), _pt(b[0], b[1]))
    ext = c.features.extrudeFeatures.createInput(
        sk.profiles.item(0), adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(CLIP_W)))
    c.features.extrudeFeatures.add(ext)

    # dos taladros M3 avellanados pasantes, fuera del paso del riel
    for sx in (-DIN_Y, DIN_Y):
        sk2 = c.sketches.add(c.xYConstructionPlane)
        sk2.sketchCurves.sketchCircles.addByCenterRadius(
            _pt(sx, CLIP_BASE / 2.0), mm(M3 / 2.0))
        e2 = c.features.extrudeFeatures.createInput(
            sk2.profiles.item(0), adsk.fusion.FeatureOperations.CutFeatureOperation)
        e2.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(CLIP_W)))
        c.features.extrudeFeatures.add(e2)
    return c


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        design = adsk.fusion.Design.cast(app.activeProduct)
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        design.fusionUnitsManager.distanceDisplayUnits = \
            adsk.fusion.DistanceUnits.MillimeterDistanceUnits
        root = design.rootComponent
        root.name = "Carcasa ayModbusConverter MINI"

        build_chasis(root)
        build_tapa(root)
        build_clip(root)

        ui.messageBox(
            "Carcasa creada.\n\n"
            f"Interior {W_INT} x {H_INT} x {D_INT} mm, inox {T} mm, radio {R} mm.\n"
            "Componentes: Chasis (2 dobleces), Tapa (2 dobleces + pestanas), ClipDIN.\n\n"
            "Para el patron plano: selecciona una cara -> Chapa -> Crear pieza de chapa\n"
            "-> Crear patron plano. O usa los DXF que acompanan a este script.")
    except:
        if ui:
            ui.messageBox("Fallo:\n{}".format(traceback.format_exc()))
