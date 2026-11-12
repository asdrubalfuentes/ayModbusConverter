// Base de montaje para riel DIN 35 (TS35) - ayModbusConverter MINI
// Impresion 3D. Se atornilla al dorso del chasis con 2 tornillos M3.
//
//   Exportar STL:  openscad -o clip_din35.stl clip_din35.scad
//   o abrirlo en OpenSCAD y pulsar F6 -> Exportar como STL.
//
// Se necesitan DOS unidades por equipo, separadas 60 mm (DIN_X = 30 en el PCB).

// ----------------------------------------------------------- parametros
rail_w     = 35.0;   // ancho nominal del riel TS35
rail_t     = 1.3;    // espesor del labio del riel
gap        = 0.8;    // holgura de montaje
clip_w     = 14.0;   // ancho del clip, en la direccion del riel
base_t     = 4.0;    // espesor de la base atornillada
hook       = 6.0;    // vuelo del gancho sobre el labio del riel
lip_t      = 2.6;    // espesor del gancho
screw_d    = 3.4;    // paso libre M3
head_d     = 6.2;    // alojamiento de la cabeza
head_h     = 2.2;
screw_sep  = 44.0;   // separacion entre tornillos (DIN_Y = 22 -> 44)
spring_t   = 1.8;    // espesor del brazo flexible
spring_len = 12.0;   // largo del brazo flexible
ears       = 7.0;    // material mas alla de los tornillos

$fn = 48;

half   = rail_w/2 + gap;          // semiancho libre para el riel
z_rail = base_t + rail_t + gap;   // cara superior del labio del riel
total  = screw_sep/2 + ears;      // semilargo de la pieza

// ----------------------------------------------------------- modulo
module clip() {
  difference() {
    union() {
      // base
      translate([-total, -clip_w/2, 0]) cube([2*total, clip_w, base_t]);

      // --- lado fijo: gancho rigido que engancha el labio superior
      translate([half - 1, -clip_w/2, 0])
        cube([total - half + 1, clip_w, z_rail]);
      translate([half - hook, -clip_w/2, z_rail])
        cube([hook + 3, clip_w, lip_t]);
      // refuerzo en diagonal
      translate([half + 2, -clip_w/2, base_t])
        rotate([0, -35, 0]) cube([3, clip_w, 9]);

      // --- lado flexible: brazo en voladizo que cede al montar
      translate([-total, -clip_w/2, 0])
        cube([total - half + 1 - spring_len, clip_w, z_rail]);
      // brazo delgado
      translate([-half - spring_len, -clip_w/2, z_rail - spring_t])
        cube([spring_len + 1, clip_w, spring_t]);
      // labio de enganche con rampa de entrada
      translate([-half - 1, -clip_w/2, z_rail - spring_t])
        cube([1 + 3, clip_w, spring_t + lip_t]);
      translate([-half + hook, -clip_w/2, z_rail])
        cube([0.01, clip_w, lip_t]);
      hull() {
        translate([-half, -clip_w/2, z_rail + lip_t - 0.01])
          cube([hook, clip_w, 0.01]);
        translate([-half + hook, -clip_w/2, z_rail])
          cube([0.01, clip_w, 0.01]);
      }
    }

    // --- taladros de fijacion
    for (x = [-screw_sep/2, screw_sep/2]) {
      translate([x, 0, -1])            cylinder(d = screw_d, h = base_t + 2);
      translate([x, 0, base_t - head_h]) cylinder(d = head_d, h = head_h + 1);
    }

    // --- hueco del riel, por si algun saliente invade
    translate([-half, -clip_w/2 - 1, base_t])
      cube([2*half, clip_w + 2, rail_t + gap]);

    // --- ranura de alivio para que el brazo flexione
    translate([-half - spring_len - 0.5, -clip_w/2 - 1, base_t])
      cube([spring_len, clip_w + 2, z_rail - spring_t - base_t]);
  }
}

clip();

// ----------------------------------------------------------- impresion
// Material   : PETG o ABS. El PLA cede con el calor del tablero.
// Orientacion: tal como queda aqui, la base sobre la cama. Las capas quedan
//              perpendicular al esfuerzo del brazo flexible, que es lo correcto.
// Perimetros : 4.  Relleno: 40 %.  Sin soporte.
// Comprobar  : el brazo debe ceder unos 2 mm al montar. Si queda duro, subir
//              spring_len o bajar spring_t en 0,2 mm.
