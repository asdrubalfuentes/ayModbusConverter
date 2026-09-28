"""
Servidor de prueba de la interfaz web SIN la placa.
Sirve la UI embebida en src/web_ui.h y simula la API REST del firmware
(incluidas las herramientas de escaneo / autodeteccion / exploracion con
progreso, la tabla de sondeo, el aprendizaje y el asistente).

    python test/webmock/mock.py            ->  http://127.0.0.1:8765/
    python test/webmock/mock.py --wizard   ->  arranca como equipo nuevo (abre el asistente)
"""
import json, http.server, time, random, os, sys, copy

HERE = os.path.dirname(os.path.abspath(__file__))


def ui():
    src = open(os.path.join(HERE, '..', '..', 'src', 'web_ui.h'), encoding='utf-8').read()
    return src[src.index('R"UI(') + 5:src.index(')UI"')].encode('utf-8')


T0 = time.time()
PORT0 = {"en": True, "name": "RS-485", "baud": 9600, "data": 8, "parity": "N", "stop": 1, "timeout": 1000, "charTo": 0,
         "turn": 20, "bcast": 100, "retries": 0, "uidFrom": 1, "uidTo": 247, "uidOff": 0, "offAfter": 3, "offRetry": 10,
         "cache": False, "maxAge": 2000}
PORT1 = dict(PORT0, en=False, name="RS-232", uidFrom=101, uidTo=147, uidOff=-100)
CFG = {
    "sys": {"name": "ayMC-A1B2C3", "user": "admin", "pass": "", "apiKey": "", "apiKeySet": False,
            "wizard": "--wizard" not in sys.argv, "oledEn": True, "oledFlip": False, "oledSleep": 10,
            "oledContrast": 180, "otaAuto": False, "ntp": "pool.ntp.org", "tz": "<-04>4<-03>,M9.1.6/24,M4.1.6/24"},
    "net": {"host": "aymc-a1b2c3", "mdns": True, "eth": {"dhcp": True, "ip": "", "mask": "", "gw": "", "dns1": "", "dns2": ""},
            "ap": {"mode": 1, "ssid": "ayModbus-A1B2C3", "pass": "", "ch": 6}},
    "ports": [PORT0, PORT1],
    "mb": {"proto": 0, "port": 502, "maxCli": 4, "idle": 300, "exc": True, "uidMode": 0, "fixUid": 1, "uid0Bc": True,
           "alive": 60, "initDelay": 0, "allowEn": False, "allow": [{"ip": "", "mask": ""}] * 4},
    "polls": [{"en": True, "port": 0, "uid": 1, "fc": 3, "addr": 0, "qty": 10, "period": 1000, "mqtt": True, "name": "medidor1"}],
    "ro": [],
    "mqtt": {"en": False, "host": "emqx.aysafi.com", "port": 8883, "tls": True, "user": "", "pass": "", "base": "",
             "baseEff": "aysafi/aymc-a1b2c3", "statusS": 30, "retain": True, "cmd": False},
}
req = [0]
seq = [0]
learn = {"en": False, "items": []}
TOOL = {"kind": ""}


def tool_tick():
    t = TOOL
    if not t.get("kind") or not t.get("active"):
        return
    t["progress"] = min(t["total"], t["progress"] + (2 if t["kind"] == "detect" else 5))
    k = t["kind"]
    if k == "scan":
        cur = t["from"] + t["progress"] - 1
        t["status"] = f"Unit ID {cur}"
        t["found"] = [u for u in (1, 2, 10) if t["from"] <= u <= min(cur, t["to"])]
    elif k == "detect":
        combos = [(b, p) for b in (9600, 19200, 38400, 115200, 4800, 57600, 2400, 1200) for p in "NEO"]
        b, p = combos[min(t["progress"], len(combos) - 1)]
        t["status"] = f"{b} 8{p}1"
        if t["progress"] >= 4:
            t.update(found=True, baud=19200, parity="E", uid=1, status="Encontrado: 19200 8E1, ID 1", progress=t["total"])
    elif k == "discover":
        t["status"] = f"FC03 direccion {t['progress'] * 100}"
        if t["progress"] >= t["total"]:
            t.update(vendor="Eastron", product="SDM630", rev="2.1",
                     ranges=[{"fc": 3, "from": 0, "to": 199}, {"fc": 4, "from": 0, "to": 399}])
    if t["progress"] >= t["total"]:
        t["active"] = False
        if k == "scan":
            t["status"] = f"Terminado: {len(t['found'])} esclavo(s)"
        elif k == "detect" and not t.get("found"):
            t["status"] = "Ningun equipo respondio"
        elif k == "discover":
            t["status"] = "Exploracion terminada"


def status():
    req[0] += random.randint(5, 15)
    if learn["en"] and not learn["items"]:
        learn["items"] = [{"port": 0, "uid": 1, "fc": 3, "addr": 0, "qty": 10, "count": 42, "period": 1000, "ago": 0},
                          {"port": 0, "uid": 2, "fc": 4, "addr": 100, "qty": 4, "count": 21, "period": 2000, "ago": 1}]
    tool_tick()
    return {"name": CFG["sys"]["name"], "fw": "1.1.0", "product": "ayModbusConverter", "uptime": int(time.time() - T0) + 3600,
            "heap": 180000, "minHeap": 160000, "chip": "A1B2C3", "defPass": True, "mon": False,
            "wizard": CFG["sys"]["wizard"], "state": "FALLAS", "mqtt": "desactivado",
            "net": {"host": CFG["net"]["host"], "eth": {"link": True, "up": True, "ip": "192.168.1.50", "speed": 100, "fd": True,
                    "mac": "A0:B7:65:A1:B2:C3", "dhcp": True, "mask": "255.255.255.0", "gw": "192.168.1.1", "dns": "192.168.1.1"},
                    "ap": {"up": True, "ip": "192.168.4.1", "ssid": "ayModbus-A1B2C3", "sta": 0}},
            "ota": {"state": "", "latest": ""},
            "gw": {"listening": True, "state": "FALLAS", "learn": learn["en"],
                   "stats": {"tcpReq": req[0], "ok": req[0] - 3, "exc": 1, "timeout": 3, "crc": 0, "bad": 0, "bcast": 0, "gwExc": 3,
                             "fast": 12, "cache": req[0] // 3, "polls": req[0] // 2, "rejected": 0, "conn": 2, "rx": 1000, "tx": 2000, "lat": 38},
                   "ports": [{"name": p["name"], "en": p["en"], "state": ("FALLAS" if i == 0 else "APAGADO") if p["en"] else "APAGADO",
                              "fmt": f"{p['baud']} {p['data']}{p['parity']}{p['stop']}", "gapUs": 4010, "queue": 0, "cache": p["cache"]}
                             for i, p in enumerate(CFG["ports"])],
                   "clients": [{"ip": "192.168.1.20", "port": 51234, "up": 120, "idle": 0, "reqs": req[0]}],
                   "slaves": [{"port": 0, "uid": 1, "state": "OK", "req": req[0] - 3, "ok": req[0] - 4, "exc": 1, "tout": 0, "err": 0, "fast": 0, "lat": 38, "ago": 0, "ident": "Eastron SDM630"},
                              {"port": 0, "uid": 5, "state": "DESCONECTADO", "req": 15, "ok": 0, "exc": 0, "tout": 3, "err": 0, "fast": 12, "lat": 0, "ago": -1}]}}


class H(http.server.BaseHTTPRequestHandler):
    def j(s, o, c=200):
        b = json.dumps(o).encode()
        s.send_response(c)
        s.send_header('Content-Type', 'application/json')
        s.end_headers()
        s.wfile.write(b)

    def log_message(s, *a):
        pass

    def body(s):
        n = int(s.headers.get('Content-Length', 0))
        raw = s.rfile.read(n)
        try:
            return json.loads(raw or b'{}')
        except Exception:
            return {}

    def do_GET(s):
        p = s.path
        if p == '/' or p.startswith('/#'):
            b = ui()
            s.send_response(200)
            s.send_header('Content-Type', 'text/html; charset=utf-8')
            s.end_headers()
            s.wfile.write(b)
            return
        if p.startswith('/api/config'): return s.j(CFG)
        if p.startswith('/api/status'): return s.j(status())
        if p.startswith('/api/tool'): tool_tick(); return s.j(TOOL)
        if p.startswith('/api/learn'):
            status()   # genera filas de ejemplo si esta aprendiendo
            return s.j({"en": learn["en"], "items": learn["items"]})
        if p.startswith('/api/tags'):
            return s.j({"tags": [dict(pl, i=i, q="ok", age=random.randint(10, 900), ok=10, fail=0,
                                      v=[(pl["uid"] * 1000 + pl["addr"] + k) & 0xFFFF for k in range(pl["qty"])])
                                 for i, pl in enumerate(CFG["polls"])]})
        if p.startswith('/api/monitor'):
            seq[0] += 2
            t = int((time.time() - T0) * 1000)
            return s.j({"en": True, "last": seq[0], "items": [
                {"s": seq[0] - 1, "t": t, "k": "TCP>", "n": 12, "h": "00 01 00 00 00 06 01 03 00 00 00 0A"},
                {"s": seq[0], "t": t + 5, "k": ">RTU", "p": 1, "n": 8, "h": "01 03 00 00 00 0A C5 CD"}]})
        s.j({"ok": False, "error": "404"}, 404)

    def do_POST(s):
        b = s.body()
        p = s.path
        if p == '/api/test':
            return s.j({"ok": True, "result": "OK", "port": 1, "lat": 35, "raw": "01 03 04 00 7B FF FE", "values": [123, 65534]})
        if p == '/api/tool':
            k = b.get("kind")
            if k == "stop":
                TOOL["active"] = False
                TOOL["status"] = "Detenido"
            elif k == "apply":
                pi = TOOL.get("port", 0)
                CFG["ports"][pi].update(baud=TOOL.get("baud", 9600), parity=TOOL.get("parity", "N"), en=True)
            else:
                TOOL.clear()
                TOOL.update(kind=k, port=b.get("port", 1) - 1, active=True, progress=0, status="", found=[] if k == "scan" else False)
                TOOL["total"] = {"scan": b.get("to", 247) - b.get("from", 1) + 1, "detect": 24, "discover": 21}[k]
                TOOL.update({kk: b[kk] for kk in ("from", "to", "uid") if kk in b})
                if k == "discover":
                    TOOL.update(vendor="", product="", rev="", step=b.get("step", 100), ranges=[])
            return s.j({"ok": True})
        if p == '/api/learn':
            if "en" in b: learn["en"] = b["en"]
            if b.get("clear"): learn["items"] = []
            return s.j({"ok": True})
        if p == '/api/port/apply':
            CFG["ports"][b["port"] - 1].update(b.get("cfg", {}))
            return s.j({"ok": True})
        if p == '/api/wizard/done':
            CFG["sys"]["wizard"] = True
            return s.j({"ok": True})
        if p == '/api/config':
            for k, v in b.items():
                if isinstance(v, dict) and isinstance(CFG.get(k), dict):
                    CFG[k].update(v)
                else:
                    CFG[k] = v
            return s.j({"ok": True, "reboot": True})
        s.j({"ok": True})


print('UI de prueba en http://127.0.0.1:8765/')
http.server.ThreadingHTTPServer(('127.0.0.1', 8765), H).serve_forever()
