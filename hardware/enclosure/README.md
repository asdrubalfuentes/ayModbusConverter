# Carcasa de chapa inoxidable — ayModbusConverter MINI

Dos piezas de acero inoxidable AISI 304 de 1,0 mm, todos los dobleces a 90°, más dos
bases de riel DIN impresas en 3D.

![Desarrollos planos](desarrollos.png)

---

## 1. Concepto

| Pieza | Material | Dobleces | Desarrollo |
|---|---|:-:|---|
| 1 · Chasis en U | Inox 304, 1,0 mm | 2 | 104,0 × 162,3 mm |
| 2 · Tapa en U | Inox 304, 1,0 mm | 4 | 182,3 × 108,0 mm |
| 3 · Clip DIN ×2 | PETG o ABS | — | impreso |

Chapa bruta: **366 cm² por equipo**, unos **289 g** de inoxidable.

El **chasis** lleva el fondo y las dos paredes donde se apoyan las borneras: la ventana
superior sostiene J1 (24 VDC) y la inferior J2 (RS-485 A/B/GND). El PCB se atornilla al
fondo sobre cuatro separadores de 6 mm. La **tapa** envuelve el frente y los dos
laterales, con la ventana del RJ45 en el costado izquierdo y rejilla de ventilación en el
derecho; se fija con cuatro tornillos M3 a través de dos pestañas.

Las borneras quedan **retenidas por la chapa**: el cuerpo del conector asoma por la
ventana y la pared de 1 mm absorbe el esfuerzo del cable, en lugar de transmitirlo a las
soldaduras del PCB. Es la razón de que las ventanas sean ajustadas.

---

## 2. Cotas

| Concepto | Valor |
|---|---|
| Interior | 104 × 82 × 40 mm |
| Espesor | 1,0 mm |
| Radio interior de doblado | 1,0 mm |
| Factor K | 0,44 (inox recocido) |
| Bend allowance 90° | 2,262 mm |
| Deducción de doblado | 1,738 mm |
| Alivio de esquina | 2,0 mm |

Altura ocupada dentro de la caja: 6 mm de separador + 1,6 de PCB + 26 del módulo con
zócalos = **33,6 mm** de los 40 disponibles. Quedan 6,4 mm de holgura.

> El factor K de 0,44 es el valor habitual del inoxidable recocido con radio igual al
> espesor. Si tu taller trabaja con otro, cámbialo en `gen_carcasa.py` y vuelve a generar
> los DXF: el desarrollo cambia unas décimas por doblez.

---

## 3. Archivos

| Archivo | Para qué sirve |
|---|---|
| `pieza1_chasis_desarrollo.dxf` | Corte láser y plegado del chasis |
| `pieza2_tapa_desarrollo.dxf` | Corte láser y plegado de la tapa |
| `desarrollos.png` | Vista rápida de ambos desarrollos |
| `fusion_carcasa.py` | Script de Fusion 360 que construye el 3D |
| `clip_din35.scad` | Clip de riel DIN paramétrico en OpenSCAD |
| `../tools/gen_carcasa.py` | Generador de los DXF (Python puro) |

### Capas de los DXF

| Capa | Color | Significado |
|---|---|---|
| `CORTE` | blanco | Contorno y ventanas: se cortan |
| `DOBLEZ` | rojo | Líneas de plegado: **no cortar** |
| `TEXTO` | verde | Anotaciones, se pueden borrar |

Avisa al taller de que las líneas rojas son de plegado. Es el malentendido más caro que
puede ocurrir con estos archivos.

---

## 4. Fusion 360

`fusion_carcasa.py` usa la API `adsk`, que **solo existe dentro de Fusion**. No se ejecuta
desde una terminal.

1. Fusion 360 → Utilidades → Complementos → Scripts y complementos
2. Crear → Script → Python, nombre `carcasa_aymc`
3. Sustituye el archivo generado por este y pulsa Ejecutar

Crea un documento con tres componentes: `Chasis`, `Tapa` y `ClipDIN`. Las piezas se
modelan como sólidos con el espesor y los radios correctos, porque la API de Fusion no
expone las operaciones de chapa. Para el patrón plano dentro de Fusion: selecciona una
cara → Chapa → Crear pieza de chapa → Crear patrón plano. Si solo vas a cortar, los DXF
de esta carpeta ya están listos y no hace falta Fusion.

Todos los parámetros están arriba del script, en milímetros.

---

## 5. Clip de riel DIN

```bash
openscad -o clip_din35.stl clip_din35.scad
```

Se necesitan **dos unidades** por equipo. Van al dorso del chasis con dos tornillos M3
cada uno, separados 60 mm entre clips.

Diseño: gancho rígido en un lado y brazo en voladizo en el otro, que cede unos 2 mm al
montar sobre el riel TS35. Sin piezas móviles ni muelles metálicos.

| Parámetro de impresión | Valor |
|---|---|
| Material | PETG o ABS |
| Perímetros | 4 |
| Relleno | 40 % |
| Soporte | No |
| Orientación | Base sobre la cama |

**El PLA no sirve**: dentro de un tablero cerrado se deforma y el clip acaba soltándose.
La orientación importa: tal como sale del `.scad`, las capas quedan perpendiculares al
esfuerzo del brazo flexible, que es lo correcto. Si el brazo queda duro, sube `spring_len`
o baja `spring_t` dos décimas.

---

## 6. Antes de mandar a cortar

Cuatro cosas que conviene verificar contra el material real, porque salen de cotas
nominales y no de piezas medidas:

1. **Ventana del RJ45**: 17 × 14 mm, con unos 1,5 mm de holgura sobre un jack estándar.
   Mide el del módulo que tengas antes de cortar.
2. **Ventanas de las borneras**: dependen del fabricante del conector. Las de 5,08 mm de
   paso varían entre marcas unos milímetros.
3. **Posición del RJ45 en profundidad**: la ventana cubre de 16 a 33 mm desde el fondo,
   calculada con zócalo de 8,5 mm. Si sueldas el módulo directamente, baja esa cota.
4. **Altura de las borneras sobre el PCB**: las ventanas empiezan a 6 mm del fondo.

Lo barato es cortar una pieza de prueba en acero al carbono o incluso en cartón de 1 mm
antes de lanzar la tirada en inoxidable.

---

## 7. Montaje

1. Atornilla los dos clips DIN al dorso del chasis (M3 × 8, avellanados).
2. Monta los cuatro separadores M3 de 6 mm en el fondo.
3. Coloca el PCB ya poblado, con las borneras asomando por las ventanas.
4. Enchufa la WT32-ETH01 en los zócalos.
5. Desliza la tapa y fíjala con los cuatro M3 × 6.
6. Engancha el equipo al riel: primero el lado del gancho rígido, luego presiona el otro.

Para desmontar, haz palanca suave en el brazo flexible con un destornillador plano.
