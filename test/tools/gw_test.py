"""
Bateria de pruebas automaticas del ayModbusConverter (lado Modbus TCP).

Requiere el banco de pruebas: el conversor en la LAN y, en su bus RS-485,
rtu_slave_sim.py emulando los esclavos --ids (datos deterministas). Habla
Modbus TCP con sockets crudos (sin pymodbus) para controlar Transaction ID,
tramas partidas, peticiones encadenadas y MBAP invalidos.

  python test/tools/gw_test.py --host 192.168.1.50
  python test/tools/gw_test.py --host 192.168.1.50 --ids 1,2 --missing 9 --clients 4
  python test/tools/gw_test.py --host 192.168.1.50 --only fc03,concurrency
  python test/tools/gw_test.py --host 192.168.1.50 --soak 3600       # prueba de duracion
  python test/tools/gw_test.py --host 192.168.1.50 --rtu-over-tcp    # modo RTU sobre TCP

Configuracion del conversor esperada: Unit ID transparente, 0 = broadcast,
excepciones de pasarela activas. Codigo de salida 0 si todo pasa.
"""
from __future__ import annotations

import argparse
import base64
import json
import random
import socket
import statistics
import struct
import sys
import threading
import time
import urllib.error
import urllib.request

# ---------------------------------------------------------------------------
# modelo de datos del simulador (debe coincidir con rtu_slave_sim.py)
# ---------------------------------------------------------------------------
def hr_val(uid, a): return (uid * 1000 + a) & 0xFFFF
def ir_val(uid, a): return (uid * 1000 + a + 500) & 0xFFFF
def co_val(uid, a): return (a + uid) % 3 == 0
def di_val(uid, a): return (a + uid) % 2 == 0


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


class ModbusError(Exception):
    def __init__(self, code: int):
        super().__init__(f"excepcion Modbus 0x{code:02X}")
        self.code = code


# ---------------------------------------------------------------------------
# cliente minimo
# ---------------------------------------------------------------------------
class Client:
    def __init__(self, host: str, port: int, rtu: bool = False, timeout: float = 3.0):
        self.rtu = rtu
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.tid = random.randint(0, 0xFFFF)

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def _recv_exact(self, n: int) -> bytes:
        b = b""
        while len(b) < n:
            chunk = self.sock.recv(n - len(b))
            if not chunk:
                raise ConnectionError("conexion cerrada por el conversor")
            b += chunk
        return b

    def frame(self, uid: int, pdu: bytes, tid: int | None = None) -> tuple[bytes, int]:
        if self.rtu:
            adu = bytes([uid]) + pdu
            c = crc16(adu)
            return adu + bytes([c & 0xFF, c >> 8]), 0
        if tid is None:
            self.tid = (self.tid + 1) & 0xFFFF
            tid = self.tid
        return struct.pack(">HHHB", tid, 0, len(pdu) + 1, uid) + pdu, tid

    def recv_response(self) -> tuple[int, int, bytes]:
        """-> (tid, uid, pdu)"""
        if self.rtu:
            head = self._recv_exact(3)
            uid, fc, b2 = head
            if fc & 0x80:
                rest = self._recv_exact(2)
            elif fc in (1, 2, 3, 4, 23):
                rest = self._recv_exact(b2 + 2)
            else:
                rest = self._recv_exact(5)
            adu = head + rest
            c = crc16(adu[:-2])
            if adu[-2] != (c & 0xFF) or adu[-1] != (c >> 8):
                raise ValueError("CRC invalido en la respuesta RTU/TCP")
            return 0, uid, adu[1:-2]
        head = self._recv_exact(7)
        tid, proto, length, uid = struct.unpack(">HHHB", head)
        if proto != 0:
            raise ValueError(f"protocol id {proto}")
        return tid, uid, self._recv_exact(length - 1)

    def request(self, uid: int, pdu: bytes) -> bytes:
        adu, tid = self.frame(uid, pdu)
        self.sock.sendall(adu)
        rtid, ruid, rpdu = self.recv_response()
        if not self.rtu and rtid != tid:
            raise ValueError(f"Transaction ID {rtid} != {tid}")
        if ruid != uid:
            raise ValueError(f"Unit ID {ruid} != {uid}")
        if rpdu[0] & 0x80:
            raise ModbusError(rpdu[1])
        if rpdu[0] != pdu[0]:
            raise ValueError(f"funcion {rpdu[0]} != {pdu[0]}")
        return rpdu

    # --- funciones ---
    def read_regs(self, uid, fc, addr, qty) -> list[int]:
        r = self.request(uid, struct.pack(">BHH", fc, addr, qty))
        if r[1] != qty * 2:
            raise ValueError(f"byte count {r[1]} != {qty * 2}")
        return list(struct.unpack(f">{qty}H", r[2:2 + qty * 2]))

    def read_bits(self, uid, fc, addr, qty) -> list[bool]:
        r = self.request(uid, struct.pack(">BHH", fc, addr, qty))
        return [bool((r[2 + i // 8] >> (i % 8)) & 1) for i in range(qty)]

    def write_reg(self, uid, addr, val):
        r = self.request(uid, struct.pack(">BHH", 6, addr, val))
        if r[1:5] != struct.pack(">HH", addr, val):
            raise ValueError("eco FC06 distinto")

    def write_regs(self, uid, addr, vals):
        pdu = struct.pack(">BHHB", 16, addr, len(vals), len(vals) * 2) + struct.pack(f">{len(vals)}H", *vals)
        self.request(uid, pdu)

    def write_coil(self, uid, addr, on):
        self.request(uid, struct.pack(">BHH", 5, addr, 0xFF00 if on else 0))

    def write_coils(self, uid, addr, bits):
        by = bytearray((len(bits) + 7) // 8)
        for i, b in enumerate(bits):
            if b:
                by[i // 8] |= 1 << (i % 8)
        self.request(uid, struct.pack(">BHHB", 15, addr, len(bits), len(by)) + bytes(by))


# ---------------------------------------------------------------------------
# pruebas
# ---------------------------------------------------------------------------
class Ctx:
    def __init__(self, a):
        self.a = a
        self.ids = [int(x) for x in a.ids.split(",")]
        self.uid = self.ids[0]

    def client(self, timeout=3.0) -> Client:
        return Client(self.a.host, self.a.port, self.a.rtu_over_tcp, timeout)


def expect(cond, msg):
    if not cond:
        raise AssertionError(msg)


def t_fc03(c: Ctx):
    with_c = c.client()
    for uid in c.ids:
        v = with_c.read_regs(uid, 3, 10, 10)
        expect(v == [hr_val(uid, 10 + i) for i in range(10)], f"ID{uid} HR={v}")
    with_c.close()
    return f"IDs {c.ids}"


def t_fc03_max(c: Ctx):
    k = c.client()
    v = k.read_regs(c.uid, 3, 200, 125)
    k.close()
    expect(v == [hr_val(c.uid, 200 + i) for i in range(125)], "125 registros no coinciden")
    return "125 registros (trama maxima 255 B)"


def t_fc04(c: Ctx):
    k = c.client()
    v = k.read_regs(c.uid, 4, 0, 20)
    live1 = k.read_regs(c.uid, 4, 999, 1)[0]
    time.sleep(2.2)
    live2 = k.read_regs(c.uid, 4, 999, 1)[0]
    k.close()
    expect(v == [ir_val(c.uid, i) for i in range(20)], f"IR={v}")
    expect(live2 != live1, "IR[999] no avanza (esclavo congelado?)")
    return f"contador vivo {live1}->{live2}"


def t_fc01_02(c: Ctx):
    k = c.client()
    co = k.read_bits(c.uid, 1, 0, 37)
    di = k.read_bits(c.uid, 2, 0, 37)
    k.close()
    expect(co == [co_val(c.uid, i) for i in range(37)], "coils no coinciden")
    expect(di == [di_val(c.uid, i) for i in range(37)], "entradas discretas no coinciden")
    return "37 bits (byte parcial)"


def t_fc06_16(c: Ctx):
    k = c.client()
    v = random.randint(0, 0xFFFF)
    k.write_reg(c.uid, 500, v)
    expect(k.read_regs(c.uid, 3, 500, 1) == [v], "FC06 no se lee de vuelta")
    vals = [random.randint(0, 0xFFFF) for _ in range(20)]
    k.write_regs(c.uid, 510, vals)
    expect(k.read_regs(c.uid, 3, 510, 20) == vals, "FC16 no se lee de vuelta")
    k.close()
    return "escritura + relectura"


def t_fc05_15(c: Ctx):
    k = c.client()
    k.write_coil(c.uid, 600, True)
    expect(k.read_bits(c.uid, 1, 600, 1) == [True], "FC05 ON")
    k.write_coil(c.uid, 600, False)
    expect(k.read_bits(c.uid, 1, 600, 1) == [False], "FC05 OFF")
    bits = [random.random() < 0.5 for _ in range(19)]
    k.write_coils(c.uid, 610, bits)
    expect(k.read_bits(c.uid, 1, 610, 19) == bits, "FC15 no se lee de vuelta")
    k.close()
    return "coils"


def t_exc_addr(c: Ctx):
    k = c.client()
    try:
        k.read_regs(c.uid, 3, 5000, 2)
    except ModbusError as e:
        expect(e.code == 2, f"se esperaba 0x02, llego 0x{e.code:02X}")
        return "excepcion 0x02 del esclavo llega intacta"
    finally:
        k.close()
    raise AssertionError("no hubo excepcion")


def t_exc_func(c: Ctx):
    k = c.client()
    try:
        k.request(c.uid, bytes([0x41, 0, 0]))
    except ModbusError as e:
        expect(e.code == 1, f"se esperaba 0x01, llego 0x{e.code:02X}")
        return "excepcion 0x01 (funcion ilegal)"
    except (socket.timeout, ConnectionError):
        return "SKIP: el esclavo simulado no responde a funciones desconocidas"
    finally:
        k.close()
    raise AssertionError("no hubo excepcion")


def t_missing(c: Ctx):
    k = c.client(timeout=c.a.rtu_timeout + 3)
    t0 = time.time()
    try:
        k.read_regs(c.a.missing, 3, 0, 1)
    except ModbusError as e:
        dt = time.time() - t0
        expect(e.code == 0x0B, f"se esperaba 0x0B, llego 0x{e.code:02X}")
        return f"0x0B tras {dt * 1000:.0f} ms"
    finally:
        k.close()
    raise AssertionError(f"el ID {c.a.missing} respondio: no deberia existir en el bus")


def t_tid_uid(c: Ctx):
    if c.a.rtu_over_tcp:
        return "SKIP: RTU sobre TCP no tiene Transaction ID"
    k = c.client()
    for tid in (0, 1, 0x7FFF, 0xFFFF, random.randint(2, 0xFFFE)):
        adu, _ = k.frame(c.uid, struct.pack(">BHH", 3, 0, 1), tid=tid)
        k.sock.sendall(adu)
        rtid, ruid, _ = k.recv_response()
        expect(rtid == tid and ruid == c.uid, f"TID {tid}->{rtid} UID {c.uid}->{ruid}")
    k.close()
    return "TID 0, 1, 0x7FFF, 0xFFFF preservados"


def t_pipeline(c: Ctx):
    if c.a.rtu_over_tcp:
        return "SKIP"
    k = c.client()
    tids, blob = [], b""
    for i in range(5):
        adu, tid = k.frame(c.uid, struct.pack(">BHH", 3, i * 10, 2))
        tids.append(tid)
        blob += adu
    k.sock.sendall(blob)                           # 5 peticiones en un solo segmento TCP
    for i, tid in enumerate(tids):
        rtid, _, pdu = k.recv_response()
        vals = list(struct.unpack(">2H", pdu[2:6]))
        expect(rtid == tid and vals == [hr_val(c.uid, i * 10), hr_val(c.uid, i * 10 + 1)], f"respuesta {i} fuera de orden")
    k.close()
    return "5 peticiones en un segmento, respuestas en orden"


def t_fragmented(c: Ctx):
    k = c.client()
    adu, _ = k.frame(c.uid, struct.pack(">BHH", 3, 0, 4))
    k.sock.sendall(adu[:4])
    time.sleep(0.03 if c.a.rtu_over_tcp else 0.3)  # en RTU/TCP el fin de trama es por silencio (50 ms)
    k.sock.sendall(adu[4:])
    _, _, pdu = k.recv_response()
    k.close()
    expect(list(struct.unpack(">4H", pdu[2:10])) == [hr_val(c.uid, i) for i in range(4)], "valores")
    return "peticion partida en 2 segmentos"


def t_bad_mbap(c: Ctx):
    if c.a.rtu_over_tcp:
        return "SKIP"
    k = c.client(timeout=2)
    k.sock.sendall(struct.pack(">HHHB", 1, 0x1234, 6, c.uid) + struct.pack(">BHH", 3, 0, 1))
    try:
        data = k.sock.recv(64)
        expect(data == b"", "el conversor respondio a un MBAP con protocol id != 0")
    except (ConnectionResetError, ConnectionAbortedError):
        pass
    except socket.timeout:
        raise AssertionError("no cerro la conexion ante un MBAP invalido")
    finally:
        k.close()
    return "conexion cerrada"


def t_broadcast(c: Ctx):
    if len(c.ids) < 2:
        return "SKIP: se necesitan 2 esclavos (--ids 1,2)"
    k = c.client(timeout=1)
    v = random.randint(1, 0xFFFE)
    adu, _ = k.frame(0, struct.pack(">BHH", 6, 700, v))
    k.sock.sendall(adu)
    try:
        k.sock.recv(64)
        raise AssertionError("un broadcast no debe tener respuesta")
    except socket.timeout:
        pass
    k.close()
    time.sleep(0.3)
    k = c.client()
    got = {uid: k.read_regs(uid, 3, 700, 1)[0] for uid in c.ids[:2]}
    k.close()
    expect(all(x == v for x in got.values()), f"broadcast no llego a todos: {got}")
    return f"FC06 a Unit ID 0 escrito en {list(got)}"


def t_concurrency(c: Ctx):
    n, per = c.a.clients, 40
    errors, lat = [], []
    lock = threading.Lock()

    def worker(w):
        try:
            k = c.client(timeout=10)
            uid = c.ids[w % len(c.ids)]
            for j in range(per):
                addr = (w * 50 + j) % 900
                t0 = time.time()
                v = k.read_regs(uid, 3, addr, 5)
                with lock:
                    lat.append(time.time() - t0)
                if v != [hr_val(uid, addr + i) for i in range(5)]:
                    raise AssertionError(f"cliente {w}: respuesta de otra peticion {v}")
            k.close()
        except Exception as e:  # noqa: BLE001
            with lock:
                errors.append(f"cliente {w}: {e}")

    th = [threading.Thread(target=worker, args=(w,)) for w in range(n)]
    t0 = time.time()
    for t in th: t.start()
    for t in th: t.join()
    dt = time.time() - t0
    expect(not errors, "; ".join(errors[:3]))
    return f"{n} clientes x {per} = {n * per} peticiones en {dt:.1f} s ({n * per / dt:.1f}/s), espera media {statistics.mean(lat) * 1000:.0f} ms"


def t_maxclients(c: Ctx):
    m = c.a.max_clients
    conns = []
    try:
        for _ in range(m):
            k = c.client()
            k.read_regs(c.uid, 3, 0, 1)
            conns.append(k)
        extra = c.client(timeout=2)
        try:
            extra.read_regs(c.uid, 3, 0, 1)
            raise AssertionError(f"la conexion {m + 1} fue aceptada (maximo {m})")
        except (ConnectionError, ConnectionResetError, ConnectionAbortedError, socket.timeout, OSError):
            pass
        finally:
            extra.close()
    finally:
        for k in conns:
            k.close()
    return f"{m} aceptadas, la {m + 1}a rechazada"


def t_latency(c: Ctx):
    k = c.client()
    lat = []
    for i in range(c.a.samples):
        t0 = time.perf_counter()
        k.read_regs(c.uid, 3, i % 900, 10)
        lat.append((time.perf_counter() - t0) * 1000)
    k.close()
    lat.sort()
    p95 = lat[int(len(lat) * 0.95) - 1]
    return f"{len(lat)} x FC03(10): min {lat[0]:.0f}  media {statistics.mean(lat):.0f}  p95 {p95:.0f}  max {lat[-1]:.0f} ms"


def http(c: Ctx, path: str, body=None, timeout=15):
    """GET/POST a la API web con HTTP Basic (o X-API-Key si se dio --api-key). -> (codigo, json)"""
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(f"http://{c.a.host}{path}", data=data)
    if data is not None:
        req.add_header("Content-Type", "application/json")
    if c.a.api_key:
        req.add_header("X-API-Key", c.a.api_key)
    else:
        tok = base64.b64encode(f"{c.a.user}:{c.a.password}".encode()).decode()
        req.add_header("Authorization", "Basic " + tok)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, json.load(r)
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.load(e)
        except Exception:  # noqa: BLE001
            return e.code, {}


def t_api(c: Ctx):
    _, s = http(c, "/api/status")
    st = s["gw"]["stats"]
    expect(s["gw"]["listening"], "el servidor Modbus no escucha")
    expect(s.get("state") in ("OK", "FALLAS", "DESCONECTADO", "SIN TRAFICO", "LAN DESCONECTADA"), f"estado desconocido {s.get('state')}")
    return f"FW {s['fw']}  estado {s['state']}  pet={st['tcpReq']} ok={st['ok']} timeout={st['timeout']} heap {s['heap'] // 1024} kB"


def t_fastfail(c: Ctx):
    """Tras N timeouts seguidos el esclavo queda 'caido' y se responde 0x0B al instante."""
    k = c.client(timeout=c.a.rtu_timeout * 2 + 3)
    times = []
    for _ in range(c.a.off_after + 2):
        t0 = time.time()
        try:
            k.read_regs(c.a.missing, 3, 0, 1)
        except ModbusError as e:
            expect(e.code == 0x0B, f"se esperaba 0x0B, llego 0x{e.code:02X}")
        times.append((time.time() - t0) * 1000)
    k.close()
    fast = times[-1]
    expect(fast < c.a.rtu_timeout * 1000 / 2, f"el esclavo caido sigue tardando {fast:.0f} ms (esperado inmediato)")
    _, s = http(c, "/api/status")
    sl = [x for x in s["gw"]["slaves"] if x["uid"] == c.a.missing]
    st = sl[0]["state"] if sl else "?"
    expect(st == "DESCONECTADO", f"el esclavo {c.a.missing} figura como {st}")
    return f"timeouts {[round(t) for t in times[:c.a.off_after]]} ms -> luego {fast:.0f} ms; estado {st}"


def t_rest_read(c: Ctx):
    code, j = http(c, f"/api/read?uid={c.uid}&fc=3&addr=20&qty=5&cache=0")
    expect(code == 200 and j.get("ok"), f"HTTP {code} {j}")
    expect(j["values"] == [hr_val(c.uid, 20 + i) for i in range(5)], f"valores {j['values']}")
    return f"/api/read -> {j['values']} en {j['lat']} ms"


def t_rest_write(c: Ctx):
    v = random.randint(0, 0xFFFF)
    code, j = http(c, "/api/write", {"uid": c.uid, "fc": 6, "addr": 800, "value": v})
    expect(code == 200 and j.get("ok"), f"HTTP {code} {j}")
    _, j = http(c, f"/api/read?uid={c.uid}&fc=3&addr=800&qty=1&cache=0")
    expect(j.get("values") == [v], f"relectura {j.get('values')} != {v}")
    return f"/api/write FC06 {v} + relectura"


def t_tags(c: Ctx):
    code, j = http(c, "/api/tags")
    expect(code == 200, f"HTTP {code}")
    return f"{len(j['tags'])} entrada(s) en la tabla de sondeo"


def t_cache(c: Ctx):
    """Requiere --cache: entrada de sondeo P1, esclavo --ids[0], FC03 addr 0 qty>=10 y cache activa en el puerto."""
    if not c.a.cache:
        return "SKIP: use --cache (ver test/README.md)"
    _, s0 = http(c, "/api/status")
    k = c.client()
    lat = []
    for _ in range(30):
        t0 = time.perf_counter()
        v = k.read_regs(c.uid, 3, 0, 10)
        lat.append((time.perf_counter() - t0) * 1000)
        expect(v == [hr_val(c.uid, i) for i in range(10)], f"valores {v}")
    k.close()
    _, s1 = http(c, "/api/status")
    hits = s1["gw"]["stats"]["cache"] - s0["gw"]["stats"]["cache"]
    expect(hits >= 25, f"solo {hits} de 30 lecturas salieron de la cache")
    return f"{hits}/30 desde cache, latencia media {statistics.mean(lat):.1f} ms"


def t_readonly(c: Ctx):
    """Requiere --ro-uid N: una regla de solo lectura que cubra al esclavo N (puerto 1)."""
    if not c.a.ro_uid:
        return "SKIP: use --ro-uid N (ver test/README.md)"
    k = c.client()
    try:
        k.write_reg(c.a.ro_uid, 900, 1)
    except ModbusError as e:
        expect(e.code == 0x01, f"se esperaba 0x01, llego 0x{e.code:02X}")
        return "escritura bloqueada con excepcion 0x01"
    finally:
        k.close()
    raise AssertionError("la escritura paso: la regla de solo lectura no actuo")


TESTS = [
    ("fc03", "Lectura holding (FC03) en cada esclavo", t_fc03),
    ("fc03max", "Lectura maxima 125 registros", t_fc03_max),
    ("fc04", "Input registers (FC04) + contador vivo", t_fc04),
    ("fc01", "Coils y entradas discretas (FC01/02)", t_fc01_02),
    ("fc06", "Escritura de registros (FC06/16)", t_fc06_16),
    ("fc05", "Escritura de coils (FC05/15)", t_fc05_15),
    ("excaddr", "Excepcion del esclavo pasa intacta", t_exc_addr),
    ("excfunc", "Funcion ilegal", t_exc_func),
    ("missing", "Esclavo inexistente -> 0x0B", t_missing),
    ("tid", "Transaction ID / Unit ID preservados", t_tid_uid),
    ("pipeline", "Peticiones encadenadas", t_pipeline),
    ("fragment", "Peticion fragmentada", t_fragmented),
    ("badmbap", "MBAP invalido cierra la conexion", t_bad_mbap),
    ("broadcast", "Broadcast (Unit ID 0)", t_broadcast),
    ("concurrency", "Clientes simultaneos", t_concurrency),
    ("maxclients", "Limite de conexiones", t_maxclients),
    ("latency", "Latencia extremo a extremo", t_latency),
    ("api", "API web /api/status + estado", t_api),
    ("fastfail", "Esclavo caido -> 0x0B inmediato", t_fastfail),
    ("restread", "API REST /api/read", t_rest_read),
    ("restwrite", "API REST /api/write", t_rest_write),
    ("tags", "API REST /api/tags", t_tags),
    ("cache", "Modo cache (opcional)", t_cache),
    ("readonly", "Solo lectura (opcional)", t_readonly),
]


def run_suite(ctx: Ctx, only: set[str] | None) -> bool:
    ok_all = True
    print(f"\nayModbusConverter {ctx.a.host}:{ctx.a.port} ({'RTU/TCP' if ctx.a.rtu_over_tcp else 'Modbus TCP'})  esclavos {ctx.ids}\n")
    for key, name, fn in TESTS:
        if only and key not in only:
            continue
        t0 = time.time()
        try:
            detail = fn(ctx) or ""
            tag = "SKIP" if str(detail).startswith("SKIP") else "PASS"
        except Exception as e:  # noqa: BLE001
            detail, tag = f"{type(e).__name__}: {e}", "FAIL"
            ok_all = False
        color = {"PASS": "\033[32m", "FAIL": "\033[31m", "SKIP": "\033[33m"}[tag]
        print(f" {color}{tag}\033[0m  {name:<42} {time.time() - t0:5.1f}s  {detail}")
    return ok_all


def soak(ctx: Ctx, seconds: int) -> bool:
    """Carga sostenida con varios clientes; informa cada 60 s."""
    stop = time.time() + seconds
    cnt = {"ok": 0, "err": 0}
    lock = threading.Lock()

    def worker(w):
        uid = ctx.ids[w % len(ctx.ids)]
        k = None
        while time.time() < stop:
            try:
                if k is None:
                    k = ctx.client(timeout=5)
                a = random.randint(0, 880)
                v = k.read_regs(uid, 3, a, 10)
                good = v == [hr_val(uid, a + i) for i in range(10)]
                with lock:
                    cnt["ok" if good else "err"] += 1
            except Exception:  # noqa: BLE001
                with lock:
                    cnt["err"] += 1
                if k:
                    k.close()
                k = None
                time.sleep(1)
        if k:
            k.close()

    th = [threading.Thread(target=worker, args=(w,), daemon=True) for w in range(ctx.a.clients)]
    for t in th: t.start()
    t0 = time.time()
    while any(t.is_alive() for t in th):
        time.sleep(60 if seconds > 120 else 5)
        extra = ""
        try:
            extra = "  " + t_api(ctx)
        except Exception as e:  # noqa: BLE001
            extra = f"  api: {e}"
        print(f" {int(time.time() - t0):>6}s  ok={cnt['ok']}  err={cnt['err']}{extra}")
    return cnt["err"] == 0


def main() -> None:
    ap = argparse.ArgumentParser(description="Pruebas automaticas del ayModbusConverter")
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=502)
    ap.add_argument("--ids", default="1,2", help="esclavos emulados por rtu_slave_sim.py")
    ap.add_argument("--missing", type=int, default=9, help="Unit ID que NO existe en el bus")
    ap.add_argument("--rtu-timeout", type=float, default=1.0, help="timeout RTU del conversor (s)")
    ap.add_argument("--clients", type=int, default=4)
    ap.add_argument("--max-clients", type=int, default=4, help="'Conexiones simultaneas' del conversor")
    ap.add_argument("--samples", type=int, default=200)
    ap.add_argument("--rtu-over-tcp", action="store_true")
    ap.add_argument("--user", default="admin")
    ap.add_argument("--password", default="admin")
    ap.add_argument("--api-key", default="", help="usar X-API-Key en vez de usuario/clave")
    ap.add_argument("--off-after", type=int, default=3, help="'Marcar caido tras' del puerto 1")
    ap.add_argument("--cache", action="store_true", help="probar el modo cache (requiere configurarlo)")
    ap.add_argument("--ro-uid", type=int, default=0, help="esclavo cubierto por una regla de solo lectura")
    ap.add_argument("--only", help="lista de pruebas, p.ej. fc03,latency")
    ap.add_argument("--soak", type=int, default=0, metavar="SEG", help="prueba de duracion en vez de la bateria")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    if a.list:
        for k, n, _ in TESTS:
            print(f"  {k:<12} {n}")
        return
    ctx = Ctx(a)
    if a.soak:
        ok = soak(ctx, a.soak)
    else:
        ok = run_suite(ctx, set(a.only.split(",")) if a.only else None)
    print("\nRESULTADO:", "\033[32mOK\033[0m" if ok else "\033[31mFALLAS\033[0m")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
