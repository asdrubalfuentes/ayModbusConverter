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
| `../../gen_carcasa.py` | Generador de los DXF (Python puro) |

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

## 5. Anclaje de riel DIN

Se usa el **adaptador DIN comercial verde** que ya está en uso, no una pieza impresa. Es
un cuerpo nervado con gancho fijo en un extremo y gancho de entrada en el otro, que
abraza el riel TS35 por debajo y ofrece arriba una cara plana con tres agujeros.

Se necesitan **dos unidades** por equipo, atornilladas al dorso del chasis y separadas
60 mm entre ejes. El clip va con su eje largo **en vertical**, perpendicular al riel.

### Patrón de taladros en el chasis

| Concepto | Valor |
|---|---|
| Agujeros por clip | 3 |
| Paso entre centros | 11,0 mm |
| Rosca | M2 |
| Taladro en la chapa | Ø 2,4 mm (paso libre) |
| Separación entre clips | 60 mm (±30 del centro) |

Los seis taladros ya están en `pieza1_chasis_desarrollo.dxf`. Tornillos M2 × 6 con tuerca
o, mejor, tuercas remachables M2 en la chapa para poder desmontar sin acceso por detrás.

### Cotas tomadas de la foto — confirmar con calibre

Estas salen de medir las fotos contra la cuadrícula de 5 mm y la regla, cruzadas con el
paso de 11 mm. Tienen un margen de error de alrededor del 10 %:

| Cota | Estimación | Para qué hace falta |
|---|---|---|
| Largo del clip | ≈ 38–44 mm | Holgura en el dorso |
| Ancho del clip | ≈ 9 mm | Holgura entre clips |
| Diámetro del agujero | ≈ 2,2 mm | Coherente con M2 |
| Alto total con ganchos | sin medir | Separación del riel al tablero |

Para el chasis basta con el patrón de taladros, que es dato firme. Las demás solo hacen
falta si en algún momento se quiere imprimir un repuesto del clip.

> `clip_din35.scad` queda como diseño alternativo imprimible, por si hiciera falta un
> repuesto. No pretende ser una copia del adaptador comercial.

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
