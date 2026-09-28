#pragma once
#include <Arduino.h>

// Interfaz web de configuracion (SPA sin dependencias externas: funciona en
// el AP de configuracion sin internet). Habla con la API REST de web.cpp.
static const char UI_HTML[] PROGMEM = R"UI(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ayModbusConverter</title>
<style>
:root{--bg:#F7F5F1;--panel:#FFFFFF;--panel2:#EAF2F0;--ink:#14313C;--mute:#5E7A76;--line:#D5E1DE;
--brand:#14313C;--brand2:#0D2830;--acc:#F77535;--acc2:#FFA167;--ok:#378676;--okb:#DDF3EC;--bad:#E5484D;--badb:#FDE4E4;--warn:#B8621B;--warnb:#FFF0E3;--mint:#6FD6C0}
@media (prefers-color-scheme:dark){:root{--bg:#0A1E24;--panel:#0F2A32;--panel2:#14343D;--ink:#E7EFED;--mute:#8AA29C;--line:#1F434D;
--okb:#143A33;--badb:#3A1A1D;--warnb:#3A2716}}
*{box-sizing:border-box}html,body{margin:0}
body{font:14px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--ink)}
header{background:var(--brand);color:#fff;display:flex;align-items:center;gap:14px;padding:10px 18px;position:sticky;top:0;z-index:5}
.logo{font-weight:800;font-size:20px;letter-spacing:.3px}.logo b{color:var(--acc)}
.prod{font-size:13px;opacity:.85;border-left:1px solid #ffffff40;padding-left:14px}
.hsp{flex:1}.pill{font-size:12px;padding:3px 10px;border-radius:99px;background:#ffffff1a;white-space:nowrap}
.pill.ok{background:#378676}.pill.bad{background:#E5484D}
.wrap{display:flex;min-height:calc(100vh - 50px)}
nav{width:220px;flex:none;background:var(--panel);border-right:1px solid var(--line);padding:12px 8px}
nav a{display:flex;gap:10px;align-items:center;padding:9px 12px;border-radius:8px;color:var(--ink);text-decoration:none;margin-bottom:2px;cursor:pointer}
nav a:hover{background:var(--panel2)}nav a.on{background:var(--brand);color:#fff}
nav a.on svg{stroke:var(--acc2)}nav svg{width:18px;height:18px;stroke:currentColor;fill:none;stroke-width:1.8}
main{flex:1;min-width:0;padding:20px 24px;max-width:1100px}
h1{font-size:20px;margin:0 0 4px}.sub{color:var(--mute);margin:0 0 18px}
h2{font-size:15px;margin:0 0 12px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:16px 18px;margin-bottom:16px}
.grid{display:grid;gap:14px;grid-template-columns:repeat(auto-fill,minmax(170px,1fr))}
.kpi{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px 14px}
.kpi .l{font-size:12px;color:var(--mute)}.kpi .v{font-size:22px;font-weight:700;margin-top:2px;font-variant-numeric:tabular-nums}
.kpi .s{font-size:12px;color:var(--mute)}
.fg{display:grid;gap:12px 16px;grid-template-columns:repeat(auto-fill,minmax(210px,1fr))}
label.f{display:block}label.f>span{display:block;font-size:12.5px;font-weight:600;margin-bottom:4px}
label.f small{display:block;color:var(--mute);font-size:11.5px;margin-top:3px}
input,select{width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--ink);font:inherit}
input:focus,select:focus{outline:2px solid var(--acc2);border-color:var(--acc)}
.chk{display:flex;gap:9px;align-items:flex-start;padding:6px 0}.chk input{width:auto;margin-top:3px}
.btn{display:inline-flex;gap:6px;align-items:center;border:0;border-radius:8px;padding:9px 16px;font:inherit;font-weight:600;cursor:pointer;background:var(--brand);color:#fff}
.btn.acc{background:var(--acc)}.btn.gh{background:transparent;color:var(--ink);border:1px solid var(--line)}
.btn.red{background:var(--bad)}.btn:disabled{opacity:.5;cursor:default}
.bar{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.actions{position:sticky;bottom:0;background:linear-gradient(transparent,var(--bg) 30%);padding:18px 0 8px;display:flex;gap:10px;justify-content:flex-end}
table{width:100%;border-collapse:collapse;font-size:13px}th,td{text-align:left;padding:7px 8px;border-bottom:1px solid var(--line)}
th{font-size:12px;color:var(--mute);font-weight:600}td.num,th.num{text-align:right;font-variant-numeric:tabular-nums}
.mono{font-family:ui-monospace,Consolas,monospace;font-size:12.5px}
.tag{display:inline-block;font-size:11.5px;font-weight:700;padding:2px 7px;border-radius:5px;background:var(--panel2)}
.tag.ok{background:var(--okb);color:var(--ok)}.tag.bad{background:var(--badb);color:var(--bad)}.tag.w{background:var(--warnb);color:var(--warn)}
.note{padding:10px 14px;border-radius:8px;background:var(--warnb);color:var(--warn);margin-bottom:16px;font-size:13px}
.note.info{background:var(--panel2);color:var(--ink)}
.empty{color:var(--mute);padding:14px 0;text-align:center}
.mon{max-height:480px;overflow:auto;border:1px solid var(--line);border-radius:8px}
.mon td{white-space:nowrap}.mon td.h{white-space:normal;word-break:break-all}
.k-TCP\>{color:#2F7FC1}.dir{font-weight:700;font-size:12px}
.prog{height:8px;background:var(--panel2);border-radius:99px;overflow:hidden}.prog>i{display:block;height:100%;background:var(--acc);width:0}
.chips{display:flex;flex-wrap:wrap;gap:6px}.chip{padding:4px 10px;border-radius:99px;background:var(--okb);color:var(--ok);font-weight:700;cursor:pointer}
.toast{position:fixed;right:18px;bottom:18px;background:var(--brand);color:#fff;padding:10px 16px;border-radius:8px;opacity:0;transform:translateY(10px);transition:.2s;z-index:20;max-width:80vw}
.toast.on{opacity:1;transform:none}.toast.bad{background:var(--bad)}
.ov{position:fixed;inset:0;background:#0A1E24d0;display:none;align-items:center;justify-content:center;z-index:30}
.ov.on{display:flex}.ovb{background:var(--panel);color:var(--ink);border-radius:14px;padding:24px 26px;max-width:420px;width:90%;text-align:center}
.spin{width:36px;height:36px;border:4px solid var(--panel2);border-top-color:var(--acc);border-radius:50%;animation:sp 1s linear infinite;margin:0 auto 12px}
@keyframes sp{to{transform:rotate(360deg)}}
.hide{display:none!important}
@media (max-width:760px){.wrap{display:block}nav{width:auto;display:flex;overflow-x:auto;border-right:0;border-bottom:1px solid var(--line);padding:6px}
nav a{white-space:nowrap;margin:0 2px}main{padding:16px}.prod{display:none}}
</style></head><body>
<header><div class="logo">ay<b>safi</b></div><div class="prod">ayModbusConverter &middot; RTU &harr; TCP</div>
<div class="hsp"></div><span class="pill" id="hName">-</span><span class="pill" id="hLink">-</span></header>
<div class="wrap">
<nav id="nav">
<a data-p="dash" class="on"><svg viewBox="0 0 24 24"><rect x="3" y="3" width="7" height="9" rx="1"/><rect x="14" y="3" width="7" height="5" rx="1"/><rect x="14" y="12" width="7" height="9" rx="1"/><rect x="3" y="16" width="7" height="5" rx="1"/></svg>Panel</a>
<a data-p="net"><svg viewBox="0 0 24 24"><rect x="9" y="2" width="6" height="6" rx="1"/><rect x="2" y="16" width="6" height="6" rx="1"/><rect x="16" y="16" width="6" height="6" rx="1"/><path d="M12 8v4M5 16v-4h14v4"/></svg>Red</a>
<a data-p="ser"><svg viewBox="0 0 24 24"><path d="M4 7h16M4 12h16M4 17h10"/><circle cx="19" cy="17" r="2"/></svg>Serie RS-485</a>
<a data-p="mb"><svg viewBox="0 0 24 24"><path d="M4 7h11l-3-3M20 17H9l3 3"/></svg>Modbus TCP</a>
<a data-p="diag"><svg viewBox="0 0 24 24"><path d="M3 12h4l3-8 4 16 3-8h4"/></svg>Diagn&oacute;stico</a>
<a data-p="sys"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M4.2 4.2l2.1 2.1M17.7 17.7l2.1 2.1M2 12h3M19 12h3M4.2 19.8l2.1-2.1M17.7 6.3l2.1-2.1"/></svg>Sistema</a>
</nav>
<main>
<div class="note hide" id="defPass">La clave de administrador es la de f&aacute;brica. C&aacute;mbiela en <b>Sistema &rarr; Acceso</b>.</div>

<!-- ============ PANEL ============ -->
<section id="p-dash">
<h1>Panel</h1><p class="sub">Estado en vivo de la pasarela transparente Modbus TCP &rarr; RTU.</p>
<div class="grid">
<div class="kpi"><div class="l">LAN</div><div class="v" id="kEth">-</div><div class="s" id="kEthS">-</div></div>
<div class="kpi"><div class="l">Servidor Modbus</div><div class="v" id="kSrv">-</div><div class="s" id="kSrvS">-</div></div>
<div class="kpi"><div class="l">Clientes TCP</div><div class="v" id="kCli">-</div><div class="s" id="kCliS">-</div></div>
<div class="kpi"><div class="l">Peticiones / s</div><div class="v" id="kRate">-</div><div class="s" id="kReq">-</div></div>
<div class="kpi"><div class="l">Tasa de &eacute;xito</div><div class="v" id="kOk">-</div><div class="s" id="kOkS">-</div></div>
<div class="kpi"><div class="l">Latencia RTU</div><div class="v" id="kLat">-</div><div class="s" id="kGap">-</div></div>
<div class="kpi"><div class="l">Timeouts / CRC</div><div class="v" id="kErr">-</div><div class="s" id="kErrS">-</div></div>
<div class="kpi"><div class="l">Tiempo encendido</div><div class="v" id="kUp">-</div><div class="s" id="kHeap">-</div></div>
</div>
<div class="card" style="margin-top:16px"><div class="bar" style="justify-content:space-between"><h2 style="margin:0">Esclavos RTU</h2>
<button class="btn gh" onclick="resetStats()">Reiniciar contadores</button></div>
<table><thead><tr><th>Unit ID</th><th class="num">Peticiones</th><th class="num">OK</th><th class="num">Excepciones</th><th class="num">Timeouts</th><th class="num">Errores</th><th class="num">Latencia</th><th>Estado</th></tr></thead>
<tbody id="tSlaves"></tbody></table></div>
<div class="card"><h2>Clientes TCP conectados</h2>
<table><thead><tr><th>Direcci&oacute;n</th><th class="num">Conectado</th><th class="num">Inactivo</th><th class="num">Peticiones</th></tr></thead><tbody id="tClients"></tbody></table></div>
<div class="card"><h2>Interfaces</h2><table><tbody id="tIf"></tbody></table></div>
</section>

<!-- ============ RED ============ -->
<section id="p-net" class="hide">
<h1>Red</h1><p class="sub">La LAN (Ethernet) es la conexi&oacute;n de servicio: por ella se atiende Modbus TCP y esta web. El WiFi solo levanta el portal cautivo de configuraci&oacute;n.</p>
<div class="card"><h2>LAN &mdash; estado actual</h2><div class="grid">
<div class="kpi"><div class="l">Enlace</div><div class="v" id="nLink" style="font-size:17px">-</div></div>
<div class="kpi"><div class="l">Direcci&oacute;n IP</div><div class="v mono" id="nIp" style="font-size:17px">-</div></div>
<div class="kpi"><div class="l">M&aacute;scara de red</div><div class="v mono" id="nMask" style="font-size:17px">-</div></div>
<div class="kpi"><div class="l">Puerta de enlace</div><div class="v mono" id="nGw" style="font-size:17px">-</div></div>
<div class="kpi"><div class="l">DNS</div><div class="v mono" id="nDns" style="font-size:17px">-</div></div>
<div class="kpi"><div class="l">MAC</div><div class="v mono" id="nMac" style="font-size:14px">-</div></div>
</div></div>
<div class="card"><h2>LAN &mdash; direccionamiento</h2><div class="fg">
<label class="f"><span>Modo</span><select data-k="net.eth.dhcp" data-t="b"><option value="1">Autom&aacute;tico (DHCP)</option><option value="0">IP fija (manual)</option></select><small>En planta se recomienda IP fija: los clientes Modbus apuntan a una direcci&oacute;n.</small></label>
<label class="f"><span>Nombre de host</span><input data-k="net.host" maxlength="31"><small>Se anuncia al DHCP y por mDNS como &lt;host&gt;.local</small></label>
<label class="chk"><input type="checkbox" data-k="net.mdns"><span>Anunciar por mDNS (http y modbus)</span></label>
</div><div class="fg" data-if="!c.net.eth.dhcp" style="margin-top:12px">
<label class="f"><span>Direcci&oacute;n IP</span><input data-k="net.eth.ip" placeholder="192.168.1.50" data-ip></label>
<label class="f"><span>M&aacute;scara de red</span><input data-k="net.eth.mask" placeholder="255.255.255.0" data-ip></label>
<label class="f"><span>Puerta de enlace (gateway)</span><input data-k="net.eth.gw" placeholder="192.168.1.1" data-ip></label>
<label class="f"><span>DNS primario</span><input data-k="net.eth.dns1" placeholder="(= puerta de enlace)" data-ip></label>
<label class="f"><span>DNS secundario</span><input data-k="net.eth.dns2" placeholder="8.8.8.8" data-ip></label>
</div></div>
<div class="card"><h2>WiFi &mdash; portal cautivo de configuraci&oacute;n</h2>
<p style="margin-top:0;color:var(--mute)">El equipo crea su propia red WiFi. Al conectarse desde un tel&eacute;fono o PC, esta p&aacute;gina se abre sola (o en http://192.168.4.1). No transporta tr&aacute;fico Modbus.</p><div class="fg">
<label class="f"><span>Activaci&oacute;n</span><select data-k="net.ap.mode" data-t="n"><option value="1">Siempre activo</option><option value="0">Autom&aacute;tico (sin LAN a los 30 s)</option><option value="2">Solo con el bot&oacute;n (3 s)</option></select></label>
<label class="f"><span>Nombre de la red (SSID)</span><input data-k="net.ap.ssid" maxlength="32"></label>
<label class="f"><span>Clave WPA2 (m&iacute;n. 8)</span><input type="password" data-k="net.ap.pass" maxlength="64" placeholder="(sin cambios)"></label>
<label class="f"><span>Canal</span><input type="number" min="1" max="13" data-k="net.ap.ch"></label>
</div></div>
<div class="actions"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>
</section>

<!-- ============ SERIE ============ -->
<section id="p-ser" class="hide">
<h1>Puerto serie RS-485</h1><p class="sub">Debe coincidir con la configuraci&oacute;n de los esclavos Modbus RTU del bus.</p>
<div class="card"><h2>Par&aacute;metros de l&iacute;nea</h2><div class="fg">
<label class="f"><span>Velocidad (baudios)</span><select data-k="ser.baud" data-t="n">
<option>1200</option><option>2400</option><option>4800</option><option>9600</option><option>14400</option><option>19200</option><option>38400</option><option>57600</option><option>115200</option><option>230400</option></select></label>
<label class="f"><span>Bits de datos</span><select data-k="ser.data" data-t="n"><option value="8">8</option><option value="7">7</option></select><small>Modbus RTU usa 8.</small></label>
<label class="f"><span>Paridad</span><select data-k="ser.parity"><option value="N">Ninguna</option><option value="E">Par (Even)</option><option value="O">Impar (Odd)</option></select></label>
<label class="f"><span>Bits de parada</span><select data-k="ser.stop" data-t="n"><option value="1">1</option><option value="2">2</option></select><small>Sin paridad el est&aacute;ndar pide 2.</small></label>
</div></div>
<div class="card"><h2>Temporizaci&oacute;n</h2><div class="fg">
<label class="f"><span>Timeout de respuesta (ms)</span><input type="number" min="20" max="10000" data-k="ser.timeout"><small>Espera m&aacute;xima de la respuesta del esclavo.</small></label>
<label class="f"><span>Reintentos</span><input type="number" min="0" max="5" data-k="ser.retries"><small>Ante timeout o CRC inv&aacute;lido.</small></label>
<label class="f"><span>Fin de trama (ms)</span><input type="number" min="0" max="500" data-k="ser.charTo"><small>0 = autom&aacute;tico (t3.5 del est&aacute;ndar). Actual: <b id="gapNow">-</b></small></label>
<label class="f"><span>Pausa entre peticiones (ms)</span><input type="number" min="0" max="5000" data-k="ser.turn"><small>Silencio m&iacute;nimo entre transacciones del bus.</small></label>
<label class="f"><span>Pausa tras broadcast (ms)</span><input type="number" min="0" max="5000" data-k="ser.bcast"><small>Unit ID 0: los esclavos no responden.</small></label>
<label class="f"><span>Retardo inicial (ms)</span><input type="number" min="0" max="60000" data-k="ser.initDelay"><small>Espera tras el arranque antes de atender, para esclavos lentos en encender.</small></label>
</div></div>
<div class="note info">Cableado WT32-ETH01: <b>IO5</b> &larr; RO, <b>IO17</b> &rarr; DI, <b>IO33 (485_EN)</b> &rarr; DE+/RE del transceptor. Termine el bus con 120&nbsp;&Omega; en ambos extremos.</div>
<div class="actions"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>
</section>

<!-- ============ MODBUS ============ -->
<section id="p-mb" class="hide">
<h1>Modbus TCP</h1><p class="sub">Servidor transparente: cada petici&oacute;n se reenv&iacute;a al bus RTU y la respuesta del esclavo vuelve sin modificar.</p>
<div class="card"><h2>Servidor</h2><div class="fg">
<label class="f"><span>Protocolo</span><select data-k="mb.proto" data-t="n"><option value="0">Modbus TCP (MBAP)</option><option value="1">Modbus RTU sobre TCP</option></select><small>RTU sobre TCP: trama RTU con CRC encapsulada.</small></label>
<label class="f"><span>Puerto TCP</span><input type="number" min="1" max="65535" data-k="mb.port"></label>
<label class="f"><span>Conexiones simult&aacute;neas</span><input type="number" min="1" max="8" data-k="mb.maxCli"></label>
<label class="f"><span>Cierre por inactividad (s)</span><input type="number" min="0" max="65535" data-k="mb.idle"><small>0 = nunca.</small></label>
<label class="f"><span>TCP alive check (s)</span><input type="number" min="0" max="7200" data-k="mb.alive"><small>Keep-alive: libera conexiones de clientes ca&iacute;dos. 0 = off.</small></label>
</div>
<label class="chk"><input type="checkbox" data-k="mb.exc"><span>Responder excepciones de pasarela (0x0B &laquo;el esclavo no responde&raquo;, 0x0A &laquo;ruta no disponible&raquo;). Si se desactiva, el cliente ver&aacute; su propio timeout.</span></label>
</div>
<div class="card"><h2>Unit ID</h2><div class="fg">
<label class="f"><span>Tratamiento</span><select data-k="mb.uidMode" data-t="n"><option value="0">Transparente (Unit ID = esclavo RTU)</option><option value="1">Esclavo fijo</option><option value="2">Mapeo por rango (ID virtual + offset)</option></select></label>
<label class="f"><span>Esclavo fijo / destino de Unit ID 255</span><input type="number" min="1" max="247" data-k="mb.fixUid"></label>
</div><div class="fg" data-if="c.mb.uidMode==2" style="margin-top:12px">
<label class="f"><span>Unit ID TCP desde</span><input type="number" min="1" max="247" data-k="mb.mapFrom"></label>
<label class="f"><span>Unit ID TCP hasta</span><input type="number" min="1" max="247" data-k="mb.mapTo"></label>
<label class="f"><span>Offset</span><input type="number" min="-246" max="246" data-k="mb.mapOff"><small>Esclavo RTU = Unit ID + offset. Fuera de rango: excepci&oacute;n 0x0A.</small></label>
</div>
<label class="chk"><input type="checkbox" data-k="mb.uid0Bc"><span>Unit ID 0 = broadcast (sin respuesta). Si se desactiva, el Unit ID 0 se env&iacute;a al esclavo fijo.</span></label>
</div>
<div class="card"><h2>Control de acceso</h2>
<label class="chk"><input type="checkbox" data-k="mb.allowEn"><span>Aceptar solo clientes de las redes listadas</span></label>
<table data-if="c.mb.allowEn"><thead><tr><th>#</th><th>IP / red</th><th>M&aacute;scara</th></tr></thead><tbody>
<tr><td>1</td><td><input data-k="mb.allow.0.ip" placeholder="192.168.1.0"></td><td><input data-k="mb.allow.0.mask" placeholder="255.255.255.0"></td></tr>
<tr><td>2</td><td><input data-k="mb.allow.1.ip"></td><td><input data-k="mb.allow.1.mask"></td></tr>
<tr><td>3</td><td><input data-k="mb.allow.2.ip"></td><td><input data-k="mb.allow.2.mask"></td></tr>
<tr><td>4</td><td><input data-k="mb.allow.3.ip"></td><td><input data-k="mb.allow.3.mask"></td></tr>
</tbody></table></div>
<div class="actions"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>
</section>

<!-- ============ DIAGNOSTICO ============ -->
<section id="p-diag" class="hide">
<h1>Diagn&oacute;stico</h1><p class="sub">Herramientas de puesta en marcha. Comparten el bus con el tr&aacute;fico de los clientes.</p>
<div class="card"><h2>Lectura / escritura directa</h2><div class="fg">
<label class="f"><span>Unit ID</span><input type="number" min="0" max="247" id="tUid" value="1"></label>
<label class="f"><span>Funci&oacute;n</span><select id="tFc"><option value="3">03 Leer holding registers</option><option value="4">04 Leer input registers</option><option value="1">01 Leer coils</option><option value="2">02 Leer entradas discretas</option><option value="6">06 Escribir un registro</option><option value="16">16 Escribir registros</option><option value="5">05 Escribir un coil</option></select></label>
<label class="f"><span>Direcci&oacute;n (base 0)</span><input type="number" min="0" max="65535" id="tAddr" value="0"></label>
<label class="f" id="tQtyL"><span>Cantidad</span><input type="number" min="1" max="125" id="tQty" value="10"></label>
<label class="f hide" id="tValL"><span>Valor(es)</span><input id="tVal" placeholder="123  /  1,2,3"></label>
</div><div class="bar" style="margin-top:12px"><button class="btn" onclick="runTest(this)">Ejecutar</button><span id="tRes" class="mono"></span></div>
<div id="tOut" style="margin-top:12px"></div></div>

<div class="card"><h2>Escanear el bus</h2><div class="fg">
<label class="f"><span>Desde</span><input type="number" min="1" max="247" id="sFrom" value="1"></label>
<label class="f"><span>Hasta</span><input type="number" min="1" max="247" id="sTo" value="247"></label>
<label class="f"><span>Timeout por equipo (ms)</span><input type="number" min="20" max="2000" id="sTo2" value="100"></label>
</div><div class="bar" style="margin-top:12px"><button class="btn" id="sBtn" onclick="scan()">Escanear</button><span id="sTxt" class="mono"></span></div>
<div class="prog" style="margin:12px 0"><i id="sBar"></i></div><div class="chips" id="sFound"></div>
<small style="color:var(--mute)">Consulta FC03 registro 0 en cada Unit ID; cualquier respuesta (incluida una excepci&oacute;n) indica un esclavo presente. Toque uno para probarlo.</small></div>

<div class="card"><div class="bar" style="justify-content:space-between;margin-bottom:10px"><h2 style="margin:0">Monitor de tr&aacute;fico</h2>
<div class="bar"><button class="btn" id="mBtn" onclick="monToggle()">Iniciar</button><button class="btn gh" onclick="monClear()">Limpiar</button><button class="btn gh" onclick="monCsv()">Exportar CSV</button></div></div>
<div class="mon"><table><thead><tr><th>Hora</th><th>Dir.</th><th class="num">Bytes</th><th>Trama (hex)</th><th>Detalle</th></tr></thead><tbody id="mBody"><tr><td colspan="5" class="empty">Monitor detenido.</td></tr></tbody></table></div></div>
</section>

<!-- ============ SISTEMA ============ -->
<section id="p-sys" class="hide">
<h1>Sistema</h1><p class="sub" id="sysSub">-</p>
<div class="card"><h2>Equipo</h2><div class="fg">
<label class="f"><span>Nombre del equipo</span><input data-k="sys.name" maxlength="31"><small>Se muestra en el OLED y en esta web.</small></label>
<label class="f"><span>Servidor NTP</span><input data-k="sys.ntp" maxlength="47"></label>
<label class="f"><span>Zona horaria (TZ POSIX)</span><input data-k="sys.tz" maxlength="47"><small>Chile: &lt;-04&gt;4&lt;-03&gt;,M9.1.6/24,M4.1.6/24</small></label>
</div></div>
<div class="card"><h2>Acceso</h2><div class="fg">
<label class="f"><span>Usuario administrador</span><input data-k="sys.user" maxlength="15"></label>
<label class="f"><span>Nueva clave</span><input type="password" data-k="sys.pass" maxlength="31" placeholder="(sin cambios)" id="newPass"></label>
<label class="f"><span>Repetir clave</span><input type="password" id="newPass2" placeholder="(sin cambios)"></label>
</div></div>
<div class="card"><h2>Pantalla OLED</h2><div class="fg">
<label class="chk"><input type="checkbox" data-k="sys.oledEn"><span>Pantalla habilitada</span></label>
<label class="chk"><input type="checkbox" data-k="sys.oledFlip"><span>Girar 180&deg;</span></label>
<label class="f"><span>Apagar tras (min)</span><input type="number" min="0" max="240" data-k="sys.oledSleep"><small>0 = siempre encendida. El bot&oacute;n la despierta.</small></label>
<label class="f"><span>Brillo (0-255)</span><input type="number" min="0" max="255" data-k="sys.oledContrast"></label>
</div></div>
<div class="actions" style="position:static"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>

<div class="card"><h2>Firmware</h2>
<p style="margin-top:0">Versi&oacute;n instalada: <b id="fwNow">-</b></p>
<div class="bar"><input type="file" id="fwFile" accept=".bin" style="max-width:320px"><button class="btn" onclick="fwUpload(this)">Subir firmware</button></div>
<div class="prog" style="margin:12px 0"><i id="fwBar"></i></div>
<div class="bar"><label class="chk"><input type="checkbox" data-k="sys.otaAuto"><span>Buscar actualizaciones en GitHub al arrancar</span></label></div>
<div class="bar"><button class="btn gh" onclick="ota(false)">Buscar actualizaci&oacute;n</button><button class="btn gh" id="otaApply" onclick="ota(true)">Instalar desde GitHub</button><span id="otaTxt" class="mono"></span></div>
</div>
<div class="card"><h2>Respaldo de configuraci&oacute;n</h2><div class="bar">
<a class="btn gh" href="/api/backup" download>Descargar respaldo</a>
<input type="file" id="rsFile" accept=".json" style="max-width:260px"><button class="btn gh" onclick="restore()">Restaurar</button></div>
<small style="color:var(--mute)">El respaldo incluye las claves. Gu&aacute;rdelo en un lugar seguro.</small></div>
<div class="card"><h2>Mantenimiento</h2><div class="bar">
<button class="btn gh" onclick="reboot()">Reiniciar</button><button class="btn red" onclick="factory()">Restaurar valores de f&aacute;brica</button></div></div>
</section>
</main></div>
<div class="toast" id="toast"></div>
<div class="ov" id="ov"><div class="ovb"><div class="spin"></div><h2 id="ovT">Reiniciando</h2><p id="ovM" style="margin:0;color:var(--mute)">Reconectando con el equipo&hellip;</p></div></div>
<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
let C=null,S=null,page='dash',prevReq=null,prevT=0,monOn=false,monSince=0,monRows=[],scanT=null;
const FC={1:'Leer coils',2:'Leer entradas',3:'Leer holding',4:'Leer input',5:'Escribir coil',6:'Escribir registro',15:'Escribir coils',16:'Escribir registros',8:'Diagn&oacute;stico',17:'ID esclavo',23:'Leer/escribir'};
const EXC={1:'Funci&oacute;n ilegal',2:'Direcci&oacute;n ilegal',3:'Valor ilegal',4:'Falla del esclavo',5:'ACK',6:'Esclavo ocupado',10:'Ruta no disponible',11:'Esclavo no responde'};
function toast(m,bad){const t=$('#toast');t.innerHTML=m;t.className='toast on'+(bad?' bad':'');clearTimeout(t._h);t._h=setTimeout(()=>t.className='toast',3200)}
async function api(p,o={}){if(o.body&&typeof o.body!=='string'&&!(o.body instanceof FormData)){o.body=JSON.stringify(o.body);o.headers={'Content-Type':'application/json'}}
const r=await fetch(p,o);let j={};try{j=await r.json()}catch(e){}if(!r.ok||j.ok===false)throw new Error(j.error||('HTTP '+r.status));return j}
const get=(o,p)=>p.split('.').reduce((a,k)=>a==null?a:a[k],o);
function set(o,p,v){const k=p.split('.');let a=o;for(let i=0;i<k.length-1;i++)a=a[k[i]];a[k[k.length-1]]=v}
function fill(){$$('[data-k]').forEach(e=>{const v=get(C,e.dataset.k);if(e.type==='checkbox')e.checked=!!v;else if(e.dataset.t==='b')e.value=v?'1':'0';else e.value=v==null?'':v});cond()}
function collect(){const c=JSON.parse(JSON.stringify(C));$$('[data-k]').forEach(e=>{let v;if(e.type==='checkbox')v=e.checked;else if(e.dataset.t==='b')v=e.value==='1';else if(e.dataset.t==='n'||e.type==='number')v=Number(e.value);else v=e.value.trim();set(c,e.dataset.k,v)});return c}
function cond(){const c=collect();$$('[data-if]').forEach(e=>{let ok=true;try{ok=new Function('c','return '+e.dataset.if)(c)}catch(x){}e.classList.toggle('hide',!ok)})}
document.addEventListener('change',e=>{if(e.target.dataset.k)cond()});
function show(p){page=p;$$('nav a').forEach(a=>a.classList.toggle('on',a.dataset.p===p));$$('main>section').forEach(s=>s.classList.toggle('hide',s.id!=='p-'+p));location.hash=p;if(p!=='diag'&&monOn)monToggle()}
$$('nav a').forEach(a=>a.onclick=()=>show(a.dataset.p));
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return d?d+'d '+h+'h':h?h+'h '+m+'m':m+'m '+(s%60)+'s'}
const n=v=>Number(v).toLocaleString('es');
async function refresh(){try{S=await api('/api/status')}catch(e){$('#hLink').textContent='sin conexión';$('#hLink').className='pill bad';return}
const g=S.gw,st=g.stats,e=S.net.eth;$('#hName').textContent=S.name;
$('#hLink').textContent=e.link?('LAN '+e.speed+'M'):(S.net.ap.up?'solo AP':'sin red');$('#hLink').className='pill '+(e.up?'ok':'bad');
$('#nLink').textContent=e.link?(e.speed+' Mbps '+(e.fd?'full':'half')):'sin cable';$('#nIp').textContent=e.ip+(e.up?(e.dhcp?' (DHCP)':' (fija)'):'');
$('#nMask').textContent=e.mask;$('#nGw').textContent=e.gw;$('#nDns').textContent=e.dns;$('#nMac').textContent=e.mac;
$('#defPass').classList.toggle('hide',!S.defPass);
$('#kEth').textContent=e.up?e.ip:(e.link?'sin IP':'sin cable');$('#kEthS').textContent=e.link?(e.speed+' Mbps '+(e.fd?'full':'half')+'-duplex'):'Conecte el cable de red';
$('#kSrv').textContent=g.listening?(C?(C.mb.proto?'RTU/TCP':'TCP')+' :'+C.mb.port:'activo'):'detenido';
$('#kSrvS').textContent=C?('RS-485 '+C.ser.baud+' '+C.ser.data+C.ser.parity+C.ser.stop):'';
$('#kCli').textContent=g.clients.length+(C?' / '+C.mb.maxCli:'');$('#kCliS').textContent=n(st.conn)+' conexiones, '+n(st.rejected)+' rechazadas';
const now=Date.now();if(prevReq!=null&&now>prevT){$('#kRate').textContent=((st.tcpReq-prevReq)*1000/(now-prevT)).toFixed(1)}prevReq=st.tcpReq;prevT=now;
$('#kReq').textContent=n(st.tcpReq)+' peticiones';
const tot=st.ok+st.timeout+st.crc+st.bad;$('#kOk').textContent=tot?(100*st.ok/tot).toFixed(1)+'%':'-';$('#kOkS').textContent=n(st.exc)+' excepciones del esclavo';
$('#kLat').textContent=st.lat+' ms';$('#kGap').textContent='fin de trama '+(st.gapUs/1000).toFixed(1)+' ms';$('#gapNow').textContent=(st.gapUs/1000).toFixed(1)+' ms';
$('#kErr').textContent=n(st.timeout)+' / '+n(st.crc);$('#kErrS').textContent=n(st.gwExc)+' excepciones de pasarela';
$('#kUp').textContent=dur(S.uptime);$('#kHeap').textContent='memoria libre '+Math.round(S.heap/1024)+' kB';
$('#tSlaves').innerHTML=g.slaves.length?g.slaves.map(s=>{const r=s.req?s.ok/s.req:0,ok=s.ago>=0&&s.ago<60&&r>.5;return `<tr><td><b>${s.uid}</b></td><td class=num>${n(s.req)}</td><td class=num>${n(s.ok)}</td><td class=num>${n(s.exc)}</td><td class=num>${n(s.tout)}</td><td class=num>${n(s.err)}</td><td class=num>${s.lat} ms</td><td><span class="tag ${ok?'ok':(s.ago<0?'bad':'w')}">${ok?'responde':(s.ago<0?'sin respuesta':'hace '+dur(s.ago))}</span></td></tr>`}).join(''):'<tr><td colspan=8 class=empty>Todavía no hay tráfico hacia el bus.</td></tr>';
$('#tClients').innerHTML=g.clients.length?g.clients.map(c=>`<tr><td class=mono>${c.ip}:${c.port}</td><td class=num>${dur(c.up)}</td><td class=num>${dur(c.idle)}</td><td class=num>${n(c.reqs)}</td></tr>`).join(''):'<tr><td colspan=4 class=empty>Sin clientes conectados.</td></tr>';
const ap=S.net.ap;
$('#tIf').innerHTML=`<tr><th>LAN</th><td class=mono>${e.up?e.ip+' / '+e.mask:'-'}</td><td class=mono>gw ${e.gw}</td><td>${e.link?'<span class="tag ok">enlace</span>':'<span class="tag bad">sin cable</span>'}</td></tr>`+
`<tr><th>Portal WiFi</th><td class=mono>${ap.up?ap.ip:'-'}</td><td>${esc(ap.ssid)}</td><td>${ap.up?'<span class="tag ok">'+ap.sta+' conectados</span>':'<span class="tag">apagado</span>'}</td></tr>`+
`<tr><th>Nombre</th><td class=mono colspan=3>${esc(S.net.host)}.local</td></tr>`;
$('#fwNow').textContent='v'+S.fw;$('#sysSub').textContent=S.product+' v'+S.fw+' · ID '+S.chip+' · mínimo de memoria '+Math.round(S.minHeap/1024)+' kB';
$('#otaTxt').textContent=S.ota.state||'';}
async function resetStats(){await api('/api/stats/reset',{method:'POST'});prevReq=null;toast('Contadores reiniciados');refresh()}
async function save(){const c=collect();const p1=$('#newPass').value,p2=$('#newPass2').value;if(p1!==p2){toast('Las claves no coinciden',1);show('sys');return}
const ipRe=/^((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)$/;
if(!c.net.eth.dhcp){for(const k of ['ip','mask','gw'])if(!ipRe.test(c.net.eth[k])){toast('LAN: revise '+({ip:'la dirección IP',mask:'la máscara',gw:'la puerta de enlace'})[k],1);show('net');return}
for(const k of ['dns1','dns2'])if(c.net.eth[k]&&!ipRe.test(c.net.eth[k])){toast('LAN: DNS inválido',1);show('net');return}}
if(c.net.ap.pass&&c.net.ap.pass.length<8){toast('La clave del AP debe tener al menos 8 caracteres',1);return}
if(!confirm('Se guardará la configuración y el equipo se reiniciará. ¿Continuar?'))return;
try{await api('/api/config',{method:'POST',body:c});waitReboot(c)}catch(e){toast('Error: '+e.message,1)}}
function waitReboot(c,msg){$('#ov').classList.add('on');if(msg)$('#ovM').innerHTML=msg;
if(c&&!c.net.eth.dhcp&&c.net.eth.ip&&c.net.eth.ip!==location.hostname&&S&&S.net.eth.ip===location.hostname){$('#ovM').innerHTML='La IP cambi&oacute;. Abra <a href="http://'+c.net.eth.ip+'/">http://'+c.net.eth.ip+'/</a>';return}
setTimeout(function poll(){fetch('/api/status',{cache:'no-store'}).then(r=>{if(r.ok)location.reload();else setTimeout(poll,1500)}).catch(()=>setTimeout(poll,1500))},4000)}
async function reboot(){if(!confirm('¿Reiniciar el equipo?'))return;await api('/api/reboot',{method:'POST'});waitReboot()}
async function factory(){if(prompt('Se borrará toda la configuración (red incluida). Escriba FABRICA para confirmar')!=='FABRICA')return;await api('/api/factory',{method:'POST'});waitReboot(null,'Valores de f&aacute;brica. Si la IP cambi&oacute;, con&eacute;ctese al WiFi del equipo (AP) y abra http://192.168.4.1')}
$('#tFc').onchange=()=>{const f=+$('#tFc').value,w=f==5||f==6||f==16;$('#tQtyL').classList.toggle('hide',w);$('#tValL').classList.toggle('hide',!w)};
async function runTest(b){const f=+$('#tFc').value,q={uid:+$('#tUid').value,fc:f,addr:+$('#tAddr').value,qty:+$('#tQty').value};
const vs=$('#tVal').value.split(/[,; ]+/).filter(x=>x!=='').map(Number);if(f==5||f==6)q.value=vs[0]||0;if(f==16)q.values=vs;
b.disabled=true;$('#tRes').textContent='...';try{const j=await fetch('/api/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(q)}).then(r=>r.json());
$('#tRes').innerHTML=(j.ok?'<span class="tag ok">'+j.result+'</span>':'<span class="tag bad">'+(j.exception?('Excepci&oacute;n '+j.exception+': '+(EXC[j.exception]||'')):(j.result||j.error))+'</span>')+' '+j.lat+' ms';
let h='';if(j.values){h='<table><thead><tr><th>Direcci&oacute;n</th><th class=num>Decimal</th><th class=num>Con signo</th><th class=num>Hex</th><th class=num>Binario</th></tr></thead><tbody>'+j.values.map((v,i)=>`<tr><td>${q.addr+i}</td><td class=num>${v}</td><td class=num>${f<=2?v:(v>32767?v-65536:v)}</td><td class="num mono">${f<=2?'':'0x'+v.toString(16).toUpperCase().padStart(4,'0')}</td><td class="num mono">${f<=2?'':v.toString(2).padStart(16,'0').replace(/(.{4})/g,'$1 ')}</td></tr>`).join('')+'</tbody></table>'}
if(j.raw)h+='<p class=mono style="color:var(--mute)">Respuesta: '+j.raw+'</p>';$('#tOut').innerHTML=h}catch(e){$('#tRes').textContent=e.message}b.disabled=false}
async function scan(){if(scanT){await api('/api/scan',{method:'POST',body:{stop:true}});return}
try{await api('/api/scan',{method:'POST',body:{from:+$('#sFrom').value,to:+$('#sTo').value,timeout:+$('#sTo2').value}})}catch(e){toast(e.message,1);return}
$('#sBtn').textContent='Detener';scanT=setInterval(async()=>{const j=await api('/api/scan');const tot=j.to-j.from+1,d=j.cur-j.from+(j.active?0:1);
$('#sBar').style.width=(100*d/tot)+'%';$('#sTxt').textContent=j.active?('Unit ID '+j.cur+' …'):('Terminado: '+j.found.length+' esclavo(s)');
$('#sFound').innerHTML=j.found.map(u=>`<span class=chip onclick="$('#tUid').value=${u};window.scrollTo(0,0)">ID ${u}</span>`).join('');
if(!j.active){clearInterval(scanT);scanT=null;$('#sBtn').textContent='Escanear'}},600)}
async function monToggle(){monOn=!monOn;$('#mBtn').textContent=monOn?'Pausar':'Iniciar';await api('/api/monitor',{method:'POST',body:{en:monOn}}).catch(()=>{});if(monOn){if(!monRows.length)$('#mBody').innerHTML='';monPoll()}}
async function monClear(){monRows=[];$('#mBody').innerHTML='<tr><td colspan=5 class=empty>Sin tráfico.</td></tr>';await api('/api/monitor',{method:'POST',body:{clear:true}}).catch(()=>{})}
function decode(r){const b=r.h.split(' ').map(x=>parseInt(x,16));if(r.k==='EVT')return r.x||'';let o=r.k==='TCP>'||r.k==='>TCP'?(C&&C.mb.proto==0?6:0):0;const u=b[o],f=b[o+1];if(f==null)return r.x||'';
let s='UID '+u+' · '+(f&128?'<span class="tag bad">EXC '+(EXC[b[o+2]]||b[o+2])+'</span>':(FC[f]||'FC '+f));if(r.x)s+=' · '+r.x;return s}
async function monPoll(){if(!monOn)return;try{const j=await api('/api/monitor?since='+monSince);monSince=j.last;
if(j.items.length){const t0=Date.now()-(S?S.uptime*1000:0);const html=j.items.map(r=>{monRows.push(r);const d=new Date(t0+r.t);
return `<tr><td class=mono>${d.toLocaleTimeString('es',{hour12:false})}.${String(r.t%1000).padStart(3,'0')}</td><td class=dir style="color:${r.k[0]=='>'?'var(--acc)':(r.k=='EVT'?'var(--bad)':'var(--ok)')}">${r.k}</td><td class=num>${r.n||''}</td><td class="mono h">${r.h}</td><td>${decode(r)}</td></tr>`}).join('');
const tb=$('#mBody');tb.insertAdjacentHTML('afterbegin',html.split('</tr>').filter(x=>x).reverse().join('</tr>')+'</tr>');while(tb.rows.length>500)tb.deleteRow(-1)}}catch(e){}setTimeout(monPoll,700)}
function monCsv(){const t='seq;ms;dir;bytes;hex;nota\n'+monRows.map(r=>[r.s,r.t,r.k,r.n,r.h,r.x||''].join(';')).join('\n');const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([t],{type:'text/csv'}));a.download='monitor-modbus.csv';a.click()}
function fwUpload(b){const f=$('#fwFile').files[0];if(!f){toast('Seleccione un archivo .bin',1);return}if(!confirm('Instalar '+f.name+' ('+Math.round(f.size/1024)+' kB)?'))return;
const x=new XMLHttpRequest(),fd=new FormData();fd.append('firmware',f,f.name);b.disabled=true;
x.upload.onprogress=e=>{if(e.lengthComputable)$('#fwBar').style.width=(100*e.loaded/e.total)+'%'};
x.onload=()=>{b.disabled=false;let j={};try{j=JSON.parse(x.responseText)}catch(e){}if(x.status==200&&j.ok)waitReboot(null,'Firmware instalado. Reiniciando&hellip;');else toast('Error: '+(j.error||x.status),1)};
x.onerror=()=>{b.disabled=false;toast('Error de red',1)};x.open('POST','/api/fw');x.send(fd)}
async function ota(apply){if(apply&&!confirm('Descargar e instalar la última versión publicada en GitHub? El equipo necesita salida a internet.'))return;
try{await api('/api/ota',{method:'POST',body:{apply}});toast(apply?'Instalando si hay una versión nueva...':'Consultando GitHub...');
let k=0;const t=setInterval(async()=>{await refresh();if(++k>60||/al dia|error|disponible/.test(S.ota.state))clearInterval(t);if(/reiniciando/.test(S.ota.state)){clearInterval(t);waitReboot(null,'Instalando actualizaci&oacute;n&hellip;')}},1500)}catch(e){toast(e.message,1)}}
async function restore(){const f=$('#rsFile').files[0];if(!f){toast('Seleccione el archivo de respaldo',1);return}if(!confirm('Reemplazar TODA la configuración con '+f.name+'?'))return;
try{await api('/api/restore',{method:'POST',body:await f.text()});waitReboot()}catch(e){toast('Error: '+e.message,1)}}
(async()=>{try{C=await api('/api/config');fill()}catch(e){toast('No se pudo leer la configuración',1)}
const h=location.hash.slice(1);if(h&&$('#p-'+h))show(h);refresh();setInterval(()=>{if(page==='dash'||page==='sys'||page==='ser')refresh()},1000)})();
</script></body></html>)UI";
