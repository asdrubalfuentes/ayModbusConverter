"""
Servidor de prueba de la interfaz web SIN la placa.
Sirve la UI embebida en src/web_ui.h y simula la API REST del firmware.

    python test/webmock/mock.py      ->  http://127.0.0.1:8765/
"""
import json, http.server, time, random, os
HERE=os.path.dirname(os.path.abspath(__file__))
def ui():
    src=open(os.path.join(HERE,'..','..','src','web_ui.h'),encoding='utf-8').read()
    return src[src.index('R"UI(')+5:src.index(')UI"')].encode('utf-8')
T0=time.time()
CFG={"sys":{"name":"ayMC-A1B2C3","user":"admin","pass":"","oledEn":True,"oledFlip":False,"oledSleep":10,"oledContrast":180,"otaAuto":False,"ntp":"pool.ntp.org","tz":"<-04>4<-03>,M9.1.6/24,M4.1.6/24"},
"net":{"host":"aymc-a1b2c3","mdns":True,"eth":{"dhcp":True,"ip":"","mask":"","gw":"","dns1":"","dns2":""},"ap":{"mode":1,"ssid":"ayModbus-A1B2C3","pass":"","ch":6}},
"ser":{"baud":9600,"data":8,"parity":"N","stop":1,"timeout":1000,"charTo":0,"turn":20,"bcast":100,"retries":0,"initDelay":0},
"mb":{"proto":0,"port":502,"maxCli":4,"idle":300,"exc":True,"uidMode":0,"fixUid":1,"uid0Bc":True,"mapFrom":1,"mapTo":247,"mapOff":0,"alive":60,"allowEn":False,"allow":[{"ip":"","mask":""}]*4}}
req=[0]; seq=[0]
class H(http.server.BaseHTTPRequestHandler):
    def j(s,o,c=200):
        b=json.dumps(o).encode(); s.send_response(c); s.send_header('Content-Type','application/json'); s.end_headers(); s.wfile.write(b)
    def log_message(s,*a): pass
    def do_GET(s):
        if s.path=='/' : b=ui(); s.send_response(200); s.send_header('Content-Type','text/html; charset=utf-8'); s.end_headers(); s.wfile.write(b); return
        if s.path.startswith('/api/config'): return s.j(CFG)
        if s.path.startswith('/api/status'):
            req[0]+=random.randint(5,15)
            return s.j({"name":"ayMC-A1B2C3","fw":"1.0.0","product":"ayModbusConverter","uptime":int(time.time()-T0)+3600,"heap":180000,"minHeap":160000,"chip":"A1B2C3","defPass":True,"mon":False,
             "net":{"host":"aymc-a1b2c3","eth":{"link":True,"up":True,"ip":"192.168.1.50","speed":100,"fd":True,"mac":"A0:B7:65:A1:B2:C3","dhcp":True,"mask":"255.255.255.0","gw":"192.168.1.1","dns":"192.168.1.1"},"ap":{"up":True,"ip":"192.168.4.1","ssid":"ayModbus-A1B2C3","sta":0}},
             "ota":{"state":"","latest":""},
             "gw":{"listening":True,"stats":{"tcpReq":req[0],"ok":req[0]-3,"exc":1,"timeout":3,"crc":0,"bad":0,"bcast":0,"gwExc":3,"rejected":0,"conn":2,"rx":1000,"tx":2000,"lat":38,"gapUs":4010},
               "clients":[{"ip":"192.168.1.20","port":51234,"up":120,"idle":0,"reqs":req[0]}],
               "slaves":[{"uid":1,"req":req[0]-3,"ok":req[0]-4,"exc":1,"tout":0,"err":0,"lat":38,"ago":0},{"uid":5,"req":3,"ok":0,"exc":0,"tout":3,"err":0,"lat":0,"ago":-1}]}})
        if s.path.startswith('/api/monitor'):
            seq[0]+=2
            return s.j({"en":True,"last":seq[0],"items":[{"s":seq[0]-1,"t":int((time.time()-T0)*1000),"k":"TCP>","n":12,"h":"00 01 00 00 00 06 01 03 00 00 00 0A"},{"s":seq[0],"t":int((time.time()-T0)*1000)+5,"k":">RTU","n":8,"h":"01 03 00 00 00 0A C5 CD"}]})
        if s.path.startswith('/api/scan'): return s.j({"active":False,"from":1,"to":10,"cur":10,"found":[1,3]})
        s.j({"ok":False,"error":"404"},404)
    def do_POST(s):
        n=int(s.headers.get('Content-Length',0)); body=s.rfile.read(n)
        if s.path=='/api/test': return s.j({"ok":True,"result":"OK","lat":35,"raw":"01 03 04 00 7B FF FE","values":[123,65534]})
        s.j({"ok":True})
print('UI de prueba en http://127.0.0.1:8765/')
http.server.ThreadingHTTPServer(('127.0.0.1',8765),H).serve_forever()
