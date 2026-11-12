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
import os
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
M2             = 2.4
# Anclaje DIN comercial: 3 agujeros M2 a 11 mm entre centros, clip en vertical.
DIN_X          = 30.0      # separacion horizontal entre los dos clips
DIN_P          = 11.0      # paso entre taladros
DIN_N          = 3         # taladros por clip
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


def biggest_profile(sk):
    """Devuelve el perfil cerrado de mayor area del boceto.

    profiles.item(0) no sirve: un boceto con arcos puede generar varias
    regiones y la primera suele ser un recorte suelto.
    """
    best, best_area = None, -1.0
    for i in range(sk.profiles.count):
        p = sk.profiles.item(i)
        try:
            a = p.areaProperties().area
        except:
            continue
        if a > best_area:
            best, best_area = p, a
    return best


def u_profile(sk, w_in, h_in, depth, t, r):
    """Dibuja el perfil cerrado de un canal en U con radios de doblado reales.

    Cara interior del dorso en x=0, alas hacia +x. El dorso ocupa y de 0 a h_in.
    Devuelve el perfil para extruir.
    """
    L = sk.sketchCurves.sketchLines
    A = sk.sketchCurves.sketchArcs
    ro = r + t
    hw = h_in

    # Los barridos son NEGATIVOS (horario). Con +pi/2 los arcos salen hacia
    # fuera, el contorno no cierra y la extrusion produce una tira plana.
    q = -math.pi / 2
    # ---- contorno interior, de fuera hacia dentro
    L.addByTwoPoints(_pt(depth, 0.0), _pt(r, 0.0))
    A.addByCenterStartSweep(_pt(r, r), _pt(r, 0.0), q)
    L.addByTwoPoints(_pt(0.0, r), _pt(0.0, hw - r))
    A.addByCenterStartSweep(_pt(r, hw - r), _pt(0.0, hw - r), q)
    L.addByTwoPoints(_pt(r, hw), _pt(depth, hw))
    # ---- tapas de los extremos
    L.addByTwoPoints(_pt(depth, hw), _pt(depth, hw + t))
    L.addByTwoPoints(_pt(depth, 0.0), _pt(depth, -t))
    # ---- contorno exterior
    L.addByTwoPoints(_pt(depth, -t), _pt(r, -t))
    A.addByCenterStartSweep(_pt(r, r), _pt(r, -t), q)
    L.addByTwoPoints(_pt(-t, r), _pt(-t, hw - r))
    A.addByCenterStartSweep(_pt(r, hw - r), _pt(-t, hw - r), q)
    L.addByTwoPoints(_pt(-t, hw + t), _pt(depth, hw + t))
    return biggest_profile(sk)


def cut_box(comp, body, x, y, z, dx, dy, dz):
    """Resta un prisma recto definido por esquina y tamano, en mm.

    participantBodies es obligatorio: sin el, Fusion lanza
    "No se ha encontrado ningun cuerpo de destino para cortar o intersecar".
    """
    planes = comp.constructionPlanes
    ipt = planes.createInput()
    ipt.setByOffset(comp.xYConstructionPlane,
                    adsk.core.ValueInput.createByReal(mm(z)))
    pl = planes.add(ipt)
    pl.isLightBulbOn = False
    sk = comp.sketches.add(pl)
    sk.isLightBulbOn = False
    sk.sketchCurves.sketchLines.addTwoPointRectangle(_pt(x, y), _pt(x + dx, y + dy))
    ext = comp.features.extrudeFeatures.createInput(
        sk.profiles.item(0),
        adsk.fusion.FeatureOperations.CutFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(dz)))
    ext.participantBodies = [body]
    comp.features.extrudeFeatures.add(ext)


def hole(comp, body, plane, x, y, depth, d=M3):
    sk = comp.sketches.add(plane)
    sk.isLightBulbOn = False
    sk.sketchCurves.sketchCircles.addByCenterRadius(_pt(x, y), mm(d / 2.0))
    ext = comp.features.extrudeFeatures.createInput(
        sk.profiles.item(0),
        adsk.fusion.FeatureOperations.CutFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(depth)))
    ext.participantBodies = [body]
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
    feat = c.features.extrudeFeatures.add(ext)
    body = feat.bodies.item(0)
    body.name = "Chasis en U"

    # ventana de la bornera inferior (pared en y<0)
    cut_box(c, body, WIN_Z0, -T - 1.0, (W_INT - J2_W) / 2.0, J2_H, T + 2.0, J2_W)
    # ventana de la bornera superior
    cut_box(c, body, WIN_Z0, H_INT - 1.0, (W_INT - J1_W) / 2.0, J1_H, T + 2.0, J1_W)

    # taladros del PCB y de los clips, en el dorso (plano YZ desplazado)
    planes = c.constructionPlanes
    ip = planes.createInput()
    ip.setByOffset(c.yZConstructionPlane, adsk.core.ValueInput.createByReal(mm(-T)))
    back = planes.add(ip)
    bx0 = (H_INT - PCB_H) / 2.0 + PCB_HOLE_IN
    bz0 = (W_INT - PCB_W) / 2.0 + PCB_HOLE_IN
    for dy in (0.0, PCB_H - 2 * PCB_HOLE_IN):
        for dz in (0.0, PCB_W - 2 * PCB_HOLE_IN):
            hole(c, body, back, bx0 + dy, bz0 + dz, T + 1.0)
    span = (DIN_N - 1) * DIN_P
    for sz in (-DIN_X, DIN_X):
        for i in range(DIN_N):
            hole(c, body, back, H_INT / 2.0 - span / 2.0 + i * DIN_P,
                 W_INT / 2.0 + sz, T + 1.0, M2)
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
    feat = c.features.extrudeFeatures.add(ext)
    body = feat.bodies.item(0)
    body.name = "Tapa en U"
    # ventana del RJ45 en el lateral (ala inferior del perfil)
    cut_box(c, body, WIN_Z0, -T - 1.0, (W_COVER - RJ_W) / 2.0, RJ_W, T + 2.0, RJ_H)
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
        biggest_profile(sk), adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
    ext.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(CLIP_W)))
    feat = c.features.extrudeFeatures.add(ext)
    body = feat.bodies.item(0)
    body.name = "Clip DIN (repuesto imprimible)"

    # taladros de fijacion, al paso del anclaje comercial
    for sx in (-DIN_P, 0.0, DIN_P):
        sk2 = c.sketches.add(c.xYConstructionPlane)
        sk2.isLightBulbOn = False
        sk2.sketchCurves.sketchCircles.addByCenterRadius(
            _pt(sx, CLIP_BASE / 2.0), mm(M2 / 2.0))
        e2 = c.features.extrudeFeatures.createInput(
            sk2.profiles.item(0), adsk.fusion.FeatureOperations.CutFeatureOperation)
        e2.setDistanceExtent(False, adsk.core.ValueInput.createByReal(mm(CLIP_W)))
        e2.participantBodies = [body]
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
        # Ojo: Fusion no permite renombrar el componente raiz desde la API
        # (RuntimeError 3). El nombre se pone al guardar el documento.

        build_chasis(root)
        build_tapa(root)
        build_clip(root)

        # Informe de comprobacion en disco: cotas reales de cada cuerpo.
        # Mas fiable que mirar la pantalla, y permite verificar sin Fusion.
        lineas = ["Informe de la carcasa ayModbusConverter MINI", ""]
        for occ in root.occurrences:
            comp = occ.component
            for b in comp.bRepBodies:
                bb = b.boundingBox
                dx = (bb.maxPoint.x - bb.minPoint.x) * 10.0
                dy = (bb.maxPoint.y - bb.minPoint.y) * 10.0
                dz = (bb.maxPoint.z - bb.minPoint.z) * 10.0
                lineas.append(
                    "{:<12} {:<32} caja {:7.2f} x {:7.2f} x {:7.2f} mm"
                    "   volumen {:9.1f} mm3   area {:9.1f} mm2".format(
                        comp.name, b.name, dx, dy, dz,
                        b.volume * 1000.0, b.area * 100.0))
        esperado = [
            "", "Esperado:",
            "  Chasis  caja  {:.1f} x {:.1f} x {:.1f} mm".format(
                D_INT + T, H_INT + 2 * T, W_INT),
            "  Tapa    caja  {:.1f} x {:.1f} x {:.1f} mm".format(
                D_SIDE + T, H_COVER + 2 * T, W_COVER),
            "  Si la primera cota del chasis sale ~1 mm, el perfil en U fallo",
            "  y solo se extruyo el dorso.",
        ]
        destino = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "reporte.txt")
        with open(destino, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lineas + esperado))

        ui.messageBox(
            "Carcasa creada.\n\n"
            f"Interior {W_INT} x {H_INT} x {D_INT} mm, inox {T} mm, radio {R} mm.\n"
            "Componentes: Chasis (2 dobleces), Tapa (2 dobleces + pestanas), ClipDIN.\n\n"
            "Para el patron plano: selecciona una cara -> Chapa -> Crear pieza de chapa\n"
            "-> Crear patron plano. O usa los DXF que acompanan a este script.")
    except:
        if ui:
            ui.messageBox("Fallo:\n{}".format(traceback.format_exc()))
