#!/usr/bin/env python3
"""Portadora MINIMA ayModbusConverter: WT32-ETH01 V1.4 enchufable + MAX485 + LAN.
Un solo puerto serie. Sin OLED, sin RS-232. Configuracion por consola serie y por WiFi.
Pinaje del modulo verificado contra egnor/wt32-eth01 y src/board.h del firmware."""
import os, uuid, csv

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, ".."))
FPDIR = os.path.join(OUT, "aymc.pretty")
os.makedirs(FPDIR, exist_ok=True)
PROJ = "aymc_carrier_min"
SHEET_UUID = "2c3d4e5f-6a7b-4c8d-9e0f-1a2b3c4d5e6f"

def nu(): return str(uuid.uuid4())
F  = '(effects (font (size 1.27 1.27)))'
FH = '(effects (font (size 1.27 1.27)) hide)'
SYMS = {}

def rect(x1,y1,x2,y2,fill="background"):
    return (f'(rectangle (start {x1} {y1}) (end {x2} {y2}) '
            f'(stroke (width 0.254) (type default)) (fill (type {fill})))')
def poly(pts,fill="none"):
    s=" ".join(f"(xy {x} {y})" for x,y in pts)
    return f'(polyline (pts {s}) (stroke (width 0.254) (type default)) (fill (type {fill})))'
def circ(x,y,r,fill="none"):
    return (f'(circle (center {x} {y}) (radius {r}) '
            f'(stroke (width 0.254) (type default)) (fill (type {fill})))')
def P(num,name,etype,x,y,ang,length=2.54):
    return dict(n=str(num),name=name,etype=etype,x=x,y=y,ang=ang,ln=length)
def pin_txt(p):
    return (f'      (pin {p["etype"]} line (at {p["x"]} {p["y"]} {p["ang"]}) (length {p["ln"]})\n'
            f'        (name "{p["name"]}" {F})\n        (number "{p["n"]}" {F})\n      )')
def mksym(key,refpfx,value,graphics,pins,power=False,hide_nums=True,hide_names=True,
          noff=0.254,ref_at=(0,5.08),val_at=(0,-5.08),fp=""):
    h=[f'  (symbol "aymc:{key}"']
    if power: h.append('    (power)')
    h.append('    (pin_numbers hide)' if hide_nums else '    (pin_numbers)')
    h.append(f'    (pin_names (offset {noff}) hide)' if hide_names else f'    (pin_names (offset {noff}))')
    h+=['    (in_bom yes)','    (on_board yes)',
        f'    (property "Reference" "{refpfx}" (at {ref_at[0]} {ref_at[1]} 0) {F})',
        f'    (property "Value" "{value}" (at {val_at[0]} {val_at[1]} 0) {F})',
        f'    (property "Footprint" "{fp}" (at 0 0 0) {FH})',
        f'    (property "Datasheet" "" (at 0 0 0) {FH})',
        f'    (symbol "{key}_0_1"']
    h+=["      "+g for g in graphics]
    h.append('    )'); h.append(f'    (symbol "{key}_1_1"')
    h+=[pin_txt(p) for p in pins]; h+=['    )','  )']
    SYMS[key]=dict(txt="\n".join(h),pins=pins)

# ---------------- pasivos
mksym("R","R","R",[rect(-1.016,-2.54,1.016,2.54,"none")],
      [P(1,"~","passive",0,3.81,270,1.27),P(2,"~","passive",0,-3.81,90,1.27)],
      ref_at=(2.54,1.27),val_at=(2.54,-1.27))
mksym("C","C","C",[poly([(-2.54,0.635),(2.54,0.635)]),poly([(-2.54,-0.635),(2.54,-0.635)])],
      [P(1,"~","passive",0,3.81,270,3.175),P(2,"~","passive",0,-3.81,90,3.175)],
      ref_at=(3.302,1.27),val_at=(3.302,-1.27))
mksym("CP","C","CP",[poly([(-2.54,1.27),(2.54,1.27)]),rect(-2.54,-0.508,2.54,-1.778,"outline"),
      poly([(-2.032,2.54),(-1.016,2.54)]),poly([(-1.524,3.048),(-1.524,2.032)])],
      [P(1,"~","passive",0,3.81,270,2.54),P(2,"~","passive",0,-3.81,90,2.032)],
      ref_at=(3.302,1.27),val_at=(3.302,-1.27))
_d=[poly([(-1.27,1.27),(-1.27,-1.27),(1.27,0),(-1.27,1.27)],fill="outline"),poly([(1.27,1.27),(1.27,-1.27)])]
mksym("D","D","D",_d,[P(1,"A","passive",0,3.81,270,2.54),P(2,"K","passive",0,-3.81,90,2.54)],
      ref_at=(3.048,1.27),val_at=(3.048,-1.27))
mksym("LED","D","LED",_d+[poly([(1.778,1.778),(3.048,3.048)]),poly([(2.286,0.762),(3.556,2.032)])],
      [P(1,"A","passive",0,3.81,270,2.54),P(2,"K","passive",0,-3.81,90,2.54)],
      ref_at=(-3.556,1.27),val_at=(-3.556,-1.27))
mksym("TVS","D","TVS",_d+[poly([(1.27,1.27),(2.286,2.032)]),poly([(1.27,-1.27),(0.254,-2.032)])],
      [P(1,"A1","passive",0,3.81,270,2.54),P(2,"A2","passive",0,-3.81,90,2.54)],
      ref_at=(3.556,1.27),val_at=(3.556,-1.27))
mksym("FUSE","F","Fuse",[rect(-1.016,-2.54,1.016,2.54,"none"),poly([(0,2.54),(0,-2.54)])],
      [P(1,"~","passive",0,3.81,270,1.27),P(2,"~","passive",0,-3.81,90,1.27)],
      ref_at=(2.54,1.27),val_at=(2.54,-1.27))
mksym("SW","SW","SW_Push",[circ(-0.254,1.27,0.254),circ(-0.254,-1.27,0.254),
      poly([(-2.032,1.778),(1.778,1.778)]),poly([(0,1.778),(0,2.794)]),poly([(-0.254,1.524),(-0.254,-1.524)])],
      [P(1,"A","passive",0,3.81,270,2.286),P(2,"B","passive",0,-3.81,90,2.286)],
      ref_at=(3.556,1.27),val_at=(3.556,-1.27))

# ---------------- integrados
mksym("MAX485","U","MAX485",[rect(-7.62,-10.16,7.62,10.16)],
      [P(1,"RO","output",-10.16,7.62,0),P(2,"~{RE}","input",-10.16,2.54,0),
       P(3,"DE","input",-10.16,-2.54,0),P(4,"DI","input",-10.16,-7.62,0),
       P(5,"GND","power_in",10.16,-7.62,180),P(6,"A","bidirectional",10.16,-2.54,180),
       P(7,"B","bidirectional",10.16,2.54,180),P(8,"VCC","power_in",10.16,7.62,180)],
      hide_nums=False,hide_names=False,ref_at=(0,12.7),val_at=(0,-12.7),
      fp="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm")
mksym("SM712","D","SM712",[rect(-5.08,-5.08,5.08,5.08),poly([(-5.08,2.54),(5.08,2.54)])],
      [P(1,"A","passive",-7.62,2.54,0),P(2,"GND","passive",-7.62,0,0),P(3,"B","passive",-7.62,-2.54,0)],
      hide_nums=False,hide_names=False,ref_at=(0,7.62),val_at=(0,-7.62))
mksym("DCDC","U","K7805-2000R3",[rect(-7.62,-5.08,7.62,5.08)],
      [P(1,"VIN","power_in",-10.16,2.54,0),P(2,"GND","power_in",-10.16,-2.54,0),
       P(3,"VOUT","power_out",10.16,2.54,180)],
      hide_nums=False,hide_names=False,ref_at=(0,7.62),val_at=(0,-7.62))

# ---------------- WT32-ETH01 V1.4: 26 pines (2x13), numeracion fisica real
#  Verificada contra el simbolo y la huella de github.com/egnor/wt32-eth01
#  y contra el datasheet V1.4. Pin 1 = CLK_EN (habilita el oscilador del PHY),
#  distinto de ESP32_EN (pin 4).
MODPINS = [
 (1,"CLK_EN","input"),      (2,"GND","power_in"),   (3,"3V3","power_out"),
 (4,"ESP32_EN","input"),    (5,"IO32","bidirectional"), (6,"IO33","bidirectional"),
 (7,"IO5","bidirectional"), (8,"IO17","bidirectional"), (9,"GND","power_in"),
 (10,"3V3","power_out"),    (11,"GND","power_in"),  (12,"5V","power_in"),
 (13,"LINK_LED","output"),
 (14,"GND","power_in"),     (15,"IO2","bidirectional"), (16,"IO4","bidirectional"),
 (17,"IO35","input"),       (18,"IO12","bidirectional"),(19,"IO14","bidirectional"),
 (20,"IO15","bidirectional"),(21,"IO36","input"),   (22,"IO39","input"),
 (23,"GND","power_in"),     (24,"IO0","bidirectional"), (25,"IO3","input"),
 (26,"IO1","output"),
]
_mp=[]
for n,nm,et in MODPINS:
    if n<=13: _mp.append(P(n,nm,et,-27.94,30.48-(n-1)*5.08,0,2.54))
    else:     _mp.append(P(n,nm,et, 27.94,30.48-(26-n)*5.08,180,2.54))
mksym("WT32_ETH01","MOD","WT32-ETH01 V1.4",
      [rect(-25.4,-33.02,25.4,33.02),
       poly([(12.7,-33.02),(12.7,-38.1),(25.4,-38.1),(25.4,-33.02)])],
      _mp,hide_nums=False,hide_names=False,ref_at=(0,35.56),val_at=(0,-41.91),
      fp="aymc:WT32-ETH01_2x13_THT")

def mkconn(count,key,value):
    h=(count*2.54)/2+1.27; g=[rect(-2.54,-h,2.54,h)]; pins=[]; y0=(count-1)*1.27
    for i in range(count):
        g.append(rect(-1.27,y0-i*2.54-0.508,0,y0-i*2.54+0.508,"outline"))
        pins.append(P(i+1,f"P{i+1}","passive",-5.08,y0-i*2.54,0,2.54))
    mksym(key,"J",value,g,pins,hide_nums=False,ref_at=(0,h+2.54),val_at=(0,-h-2.54))
for c in (2,3,4,6): mkconn(c,f"CONN{c}",f"Conn_01x0{c}")

mksym("PWR_FLAG","#FLG","PWR_FLAG",
      [poly([(0,0),(0,1.27)]),poly([(0,1.27),(-1.016,2.032),(0,2.794),(1.016,2.032),(0,1.27)])],
      [P(1,"pwr","power_out",0,0,90,0)],power=True,noff=0,ref_at=(0,4.572),val_at=(0,3.556))

# ---------------- colocacion
R0805="Resistor_SMD:R_0805_2012Metric"; C0805="Capacitor_SMD:C_0805_2012Metric"
LED08="LED_SMD:LED_0805_2012Metric"

PARTS=[
 ("J1","CONN2",40,55,"24 VDC in","",{1:"+24V_IN",2:"GND"}),
 ("F1","FUSE",70,55,"PTC 500mA","",{1:"+24V_IN",2:"+24V_F"}),
 ("D1","D",92,55,"SS34","Diode_SMD:D_SMA",{1:"+24V_F",2:"+24V"}),
 ("D2","TVS",114,55,"SMBJ33A","Diode_SMD:D_SMB",{1:"+24V",2:"GND"}),
 ("C1","CP",136,55,"22uF/50V","",{1:"+24V",2:"GND"}),
 ("C2","C",158,55,"100nF/50V",C0805,{1:"+24V",2:"GND"}),
 ("U1","DCDC",190,55,"K7805-2000R3","",{1:"+24V",2:"GND",3:"+5V"}),
 ("C3","CP",222,55,"22uF/16V","",{1:"+5V",2:"GND"}),
 ("C4","C",244,55,"100nF/16V",C0805,{1:"+5V",2:"GND"}),
 ("R1","R",266,55,"1k",R0805,{1:"+5V",2:"PWR_LED_A"}),
 ("D3","LED",266,85,"LED verde PWR",LED08,{1:"PWR_LED_A",2:"GND"}),
 ("JP1","CONN3",300,57,"Sel. VCC485","",{1:"+5V",2:"VCC485",3:"+3V3"}),
 ("#FLG01","PWR_FLAG",345,45,"PWR_FLAG","",{1:"+24V"}),
 ("#FLG02","PWR_FLAG",360,45,"PWR_FLAG","",{1:"+5V"}),
 ("#FLG03","PWR_FLAG",375,45,"PWR_FLAG","",{1:"+3V3"}),
 ("#FLG04","PWR_FLAG",390,45,"PWR_FLAG","",{1:"GND"}),

 ("R2","R",70,120,"1k",R0805,{1:"RO_RAW",2:"P1_RO"}),
 ("R3","R",92,120,"2k",R0805,{1:"P1_RO",2:"GND"}),
 ("C5","C",114,120,"100nF",C0805,{1:"VCC485",2:"GND"}),
 ("R4","R",136,120,"120R 0.5W",R0805,{1:"RS485_A",2:"TERM_N"}),
 ("JP2","CONN2",160,120,"Terminacion","",{1:"TERM_N",2:"RS485_B"}),
 ("R5","R",192,120,"560R",R0805,{1:"VCC485",2:"RS485_A"}),
 ("R6","R",214,120,"560R",R0805,{1:"RS485_B",2:"GND"}),
 ("R7","R",236,120,"10R pulse",R0805,{1:"RS485_A",2:"BUS_A"}),
 ("R8","R",258,120,"10R pulse",R0805,{1:"RS485_B",2:"BUS_B"}),
 ("R10","R",280,120,"100R 0.5W",R0805,{1:"BUS_COM",2:"GND"}),

 ("R11","R",70,175,"1k",R0805,{1:"LED_ACT_D",2:"LED_ACT_A"}),
 ("D5","LED",92,175,"LED ambar ACT",LED08,{1:"LED_ACT_A",2:"GND"}),
 ("SW1","SW",120,175,"CFG","",{1:"BTN_CFG",2:"GND"}),
 ("C7","C",144,175,"100nF",C0805,{1:"BTN_CFG",2:"GND"}),
 ("C8","CP",175,175,"10uF",'',{1:"EN",2:"GND"}),
 ("R14","R",200,175,"10k DNP",R0805,{1:"+3V3",2:"EN"}),

 ("MOD1","WT32_ETH01",80,300,"WT32-ETH01 V1.4","aymc:WT32-ETH01_2x13_THT",{
    1:"NC_CLKEN", 2:"GND",      3:"+3V3",      4:"EN",
    5:"BTN_CFG",  6:"P1_DE",    7:"P1_RO",     8:"P1_DI",
    9:"GND",     10:"+3V3",    11:"GND",      12:"+5V",
   13:"NC_LINK", 14:"GND",     15:"LED_ACT_D",16:"NC_IO4",
   17:"NC_IO35", 18:"NC_IO12",19:"NC_IO14",  20:"NC_IO15",
   21:"NC_IO36", 22:"NC_IO39", 23:"GND",      24:"IO0_BOOT",
   25:"UART0_RX",26:"UART0_TX"}),
 ("U2","MAX485",200,265,"MAX485ESA","Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",{
    1:"RO_RAW",2:"P1_DE",3:"P1_DE",4:"P1_DI",5:"GND",6:"RS485_A",7:"RS485_B",8:"VCC485"}),
 ("D4","SM712",265,265,"SM712","Package_TO_SOT_SMD:SOT-23",{1:"BUS_A",2:"GND",3:"BUS_B"}),
 ("J2","CONN3",315,265,"RS-485 (BW500)","",{1:"BUS_A",2:"BUS_B",3:"BUS_COM"}),
 ("J3","CONN6",380,265,"Consola / prog.","",{
    1:"+3V3",2:"UART0_TX",3:"UART0_RX",4:"IO0_BOOT",5:"EN",6:"GND"}),
]

NOTES=[
 (30,25,"PORTADORA ayModbusConverter MINI - Rev C - WT32-ETH01 V1.4 ENCHUFABLE + MAX485. UN PUERTO RS-485 + LAN."),
 (30,31,"Hacia el exterior solo hay dos borneras: J1 (24 VDC) y J2 (RS-485 A / B / GND). Sin OLED, sin RS-232."),
 (30,37,"Pinaje del modulo verificado contra el datasheet V1.4 y la referencia egnor/wt32-eth01. GPIO segun src/board.h."),
 (30,100,"JP1: 1-2 = MAX485 a 5 V (poblar R2=1k, R3=2k). 2-3 = MAX3485/SP3485 a 3,3 V (R2=0R, R3 sin poblar)."),
 (30,106,"R4 = 120R de fin de linea, habilitada con el puente JP2. Cerrar SOLO si la tarjeta es extremo fisico del bus."),
 (30,112,"R5/R6 de polarizacion: una sola vez por segmento. Si el BW500 o el PLC ya polarizan, retirarlas."),
 (30,145,"R10 de 100R entre el borne GND de J2 y la masa de la tarjeta: limita la corriente de lazo de tierra."),
 (30,151,"Sin aislamiento galvanico, R10 es lo unico que protege al transceptor. Sustituir por 0R solo si ambos equipos comparten barra de tierra."),
 (30,205,"C8 = 10 uF en EN: la red R-C interna del modulo es de 1 ms y con rampa lenta de 24 V el ESP32 puede no arrancar."),
 (30,211,"R14 sin poblar. El boton va en IO32, que si tiene pull-up interno (IO35/36/39 no lo tienen)."),
 (30,217,"J3 no es un conector de campo: es la cabecera de 2,54 mm para la primera grabacion y la consola serie. Sin ella la placa no se puede programar."),
 (30,250,"ALIMENTAR SOLO POR 5V (pin 12). Nunca alimentar 3V3 y 5V a la vez: el regulador del modulo no tolera mas de ~6 V."),
 (30,256,"Pin 1 CLK_EN habilita el oscilador del PHY y NO es el EN del ESP32 (pin 4). No confundirlos."),
 (30,262,"Pin 13 LINK_LED sin usar: el RJ45 del modulo ya trae sus propios LED de enlace."),
 (30,268,"IO0 comparte el reloj de 50 MHz del PHY: a GND solo para grabar, libre en operacion. IO12 debe estar bajo al arrancar."),
 (30,274,"IO2 (LED de actividad) debe quedar flotante o en bajo al arrancar: el LED a GND cumple."),
]

# ---------------- generacion del esquema
DIRMAP={0:(-1,0),180:(1,0),90:(0,1),270:(0,-1)}
LBLANG={(-1,0):180,(1,0):0,(0,1):270,(0,-1):90}
JUST={0:"left bottom",90:"left bottom",180:"right bottom",270:"right bottom"}
def fmt(v): return f"{v:g}"

body,nets=[],{}
for ref,key,ix,iy,val,fp,netmap in PARTS:
    sym=SYMS[key]; hide=' hide' if ref.startswith("#") else ''
    L=[f'  (symbol (lib_id "aymc:{key}") (at {fmt(ix)} {fmt(iy)} 0) (unit 1)',
       '    (in_bom yes) (on_board yes) (dnp no)',f'    (uuid "{nu()}")',
       f'    (property "Reference" "{ref}" (at {fmt(ix+14)} {fmt(iy-18)} 0) (effects (font (size 1.27 1.27)){hide}))',
       f'    (property "Value" "{val}" (at {fmt(ix+14)} {fmt(iy-15)} 0) (effects (font (size 1.27 1.27)){hide}))',
       f'    (property "Footprint" "{fp}" (at 0 0 0) {FH})',
       f'    (property "Datasheet" "" (at 0 0 0) {FH})']
    L+=[f'    (pin "{p["n"]}" (uuid "{nu()}"))' for p in sym["pins"]]
    L.append(f'    (instances (project "{PROJ}" (path "/{SHEET_UUID}" (reference "{ref}") (unit 1))))')
    L.append('  )')
    body.append("\n".join(L))
    for p in sym["pins"]:
        net=netmap.get(int(p["n"]))
        if not net: continue
        ax,ay=ix+p["x"],iy-p["y"]; dx,dy=DIRMAP[p["ang"]]
        ex,ey=ax+dx*5.08,ay+dy*5.08
        body.append(f'  (wire (pts (xy {fmt(ax)} {fmt(ay)}) (xy {fmt(ex)} {fmt(ey)})) '
                    f'(stroke (width 0) (type default)) (uuid "{nu()}"))')
        if net.startswith("NC"):
            body.append(f'  (no_connect (at {fmt(ex)} {fmt(ey)}) (uuid "{nu()}"))'); continue
        la=LBLANG[(dx,dy)]
        body.append(f'  (label "{net}" (at {fmt(ex)} {fmt(ey)} {la}) (fields_autoplaced yes)\n'
                    f'    (effects (font (size 1.27 1.27)) (justify {JUST[la]})) (uuid "{nu()}"))')
        nets.setdefault(net,[]).append(f"{ref}.{p['n']} ({p['name']})")
for tx,ty,t in NOTES:
    body.append(f'  (text "{t}" (at {fmt(tx)} {fmt(ty)} 0)\n'
                f'    (effects (font (size 1.524 1.524)) (justify left bottom)) (uuid "{nu()}"))')

libblock="\n".join(SYMS[k]["txt"] for k in SYMS)
sch=f'''(kicad_sch
  (version 20231120)
  (generator "eeschema")
  (generator_version "8.0")
  (uuid "{SHEET_UUID}")
  (paper "A3")
  (title_block
    (title "Portadora ayModbusConverter MINI")
    (date "2026-11-12")
    (rev "C")
    (comment 1 "WT32-ETH01 V1.4 enchufable + MAX485 - un puerto RS-485 + LAN")
    (comment 2 "Sin OLED ni RS-232 - configuracion por serie y WiFi")
  )
  (lib_symbols
{libblock}
  )
{chr(10).join(body)}
  (sheet_instances (path "/" (page "1")))
)
'''
open(os.path.join(OUT,f"{PROJ}.kicad_sch"),"w").write(sch)
open(os.path.join(OUT,"aymc.kicad_sym"),"w").write(
    '(kicad_symbol_lib\n  (version 20231120)\n  (generator "kicad_symbol_editor")\n'
    '  (generator_version "8.0")\n'+libblock+"\n)\n")
open(os.path.join(OUT,"sym-lib-table"),"w").write(
    '(sym_lib_table\n  (version 7)\n  (lib (name "aymc")(type "KiCad")'
    '(uri "${KIPRJMOD}/aymc.kicad_sym")(options "")(descr "Simbolos portadora ayModbusConverter"))\n)\n')
open(os.path.join(OUT,"fp-lib-table"),"w").write(
    '(fp_lib_table\n  (version 7)\n  (lib (name "aymc")(type "KiCad")'
    '(uri "${KIPRJMOD}/aymc.pretty")(options "")(descr "Huellas portadora ayModbusConverter"))\n)\n')
open(os.path.join(OUT,f"{PROJ}.kicad_pro"),"w").write(
    '{\n  "board": {},\n  "meta": {"filename": "aymc_carrier_min.kicad_pro", "version": 1},\n'
    '  "schematic": {},\n  "sheets": [["'+SHEET_UUID+'", "Root"]],\n  "text_variables": {}\n}\n')

# ---------------- huella 2x13 del modulo (geometria fisica medida)
PITCH=2.54; ROW=22.86; X=ROW/2
ys=[-19.05+i*PITCH for i in range(13)]
pads=[]
for n,_,_ in MODPINS:
    if n<=13: px,py=-X, ys[n-1]
    else:     px,py= X, ys[26-n]
    shape="rect" if n==1 else "circle"
    pads.append(f'  (pad "{n}" thru_hole {shape} (at {px:g} {py:g}) (size 1.7 1.7) '
                f'(drill 1.0) (layers "*.Cu" "*.Mask"))')
cy_x=13.0; cy_y0=-3.81-30.0; cy_y1=-3.81+30.0
def line(x1,y1,x2,y2,layer,w=0.12):
    return (f'  (fp_line (start {x1:g} {y1:g}) (end {x2:g} {y2:g}) '
            f'(stroke (width {w}) (type solid)) (layer "{layer}"))')
edges=[line(-cy_x,cy_y0,cy_x,cy_y0,"F.SilkS"),line(cy_x,cy_y0,cy_x,cy_y1,"F.SilkS"),
       line(cy_x,cy_y1,-cy_x,cy_y1,"F.SilkS"),line(-cy_x,cy_y1,-cy_x,cy_y0,"F.SilkS"),
       line(-cy_x-0.25,cy_y0-0.25,cy_x+0.25,cy_y0-0.25,"F.CrtYd",0.05),
       line(cy_x+0.25,cy_y0-0.25,cy_x+0.25,cy_y1+0.25,"F.CrtYd",0.05),
       line(cy_x+0.25,cy_y1+0.25,-cy_x-0.25,cy_y1+0.25,"F.CrtYd",0.05),
       line(-cy_x-0.25,cy_y1+0.25,-cy_x-0.25,cy_y0-0.25,"F.CrtYd",0.05)]
mod=('(footprint "WT32-ETH01_2x13_THT"\n  (version 20240108)\n  (generator "pcbnew")\n'
     '  (layer "F.Cu")\n'
     '  (descr "WT32-ETH01 V1.4 - modulo enchufable, 2x13 pines paso 2,54 mm, filas a 22,86 mm. '
     'Pin 1 cuadrado. Contorno indicativo 26x60 mm; el RJ45 sobresale por el extremo de pines 1/26.")\n'
     '  (tags "WT32-ETH01 ESP32 ethernet module THT")\n'
     '  (attr through_hole)\n'
     f'  (property "Reference" "MOD**" (at 0 {cy_y0-2:g} 0) (layer "F.SilkS")\n'
     '    (effects (font (size 1 1) (thickness 0.15))))\n'
     f'  (property "Value" "WT32-ETH01" (at 0 {cy_y1+2:g} 0) (layer "F.Fab")\n'
     '    (effects (font (size 1 1) (thickness 0.15))))\n'
     + "\n".join(edges) + "\n" + "\n".join(pads) + "\n)\n")
open(os.path.join(FPDIR,"WT32-ETH01_2x13_THT.kicad_mod"),"w").write(mod)

# ---------------- netlist y BOM
with open(os.path.join(OUT,"netlist.csv"),"w",newline="") as f:
    w=csv.writer(f); w.writerow(["Red","Nodos"])
    for n2 in sorted(nets): w.writerow([n2," / ".join(nets[n2])])
BOM=[["Ref","Cant","Valor","Descripcion","Notas"],
 ["MOD1",1,"WT32-ETH01 V1.4","ESP32 + LAN8720A + RJ45; va enchufado","2 zocalos hembra 1x13 paso 2,54"],
 ["U1",1,"K7805-2000R3","DC/DC 24V->5V 2A SIP-3","Alt.: TRACO TSR 1-2450"],
 ["U2",1,"MAX485ESA","Transceptor RS-485 half duplex SOIC-8","MAX3485/SP3485 si JP1 en 3,3 V"],
 ["J1",1,"Bornera 2 vias","Entrada 24 VDC","Paso 5,08 mm"],
 ["J2",1,"Bornera 3 vias","RS-485: A / B / GND","Paso 5,08 mm"],
 ["J3",1,"Header 6 vias","Consola serie y primera grabacion","Paso 2,54 mm, interno"],
 ["JP1",1,"Header 3 vias","Seleccion VCC del transceptor","1-2 = 5 V, 2-3 = 3,3 V"],
 ["JP2",1,"Header 2 vias","Habilita la terminacion de 120 ohm","Cerrar solo en extremo de bus"],
 ["F1",1,"PTC 500 mA","Proteccion de entrada","Rearmable, 60 V"],
 ["D1",1,"SS34","Schottky antiinversion SMA",""],
 ["D2",1,"SMBJ33A","TVS unidireccional 33 V SMB",""],
 ["D3",1,"LED verde 0805","Indicador de alimentacion",""],
 ["D4",1,"SM712","TVS asimetrica -7/+12 V SOT-23","Especifica para RS-485"],
 ["D5",1,"LED ambar 0805","Actividad Modbus (IO2)",""],
 ["C1",1,"22 uF / 50 V","Electrolitico de entrada",""],
 ["C3",1,"22 uF / 16 V","Electrolitico de salida",""],
 ["C8",1,"10 uF / 16 V","Retardo de arranque en EN","CLAVE: evita cuelgues con rampa lenta"],
 ["C2,C4,C5,C7",4,"100 nF","Desacoplo y antirrebote","0805 X7R"],
 ["R1,R11",2,"1 k","Limitadores de LED","0805"],
 ["R2",1,"1 k","Divisor de RO (superior)","0 ohm si VCC485 = 3,3 V"],
 ["R3",1,"2 k","Divisor de RO (inferior)","Sin poblar si VCC485 = 3,3 V"],
 ["R4",1,"120 ohm 0,5 W","Terminacion de fin de linea","Se habilita con JP2"],
 ["R5,R6",2,"560 ohm","Polarizacion de bus","Una sola vez por segmento"],
 ["R7,R8",2,"10 ohm","Antipulso en serie","Tipo pulse-proof"],
 ["R10",1,"100 ohm 0,5 W","Retorno del comun del bus (borne GND)","Clave sin aislamiento"],
 ["R14",1,"10 k","Pull-up de EN","NO POBLAR, huella de reserva"],
 ["SW1",1,"Pulsador 6x6","Boton CFG (IO32)","Corta=pagina, 3s=AP, 10s=fabrica"],
]
with open(os.path.join(OUT,"bom.csv"),"w",newline="") as f: csv.writer(f).writerows(BOM)

# ---------------- validacion
txt=open(os.path.join(OUT,f"{PROJ}.kicad_sch")).read()
d=0;mn=0;ins=False;prev=""
for ch in txt:
    if ch=='"' and prev!="\\": ins=not ins
    if not ins:
        if ch=="(": d+=1
        elif ch==")": d-=1; mn=min(mn,d)
    prev=ch
mtxt=open(os.path.join(FPDIR,"WT32-ETH01_2x13_THT.kicad_mod")).read()
dm=0
for ch in mtxt:
    if ch=="(": dm+=1
    elif ch==")": dm-=1
used={k for _,k,*_ in PARTS}
missing=[k for k in used if f'(symbol "aymc:{k}"' not in txt]
print(f"parentesis esquema  : {d} (min {mn})")
print(f"parentesis huella   : {dm}")
print(f"simbolos sin definir: {missing or 'ninguno'}")
print(f"componentes         : {len(PARTS)}   redes: {len(nets)}")
print(f"pads en la huella   : {mtxt.count('(pad ')}")

BOARD_H={"P1_RO":"IO5","P1_DI":"IO17","P1_DE":"IO33","BTN_CFG":"IO32","LED_ACT_D":"IO2"}
mod_pins={n:nm for n,nm,_ in MODPINS}
modmap=[p for p in PARTS if p[0]=="MOD1"][0][6]
print("--- contraste con src/board.h del firmware ---")
ok=True
for net,gpio in BOARD_H.items():
    pin=[k for k,v in modmap.items() if v==net]
    got=mod_pins[pin[0]] if pin else "NO CONECTADO"
    good=(got==gpio); ok&=good
    print(f"  {net:<11} -> {gpio:<5} | pin {pin[0] if pin else '-':>2} = {got:<9} {'OK' if good else 'ERROR'}")
print("coincide con board.h:", "SI" if ok else "NO")
print("--- comprobaciones de alimentacion ---")
p5=[k for k,v in modmap.items() if v=="+5V"]
p33=[k for k,v in modmap.items() if v=="+3V3"]
print(f"  5V alimentado en pin(es) {p5} (debe ser [12])")
print(f"  3V3 tomado de pin(es) {p33} (salida del modulo, no alimentar)")
for n2,nd in sorted(nets.items()):
    if len(nd)<2: print(f"  AVISO red de un solo nodo: {n2} -> {nd}")
