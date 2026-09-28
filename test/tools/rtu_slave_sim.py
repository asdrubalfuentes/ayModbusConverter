"""
Simulador de esclavos Modbus RTU para el banco de pruebas del ayModbusConverter.

Corre en el PC con un adaptador USB-RS485 conectado al mismo bus que el
conversor. Emula VARIOS esclavos a la vez con datos DETERMINISTAS, para que
gw_test.py pueda verificar cada valor que vuelve por Modbus TCP:

  Holding Registers (FC03/06/16)  HR[a]  = (uid * 1000 + a) & 0xFFFF   (escribibles)
  Input Registers   (FC04)        IR[a]  = (uid * 1000 + a + 500) & 0xFFFF
                                  IR[999] = contador de segundos (vivo)
  Coils             (FC01/05/15)  CO[a]  = (a + uid) % 3 == 0          (escribibles)
  Discrete Inputs   (FC02)        DI[a]  = (a + uid) % 2 == 0
  Direcciones 0..999. Fuera de rango -> excepcion 02 (direccion ilegal).
  Funciones no soportadas -> excepcion 01.

Los Unit ID que no se emulan no responden (como en un bus real): sirven para
probar la excepcion 0x0B de la pasarela. --delay UID:MS retrasa la respuesta
de un esclavo para probar el timeout y los reintentos.

  python test/tools/rtu_slave_sim.py --port COM7 --baud 9600 --parity N --ids 1,2,10
  python test/tools/rtu_slave_sim.py --port COM7 --ids 1,2 --delay 2:1500
"""
from __future__ import annotations

import argparse
import logging
import threading
import time

from pymodbus.datastore import (ModbusSequentialDataBlock, ModbusServerContext,
                                ModbusSlaveContext)
from pymodbus.server import StartSerialServer
from pymodbus.transaction import ModbusRtuFramer

SIZE = 1000


def hr_val(uid: int, a: int) -> int:
    return (uid * 1000 + a) & 0xFFFF


def ir_val(uid: int, a: int) -> int:
    return (uid * 1000 + a + 500) & 0xFFFF


def co_val(uid: int, a: int) -> bool:
    return (a + uid) % 3 == 0


def di_val(uid: int, a: int) -> bool:
    return (a + uid) % 2 == 0


class Block(ModbusSequentialDataBlock):
    """Bloque que cuenta accesos y puede retrasar la respuesta."""

    def __init__(self, uid: int, kind: str, values, delay_s: float, stats: dict):
        # pymodbus suma 1 a la direccion del PDU: el bloque empieza en 1
        super().__init__(1, values)
        self.uid, self.kind, self.delay_s, self.stats = uid, kind, delay_s, stats

    def getValues(self, address, count=1):
        if self.delay_s:
            time.sleep(self.delay_s)
        self.stats[self.uid] = self.stats.get(self.uid, 0) + 1
        return super().getValues(address, count)


def build_context(ids: list[int], delays: dict[int, float], stats: dict) -> ModbusServerContext:
    slaves = {}
    for uid in ids:
        d = delays.get(uid, 0.0)
        slaves[uid] = ModbusSlaveContext(
            hr=Block(uid, "hr", [hr_val(uid, a) for a in range(SIZE)], d, stats),
            ir=Block(uid, "ir", [ir_val(uid, a) for a in range(SIZE)], d, stats),
            co=Block(uid, "co", [co_val(uid, a) for a in range(SIZE)], d, stats),
            di=Block(uid, "di", [di_val(uid, a) for a in range(SIZE)], d, stats),
            zero_mode=False,
        )
    return ModbusServerContext(slaves=slaves, single=False)


def main() -> None:
    ap = argparse.ArgumentParser(description="Simulador de esclavos Modbus RTU (banco ayModbusConverter)")
    ap.add_argument("--port", required=True, help="puerto del adaptador USB-RS485 (COM7, /dev/ttyUSB0)")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--parity", choices="NEO", default="N")
    ap.add_argument("--stopbits", type=int, choices=(1, 2), default=1)
    ap.add_argument("--bytesize", type=int, choices=(7, 8), default=8)
    ap.add_argument("--ids", default="1,2", help="Unit IDs emulados, p.ej. 1,2,10")
    ap.add_argument("--delay", action="append", default=[], metavar="UID:MS",
                    help="retrasa las respuestas de un esclavo (repetible)")
    ap.add_argument("-v", "--verbose", action="store_true", help="traza de tramas de pymodbus")
    a = ap.parse_args()

    ids = [int(x) for x in a.ids.split(",") if x.strip()]
    delays = {}
    for d in a.delay:
        uid, ms = d.split(":")
        delays[int(uid)] = int(ms) / 1000.0

    logging.basicConfig(level=logging.DEBUG if a.verbose else logging.WARNING,
                        format="%(asctime)s %(levelname)s %(message)s")
    stats: dict[int, int] = {}
    ctx = build_context(ids, delays, stats)

    # contador vivo en IR[999] de cada esclavo
    def ticker():
        t0 = time.time()
        while True:
            sec = int(time.time() - t0) & 0xFFFF
            for uid in ids:
                ctx[uid].store["i"].values[SIZE - 1] = sec
            time.sleep(1)

    def reporter():
        while True:
            time.sleep(10)
            if stats:
                print("  accesos por esclavo: " + "  ".join(f"ID{k}={v}" for k, v in sorted(stats.items())))

    threading.Thread(target=ticker, daemon=True).start()
    threading.Thread(target=reporter, daemon=True).start()

    print(f"Esclavos RTU {ids} en {a.port} {a.baud} {a.bytesize}{a.parity}{a.stopbits}"
          + (f"  retardos={ {k: int(v * 1000) for k, v in delays.items()} } ms" if delays else ""))
    print("HR[a]=uid*1000+a  IR[a]=uid*1000+a+500  IR[999]=segundos  (Ctrl+C para salir)")
    StartSerialServer(
        context=ctx,
        framer=ModbusRtuFramer,
        port=a.port,
        baudrate=a.baud,
        parity=a.parity,
        stopbits=a.stopbits,
        bytesize=a.bytesize,
        ignore_missing_slaves=True,   # un ID inexistente no responde (bus real)
        broadcast_enable=True,        # Unit ID 0 = broadcast
    )


if __name__ == "__main__":
    main()
