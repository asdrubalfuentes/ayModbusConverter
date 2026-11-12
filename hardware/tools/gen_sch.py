#!/usr/bin/env python3
"""Esquema KiCad del PCB base, generado desde la MISMA tabla que la placa."""
import os, uuid, importlib.util, io, contextlib

spec = importlib.util.spec_from_file_location("g", os.path.join(os.path.dirname(os.path.abspath(__file__)),"gen_pcb.py"))
g = importlib.util.module_from_spec(spec)
with contextlib.redirect_stdout(io.StringIO()):
    spec.loader.exec_module(g)

OUT, PROJ = g.OUT, g.PROJ
SHEET = "3d4e5f6a-7b8c-4d9e-8f0a-1b2c3d4e5f6a"
def nu(): return str(uuid.uuid4())
F  = '(effects (font (size 1.27 1.27)))'
FH = '(effects (font (size 1.27 1.27)) hide)'
SYMS = {}

def P(n, name, et, x, y, ang, ln=2.54):
    return dict(n=str(n), name=name, et=et, x=x, y=y, ang=ang, ln=ln)

def pin(p):
    return (f'      (pin {p["et"]} line (at {p["x"]} {p["y"]} {p["ang"]}) (length {p["ln"]})\n'
            f'        (name "{p["name"]}" {F})\n        (number "{p["n"]}" {F})\n      )')

def mksym(key, pre, val, gfx, pins, hide_n=False, ref_at=(0,5.08), val_at=(0,-5.08)):
    h=[f'  (symbol "aymc:{key}"',
       '    (pin_numbers hide)' if hide_n else '    (pin_numbers)',
       '    (pin_names (offset 0.254))','    (in_bom yes)','    (on_board yes)',
       f'    (property "Reference" "{pre}" (at {ref_at[0]} {ref_at[1]} 0) {F})',
       f'    (property "Value" "{val}" (at {val_at[0]} {val_at[1]} 0) {F})',
       f'    (property "Footprint" "" (at 0 0 0) {FH})',
       f'    (property "Datasheet" "" (at 0 0 0) {FH})',
       f'    (symbol "{key}_0_1"']
    h += ["      " + x for x in gfx]
    h += ['    )', f'    (symbol "{key}_1_1"']
    h += [pin(p) for p in pins]
    h += ['    )','  )']
    SYMS[key] = dict(txt="\n".join(h), pins=pins)

def rect(x1,y1,x2,y2,fill="background"):
    return (f'(rectangle (start {x1} {y1}) (end {x2} {y2}) '
            f'(stroke (width 0.254) (type default)) (fill (type {fill})))')
def poly(pts,fill="none"):
    s=" ".join(f"(xy {x} {y})" for x,y in pts)
    return f'(polyline (pts {s}) (stroke (width 0.254) (type default)) (fill (type {fill})))'

# --- simbolos de dos patillas
mksym("R","R","R",[rect(-1.016,-2.54,1.016,2.54,"none")],
      [P(1,"~","passive",0,3.81,270,1.27),P(2,"~","passive",0,-3.81,90,1.27)],
      hide_n=True, ref_at=(2.54,1.27), val_at=(2.54,-1.27))
mksym("C","C","C",[poly([(-2.54,0.635),(2.54,0.635)]),poly([(-2.54,-0.635),(2.54,-0.635)])],
      [P(1,"~","passive",0,3.81,270,3.175),P(2,"~","passive",0,-3.81,90,3.175)],
      hide_n=True, ref_at=(3.3,1.27), val_at=(3.3,-1.27))
mksym("CP","C","CP",[poly([(-2.54,1.27),(2.54,1.27)]),rect(-2.54,-0.508,2.54,-1.778,"outline")],
      [P(1,"+","passive",0,3.81,270,2.54),P(2,"-","passive",0,-3.81,90,2.032)],
      hide_n=True, ref_at=(3.3,1.27), val_at=(3.3,-1.27))
mksym("LED","D","LED",
      [poly([(-1.27,1.27),(-1.27,-1.27),(1.27,0),(-1.27,1.27)],fill="outline"),
       poly([(1.27,1.27),(1.27,-1.27)]),poly([(1.8,1.8),(3.0,3.0)]),poly([(2.3,0.8),(3.5,2.0)])],
      [P(1,"A","passive",0,3.81,270,2.54),P(2,"K","passive",0,-3.81,90,2.54)],
      hide_n=True, ref_at=(-3.6,1.27), val_at=(-3.6,-1.27))

# --- cajas genericas por numero de patillas
def mkbox(key, pre, val, names):
    n = len(names)
    per = (n + 1) // 2
    h = per * 2.54 + 2.54
    gfx = [rect(-7.62, -h, 7.62, h)]
    pins = []
    for i, (num, nm) in enumerate(names):
        if i < per:
            pins.append(P(num, nm, "passive", -10.16, h - 2.54 - i * 2.54, 0))
        else:
            pins.append(P(num, nm, "passive", 10.16, h - 2.54 - (i - per) * 2.54, 180))
    mksym(key, pre, val, gfx, pins, ref_at=(0, h + 2.54), val_at=(0, -h - 2.54))

PINNAMES = {
    "J1": [(1,"A"),(2,"B"),(3,"+V"),(4,"GND")],
    "U1": [(1,"IN"),(2,"GND"),(3,"OUT")],
    "U2": [(1,"RO"),(2,"~{RE}"),(3,"DE"),(4,"DI"),(5,"GND"),(6,"A"),(7,"B"),(8,"VCC")],
}
MODNAMES = {1:"CLK_EN",2:"GND",3:"3V3",4:"EN",5:"IO32",6:"IO33",7:"IO5",8:"IO17",
            9:"GND",10:"3V3",11:"GND",12:"5V",13:"LINK",14:"GND",15:"IO2",16:"IO4",
            17:"IO35",18:"IO12",19:"IO14",20:"IO15",21:"IO36",22:"IO39",23:"GND",
            24:"IO0",25:"IO3",26:"IO1"}

SYMFOR, PLACE = {}, {}
for c in g.COMPS:
    r = c["ref"]
    if r in ("R1","R2","R3","R4"): SYMFOR[r] = "R"
    elif r in ("C2","C3"):         SYMFOR[r] = "C"
    elif r == "C1":                SYMFOR[r] = "CP"
    elif r == "D1":                SYMFOR[r] = "LED"
    elif r == "J2":
        mkbox("MODA","J","Zocalo A (pines 1-13)",[(i,MODNAMES[i]) for i in range(1,14)])
        SYMFOR[r] = "MODA"
    elif r == "J3":
        mkbox("MODB","J","Zocalo B (pines 14-26)",
              [(i-13,MODNAMES[i]) for i in range(14,27)])
        SYMFOR[r] = "MODB"
    else:
        key = "BOX_" + r
        mkbox(key, r[0], c["val"], PINNAMES[r]); SYMFOR[r] = key

PLACE = {"J1":(60,210),"J2":(200,120),"J3":(275,120),"U1":(60,60),"U2":(60,130),
         "C1":(110,60),"C2":(30,60),"C3":(110,130),
         "R1":(30,160),"R2":(140,95),"R3":(165,95),"R4":(330,95),"D1":(330,125)}

DIR={0:(-1,0),180:(1,0),90:(0,1),270:(0,-1)}
LBL={(-1,0):180,(1,0):0,(0,1):270,(0,-1):90}
JUS={0:"left bottom",90:"left bottom",180:"right bottom",270:"right bottom"}
def fmt(v): return f"{v:g}"

body=[]
for c in g.COMPS:
    r=c["ref"]; key=SYMFOR[r]; ix,iy=PLACE[r]; sym=SYMS[key]
    netof={str(p[0]):p[3] for p in c["pads"]}
    L=[f'  (symbol (lib_id "aymc:{key}") (at {fmt(ix)} {fmt(iy)} 0) (unit 1)',
       '    (in_bom yes) (on_board yes) (dnp no)', f'    (uuid "{nu()}")',
       f'    (property "Reference" "{r}" (at {fmt(ix+12)} {fmt(iy-20)} 0) {F})',
       f'    (property "Value" "{c["val"]}" (at {fmt(ix+12)} {fmt(iy-17)} 0) {F})',
       f'    (property "Footprint" "" (at 0 0 0) {FH})',
       f'    (property "Datasheet" "" (at 0 0 0) {FH})']
    L+=[f'    (pin "{p["n"]}" (uuid "{nu()}"))' for p in sym["pins"]]
    L.append(f'    (instances (project "{PROJ}" (path "/{SHEET}" (reference "{r}") (unit 1))))')
    L.append('  )')
    body.append("\n".join(L))
    for p in sym["pins"]:
        net=netof.get(p["n"])
        ax,ay=ix+p["x"], iy-p["y"]; dx,dy=DIR[p["ang"]]
        ex,ey=ax+dx*5.08, ay+dy*5.08
        body.append(f'  (wire (pts (xy {fmt(ax)} {fmt(ay)}) (xy {fmt(ex)} {fmt(ey)})) '
                    f'(stroke (width 0) (type default)) (uuid "{nu()}"))')
        if not net:
            body.append(f'  (no_connect (at {fmt(ex)} {fmt(ey)}) (uuid "{nu()}"))'); continue
        la=LBL[(dx,dy)]
        body.append(f'  (label "{net}" (at {fmt(ex)} {fmt(ey)} {la}) (fields_autoplaced yes)\n'
                    f'    (effects (font (size 1.27 1.27)) (justify {JUS[la]})) (uuid "{nu()}"))')

NOTES=[(25,25,"PCB BASE ayModbusConverter MINI - Rev D - WT32-ETH01 enchufable, un puerto RS-485"),
 (25,31,"Zocalos J2/J3 separados 22,86 mm (0,9 pulgadas), que es la cota real del modulo."),
 (25,37,"R2/R3 bajan RO de 5 V a 3,3 V: el MAX485 alimentado a 5 V destruiria la entrada IO5."),
 (25,43,"R1 de 120 ohm fija entre A y B. Quitarla si el equipo no es extremo de bus."),
 (25,250,"U1 en TO-220: con 24 V de entrada disipa cerca de 4,8 W. Ver README antes de montar."),
 (25,256,"D1 muestra el trafico Modbus desde IO2. Necesita guia de luz hasta la tapa.")]
for tx,ty,t in NOTES:
    body.append(f'  (text "{t}" (at {fmt(tx)} {fmt(ty)} 0)\n'
                f'    (effects (font (size 1.524 1.524)) (justify left bottom)) (uuid "{nu()}"))')

lib="\n".join(SYMS[k]["txt"] for k in SYMS)
sch=f'''(kicad_sch
  (version 20231120)
  (generator "eeschema")
  (generator_version "8.0")
  (uuid "{SHEET}")
  (paper "A3")
  (title_block
    (title "PCB base ayModbusConverter MINI")
    (date "2026-11-12")
    (rev "D")
    (comment 1 "Generado desde la misma tabla que el .kicad_pcb")
  )
  (lib_symbols
{lib}
  )
{chr(10).join(body)}
  (sheet_instances (path "/" (page "1")))
)
'''
open(os.path.join(OUT,f"{PROJ}.kicad_sch"),"w").write(sch)
open(os.path.join(OUT,"aymc.kicad_sym"),"w").write(
    '(kicad_symbol_lib\n  (version 20231120)\n  (generator "kicad_symbol_editor")\n'
    '  (generator_version "8.0")\n'+lib+"\n)\n")

d=0;mn=0;ins=False;prev=""
for ch in sch:
    if ch=='"' and prev!="\\": ins=not ins
    if not ins:
        if ch=="(": d+=1
        elif ch==")": d-=1; mn=min(mn,d)
    prev=ch
print(f"esquema: {len(sch)} bytes, parentesis {d} (min {mn}), simbolos {len(SYMS)}")
# contraste esquema <-> placa
pads_sch=sum(len(SYMS[SYMFOR[c['ref']]]['pins']) for c in g.COMPS)
print(f"pads en el esquema {pads_sch} / en la placa {len(g.ALLPADS)}  ->",
      "COINCIDEN" if pads_sch==len(g.ALLPADS) else "DIFIEREN")
