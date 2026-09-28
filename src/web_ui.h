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
--brand:#14313C;--brand2:#0D2830;--acc:#F77535;--acc2:#FFA167;--ok:#378676;--okb:#DDF3EC;--bad:#E5484D;--badb:#FDE4E4;--warn:#B8621B;--warnb:#FFF0E3;--mint:#6FD6C0;--idle:#8AA29C}
@media (prefers-color-scheme:dark){:root{--bg:#0A1E24;--panel:#0F2A32;--panel2:#14343D;--ink:#E7EFED;--mute:#8AA29C;--line:#1F434D;
--okb:#143A33;--badb:#3A1A1D;--warnb:#3A2716}}
*{box-sizing:border-box}html,body{margin:0}
body{font:14px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--ink)}
header{background:var(--brand);color:#fff;display:flex;align-items:center;gap:12px;padding:10px 18px;position:sticky;top:0;z-index:5}
.logo{font-weight:800;font-size:20px;letter-spacing:.3px}.logo b{color:var(--acc)}
.prod{font-size:13px;opacity:.85;border-left:1px solid #ffffff40;padding-left:14px}
.hsp{flex:1}.pill{font-size:12px;padding:3px 10px;border-radius:99px;background:#ffffff1a;white-space:nowrap;font-weight:600}
.s-OK{background:var(--ok)!important;color:#fff}.s-FALLAS{background:var(--acc)!important;color:#fff}.s-DESCONECTADO,.s-LAN{background:var(--bad)!important;color:#fff}.s-IDLE{background:#ffffff26}
.wrap{display:flex;min-height:calc(100vh - 50px)}
nav{width:220px;flex:none;background:var(--panel);border-right:1px solid var(--line);padding:12px 8px}
nav a{display:flex;gap:10px;align-items:center;padding:9px 12px;border-radius:8px;color:var(--ink);text-decoration:none;margin-bottom:2px;cursor:pointer}
nav a:hover{background:var(--panel2)}nav a.on{background:var(--brand);color:#fff}
nav a.on svg{stroke:var(--acc2)}nav svg{width:18px;height:18px;stroke:currentColor;fill:none;stroke-width:1.8;flex:none}
nav hr{border:0;border-top:1px solid var(--line);margin:8px 4px}
main{flex:1;min-width:0;padding:20px 24px;max-width:1150px}
h1{font-size:20px;margin:0 0 4px}.sub{color:var(--mute);margin:0 0 18px}
h2{font-size:15px;margin:0 0 12px}h3{font-size:13px;margin:16px 0 8px;color:var(--mute);text-transform:uppercase;letter-spacing:.4px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:16px 18px;margin-bottom:16px}
.grid{display:grid;gap:14px;grid-template-columns:repeat(auto-fill,minmax(165px,1fr))}
.kpi{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px 14px}
.kpi .l{font-size:12px;color:var(--mute)}.kpi .v{font-size:22px;font-weight:700;margin-top:2px;font-variant-numeric:tabular-nums}
.kpi .s{font-size:12px;color:var(--mute)}
.banner{display:flex;gap:16px;align-items:center;flex-wrap:wrap;border-radius:12px;padding:16px 20px;margin-bottom:16px;color:#fff;background:var(--idle)}
.banner .bs{font-size:26px;font-weight:800;letter-spacing:.5px}.banner .bt{font-size:13px;opacity:.9}
.banner.s-OK{background:var(--ok)}.banner.s-FALLAS{background:var(--acc)}.banner.s-DESCONECTADO,.banner.s-LAN{background:var(--bad)}
.bp{display:flex;gap:8px;flex-wrap:wrap;margin-left:auto}.bp span{background:#ffffff2e;border-radius:8px;padding:6px 10px;font-size:13px;font-weight:600}
.fg{display:grid;gap:12px 16px;grid-template-columns:repeat(auto-fill,minmax(200px,1fr))}
label.f{display:block}label.f>span{display:block;font-size:12.5px;font-weight:600;margin-bottom:4px}
label.f small{display:block;color:var(--mute);font-size:11.5px;margin-top:3px}
input,select{width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--ink);font:inherit}
td input,td select{padding:5px 6px;min-width:52px}
table.wide{min-width:1000px}.wide input[data-f=name]{min-width:130px}.wide input[type=number]{min-width:74px}.wide select{min-width:70px}.wide td.vals{min-width:200px;white-space:normal}
input:focus,select:focus{outline:2px solid var(--acc2);border-color:var(--acc)}
.chk{display:flex;gap:9px;align-items:flex-start;padding:6px 0}.chk input{width:auto;margin-top:3px}
.btn{display:inline-flex;gap:6px;align-items:center;border:0;border-radius:8px;padding:9px 16px;font:inherit;font-weight:600;cursor:pointer;background:var(--brand);color:#fff;text-decoration:none}
.btn.acc{background:var(--acc)}.btn.gh{background:transparent;color:var(--ink);border:1px solid var(--line)}
.btn.red{background:var(--bad)}.btn.sm{padding:4px 10px;font-size:12.5px}.btn:disabled{opacity:.5;cursor:default}
.bar{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.actions{position:sticky;bottom:0;background:linear-gradient(transparent,var(--bg) 30%);padding:18px 0 8px;display:flex;gap:10px;justify-content:flex-end}
.tw{overflow-x:auto}
table{width:100%;border-collapse:collapse;font-size:13px}th,td{text-align:left;padding:7px 8px;border-bottom:1px solid var(--line)}
th{font-size:12px;color:var(--mute);font-weight:600}td.num,th.num{text-align:right;font-variant-numeric:tabular-nums}
.mono{font-family:ui-monospace,Consolas,monospace;font-size:12.5px}
.tag{display:inline-block;font-size:11.5px;font-weight:700;padding:2px 7px;border-radius:5px;background:var(--panel2);white-space:nowrap}
.tag.ok,.tag.t-OK{background:var(--okb);color:var(--ok)}.tag.bad,.tag.t-DESCONECTADO{background:var(--badb);color:var(--bad)}.tag.w,.tag.t-FALLAS{background:var(--warnb);color:var(--warn)}
.note{padding:10px 14px;border-radius:8px;background:var(--warnb);color:var(--warn);margin-bottom:16px;font-size:13px}
.note.info{background:var(--panel2);color:var(--ink)}
.note a,small a{color:inherit;font-weight:700}
.empty{color:var(--mute);padding:14px 0;text-align:center}
.mon{max-height:480px;overflow:auto;border:1px solid var(--line);border-radius:8px}
.mon td{white-space:nowrap}.mon td.h{white-space:normal;word-break:break-all}
.dir{font-weight:700;font-size:12px}
.prog{height:8px;background:var(--panel2);border-radius:99px;overflow:hidden}.prog>i{display:block;height:100%;background:var(--acc);width:0;transition:width .3s}
.chips{display:flex;flex-wrap:wrap;gap:6px}.chip{padding:4px 10px;border-radius:99px;background:var(--okb);color:var(--ok);font-weight:700;cursor:pointer}
.toast{position:fixed;right:18px;bottom:18px;background:var(--brand);color:#fff;padding:10px 16px;border-radius:8px;opacity:0;transform:translateY(10px);transition:.2s;z-index:50;max-width:80vw;pointer-events:none}
.toast.on{opacity:1;transform:none}.toast.bad{background:var(--bad)}
.ov{position:fixed;inset:0;background:#0A1E24d0;display:none;align-items:center;justify-content:center;z-index:40}
.ov.on{display:flex}.ovb{background:var(--panel);color:var(--ink);border-radius:14px;padding:24px 26px;max-width:420px;width:90%;text-align:center}
.spin{width:36px;height:36px;border:4px solid var(--panel2);border-top-color:var(--acc);border-radius:50%;animation:sp 1s linear infinite;margin:0 auto 12px}
@keyframes sp{to{transform:rotate(360deg)}}
.wz{background:var(--panel);color:var(--ink);border-radius:14px;width:min(760px,94vw);max-height:92vh;display:flex;flex-direction:column;overflow:hidden}
.wzh{background:var(--brand);color:#fff;display:flex;gap:12px;align-items:center;padding:12px 18px}.wzh a{color:#fff;opacity:.8;cursor:pointer;font-size:13px}
.steps{display:flex;list-style:none;margin:0;padding:12px 18px;gap:6px;border-bottom:1px solid var(--line);overflow-x:auto;overflow-y:hidden;scrollbar-width:thin}
.steps li{flex:1;min-width:88px;font-size:12px;font-weight:600;color:var(--mute);border-top:3px solid var(--line);padding-top:6px}
.steps li.on{color:var(--ink);border-color:var(--acc)}.steps li.done{border-color:var(--ok)}
.wzb{padding:18px;overflow-x:hidden;overflow-y:auto;flex:1;min-height:0}.wzf{display:flex;gap:10px;justify-content:flex-end;padding:12px 18px;border-top:1px solid var(--line)}
.hide{display:none!important}
@media (max-width:760px){.wrap{display:block}nav{width:auto;display:flex;overflow-x:auto;border-right:0;border-bottom:1px solid var(--line);padding:6px}
nav a{white-space:nowrap;margin:0 2px}nav hr{display:none}main{padding:16px}.prod{display:none}#hLink{display:none}}
@media print{body *{visibility:hidden}#wSum,#wSum *{visibility:visible}#wSum{position:absolute;left:0;top:0;width:100%;color:#000}}
</style></head><body>
<header><div class="logo">ay<b>safi</b></div><div class="prod">ayModbusConverter &middot; RTU &harr; TCP</div>
<div class="hsp"></div><span class="pill" id="hName">-</span><span class="pill" id="hLink">-</span><span class="pill" id="hState">-</span></header>
<div class="wrap">
<nav id="nav">
<a data-p="dash" class="on"><svg viewBox="0 0 24 24"><rect x="3" y="3" width="7" height="9" rx="1"/><rect x="14" y="3" width="7" height="5" rx="1"/><rect x="14" y="12" width="7" height="9" rx="1"/><rect x="3" y="16" width="7" height="5" rx="1"/></svg>Panel</a>
<a data-p="net"><svg viewBox="0 0 24 24"><rect x="9" y="2" width="6" height="6" rx="1"/><rect x="2" y="16" width="6" height="6" rx="1"/><rect x="16" y="16" width="6" height="6" rx="1"/><path d="M12 8v4M5 16v-4h14v4"/></svg>Red</a>
<a data-p="ser"><svg viewBox="0 0 24 24"><path d="M4 7h16M4 12h16M4 17h10"/><circle cx="19" cy="17" r="2"/></svg>Puertos serie</a>
<a data-p="mb"><svg viewBox="0 0 24 24"><path d="M4 7h11l-3-3M20 17H9l3 3"/></svg>Modbus TCP</a>
<a data-p="poll"><svg viewBox="0 0 24 24"><path d="M4 6c0-1.7 3.6-3 8-3s8 1.3 8 3-3.6 3-8 3-8-1.3-8-3zM4 6v6c0 1.7 3.6 3 8 3s8-1.3 8-3V6M4 12v6c0 1.7 3.6 3 8 3s8-1.3 8-3v-6"/></svg>Sondeo y cach&eacute;</a>
<a data-p="iot"><svg viewBox="0 0 24 24"><path d="M5 12a7 7 0 0 1 14 0M8.5 12a3.5 3.5 0 0 1 7 0"/><circle cx="12" cy="16" r="2"/></svg>MQTT y API</a>
<a data-p="diag"><svg viewBox="0 0 24 24"><path d="M3 12h4l3-8 4 16 3-8h4"/></svg>Diagn&oacute;stico</a>
<a data-p="sys"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M4.2 4.2l2.1 2.1M17.7 17.7l2.1 2.1M2 12h3M19 12h3M4.2 19.8l2.1-2.1M17.7 6.3l2.1-2.1"/></svg>Sistema</a>
<hr><a onclick="wizOpen()"><svg viewBox="0 0 24 24"><path d="M5 19 19 5M15 5h4v4M5 9V5h4M9 19H5v-4"/></svg>Asistente</a>
</nav>
<main>
<div class="note hide" id="defPass">La clave de administrador es la de f&aacute;brica. C&aacute;mbiela en <b>Sistema &rarr; Acceso</b> o con el <a href="#" onclick="wizOpen();return false">asistente</a>.</div>

<!-- ============ PANEL ============ -->
<section id="p-dash">
<div class="banner" id="banner"><div><div class="bs" id="bState">-</div><div class="bt" id="bText">-</div></div><div class="bp" id="bPorts"></div></div>
<div class="grid">
<div class="kpi"><div class="l">LAN</div><div class="v" id="kEth">-</div><div class="s" id="kEthS">-</div></div>
<div class="kpi"><div class="l">Servidor Modbus</div><div class="v" id="kSrv">-</div><div class="s" id="kSrvS">-</div></div>
<div class="kpi"><div class="l">Clientes TCP</div><div class="v" id="kCli">-</div><div class="s" id="kCliS">-</div></div>
<div class="kpi"><div class="l">Peticiones / s</div><div class="v" id="kRate">-</div><div class="s" id="kReq">-</div></div>
<div class="kpi"><div class="l">Tasa de &eacute;xito</div><div class="v" id="kOk">-</div><div class="s" id="kOkS">-</div></div>
<div class="kpi"><div class="l">Latencia RTU</div><div class="v" id="kLat">-</div><div class="s" id="kLatS">-</div></div>
<div class="kpi"><div class="l">Timeouts / CRC</div><div class="v" id="kErr">-</div><div class="s" id="kErrS">-</div></div>
<div class="kpi"><div class="l">Desde cach&eacute;</div><div class="v" id="kCache">-</div><div class="s" id="kCacheS">-</div></div>
<div class="kpi"><div class="l">Tiempo encendido</div><div class="v" id="kUp">-</div><div class="s" id="kHeap">-</div></div>
</div>
<div class="card" style="margin-top:16px"><div class="bar" style="justify-content:space-between;margin-bottom:8px"><h2 style="margin:0">Esclavos</h2>
<button class="btn gh sm" onclick="resetStats()">Reiniciar contadores</button></div>
<div class="tw"><table><thead><tr><th>Puerto</th><th>Unit ID</th><th>Equipo</th><th>Estado</th><th class="num">Peticiones</th><th class="num">OK</th><th class="num">Excep.</th><th class="num">Timeouts</th><th class="num">Errores</th><th class="num">R&aacute;pidas</th><th class="num">Latencia</th></tr></thead>
<tbody id="tSlaves"></tbody></table></div>
<small style="color:var(--mute)">R&aacute;pidas: peticiones a un esclavo ca&iacute;do respondidas al instante con 0x0B, sin ocupar el bus.</small></div>
<div class="card"><h2>Clientes TCP conectados</h2>
<div class="tw"><table><thead><tr><th>Direcci&oacute;n</th><th class="num">Conectado</th><th class="num">Inactivo</th><th class="num">Peticiones</th></tr></thead><tbody id="tClients"></tbody></table></div></div>
<div class="card"><h2>Interfaces</h2><div class="tw"><table><tbody id="tIf"></tbody></table></div></div>
</section>

<!-- ============ RED ============ -->
<section id="p-net" class="hide">
<h1>Red</h1><p class="sub">La LAN (Ethernet) es la conexi&oacute;n de servicio: por ella se atiende Modbus TCP, esta web, MQTT y la API. El WiFi solo levanta el portal cautivo de configuraci&oacute;n.</p>
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
<label class="f"><span>Direcci&oacute;n IP</span><input data-k="net.eth.ip" placeholder="192.168.1.50"></label>
<label class="f"><span>M&aacute;scara de red</span><input data-k="net.eth.mask" placeholder="255.255.255.0"></label>
<label class="f"><span>Puerta de enlace (gateway)</span><input data-k="net.eth.gw" placeholder="192.168.1.1"></label>
<label class="f"><span>DNS primario</span><input data-k="net.eth.dns1" placeholder="(= puerta de enlace)"></label>
<label class="f"><span>DNS secundario</span><input data-k="net.eth.dns2" placeholder="8.8.8.8"></label>
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

<!-- ============ PUERTOS ============ -->
<section id="p-ser" class="hide">
<h1>Puertos serie</h1><p class="sub">Cada puerto atiende un rango de Unit ID. Deben coincidir con la configuraci&oacute;n de los esclavos conectados; si no la conoce, use la <b>detecci&oacute;n autom&aacute;tica</b>.</p>
<div id="portForms"></div>
<div class="actions"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>
</section>

<!-- ============ MODBUS ============ -->
<section id="p-mb" class="hide">
<h1>Modbus TCP</h1><p class="sub">Servidor transparente: cada petici&oacute;n se enruta por Unit ID a un puerto serie y la respuesta del esclavo vuelve sin modificar.</p>
<div class="card"><h2>Servidor</h2><div class="fg">
<label class="f"><span>Protocolo</span><select data-k="mb.proto" data-t="n"><option value="0">Modbus TCP (MBAP)</option><option value="1">Modbus RTU sobre TCP</option></select><small>RTU sobre TCP: trama RTU con CRC encapsulada.</small></label>
<label class="f"><span>Puerto TCP</span><input type="number" min="1" max="65535" data-k="mb.port"></label>
<label class="f"><span>Conexiones simult&aacute;neas</span><input type="number" min="1" max="8" data-k="mb.maxCli"></label>
<label class="f"><span>Cierre por inactividad (s)</span><input type="number" min="0" max="65535" data-k="mb.idle"><small>0 = nunca.</small></label>
<label class="f"><span>TCP alive check (s)</span><input type="number" min="0" max="7200" data-k="mb.alive"><small>Keep-alive: libera conexiones de clientes ca&iacute;dos. 0 = off.</small></label>
<label class="f"><span>Retardo inicial (ms)</span><input type="number" min="0" max="60000" data-k="mb.initDelay"><small>Espera tras el arranque antes de atender (esclavos lentos en encender).</small></label>
</div>
<label class="chk"><input type="checkbox" data-k="mb.exc"><span>Responder excepciones de pasarela (0x0B &laquo;el esclavo no responde&raquo;, 0x0A &laquo;ruta no disponible&raquo;, 0x01 &laquo;solo lectura&raquo;). Si se desactiva, el cliente ver&aacute; su propio timeout.</span></label>
</div>
<div class="card"><h2>Unit ID y enrutamiento</h2><div class="fg">
<label class="f"><span>Tratamiento</span><select data-k="mb.uidMode" data-t="n"><option value="0">Por rangos de cada puerto (transparente)</option><option value="1">Esclavo fijo (puerto 1)</option></select></label>
<label class="f"><span>Esclavo fijo / destino de Unit ID 255</span><input type="number" min="1" max="247" data-k="mb.fixUid"></label>
</div>
<label class="chk"><input type="checkbox" data-k="mb.uid0Bc"><span>Unit ID 0 = broadcast a todos los puertos (sin respuesta). Si se desactiva, el Unit ID 0 va al esclavo fijo.</span></label>
<h3>Rutas vigentes</h3><div class="tw"><table><thead><tr><th>Unit ID TCP</th><th>Puerto</th><th>Esclavo RTU</th></tr></thead><tbody id="tRoutes"></tbody></table></div>
<small style="color:var(--mute)">Los rangos y el offset se configuran en <a href="#ser" onclick="show('ser');return false">Puertos serie</a>. Ejemplo: dos equipos con ID 1 en puertos distintos &rarr; P1 atiende 1-100, P2 atiende 101-147 con offset -100 (TCP 101 = esclavo 1 del P2).</small>
</div>
<div class="card"><h2>Control de acceso</h2>
<label class="chk"><input type="checkbox" data-k="mb.allowEn"><span>Aceptar solo clientes de las redes listadas</span></label>
<div class="tw" data-if="c.mb.allowEn"><table><thead><tr><th>#</th><th>IP / red</th><th>M&aacute;scara</th></tr></thead><tbody>
<tr><td>1</td><td><input data-k="mb.allow.0.ip" placeholder="192.168.1.0"></td><td><input data-k="mb.allow.0.mask" placeholder="255.255.255.0"></td></tr>
<tr><td>2</td><td><input data-k="mb.allow.1.ip"></td><td><input data-k="mb.allow.1.mask"></td></tr>
<tr><td>3</td><td><input data-k="mb.allow.2.ip"></td><td><input data-k="mb.allow.2.mask"></td></tr>
<tr><td>4</td><td><input data-k="mb.allow.3.ip"></td><td><input data-k="mb.allow.3.mask"></td></tr>
</tbody></table></div></div>
<div class="actions"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>
</section>

<!-- ============ SONDEO ============ -->
<section id="p-poll" class="hide">
<h1>Sondeo y cach&eacute;</h1><p class="sub">El conversor puede leer los esclavos por su cuenta. Con la cach&eacute; activa en el puerto, las lecturas TCP cubiertas por esta tabla se responden al instante desde memoria (varios SCADA sin saturar un bus lento). Las escrituras siempre van directo al esclavo. La tabla tambi&eacute;n alimenta MQTT y <span class="mono">/api/tags</span>.</p>
<div class="card"><div class="bar" style="justify-content:space-between;margin-bottom:8px"><h2 style="margin:0">Tabla de sondeo</h2><button class="btn gh sm" onclick="pollAdd()">+ Agregar lectura</button></div>
<div class="tw"><table class="wide"><thead><tr><th>Act.</th><th>Nombre</th><th>Puerto</th><th>Esclavo</th><th>Funci&oacute;n</th><th>Direcci&oacute;n</th><th>Cant.</th><th>Periodo ms</th><th>MQTT</th><th>Valores</th><th></th></tr></thead><tbody id="tPolls"></tbody></table></div>
<small style="color:var(--mute)">Esclavo = direcci&oacute;n RTU real del bus (no el Unit ID TCP). Active la cach&eacute; por puerto en <a href="#ser" onclick="show('ser');return false">Puertos serie</a>.</small></div>

<div class="card"><div class="bar" style="justify-content:space-between;margin-bottom:8px"><h2 style="margin:0">Aprendizaje del polling</h2>
<div class="bar"><button class="btn sm" id="lBtn" onclick="learnToggle()">Aprender</button><button class="btn gh sm" onclick="learnClear()">Limpiar</button><button class="btn gh sm" onclick="learnAddAll()">Agregar todo a la tabla</button></div></div>
<p style="margin-top:0;color:var(--mute)">Mientras aprende, el conversor anota qu&eacute; lecturas hace su SCADA y cada cu&aacute;nto. Luego puede pasarlas a la tabla de sondeo con un clic.</p>
<div class="tw"><table><thead><tr><th>Puerto</th><th>Esclavo</th><th>Funci&oacute;n</th><th>Direcci&oacute;n</th><th>Cant.</th><th class="num">Veces</th><th class="num">Periodo</th><th></th></tr></thead><tbody id="tLearn"></tbody></table></div></div>

<div class="card"><div class="bar" style="justify-content:space-between;margin-bottom:8px"><h2 style="margin:0">Solo lectura</h2><button class="btn gh sm" onclick="roAdd()">+ Agregar regla</button></div>
<p style="margin-top:0;color:var(--mute)">Bloquea escrituras (FC 5, 6, 15, 16) a esos esclavos y direcciones: el cliente recibe la excepci&oacute;n 0x01. Protege variadores y equipos cr&iacute;ticos de una escritura accidental desde el SCADA.</p>
<div class="tw"><table class="wide" style="min-width:640px"><thead><tr><th>Act.</th><th>Puerto</th><th>Esclavo desde</th><th>hasta</th><th>Direcci&oacute;n desde</th><th>hasta</th><th></th></tr></thead><tbody id="tRo"></tbody></table></div></div>
<div class="actions"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>
</section>

<!-- ============ MQTT / API ============ -->
<section id="p-iot" class="hide">
<h1>MQTT y API</h1><p class="sub">Integraci&oacute;n con la nube Aysafi y con sistemas de TI, sobre la LAN.</p>
<div class="card"><div class="bar" style="justify-content:space-between;margin-bottom:8px"><h2 style="margin:0">MQTT</h2><span class="tag" id="mqState">-</span></div>
<label class="chk"><input type="checkbox" data-k="mqtt.en"><span>Publicar por MQTT</span></label>
<div data-if="c.mqtt.en"><div class="fg">
<label class="f"><span>Broker</span><input data-k="mqtt.host" maxlength="63"></label>
<label class="f"><span>Puerto</span><input type="number" min="1" max="65535" data-k="mqtt.port"><small>8883 con TLS, 1883 sin TLS.</small></label>
<label class="chk"><input type="checkbox" data-k="mqtt.tls"><span>TLS</span></label>
<label class="f"><span>Usuario</span><input data-k="mqtt.user" maxlength="31"></label>
<label class="f"><span>Clave</span><input type="password" data-k="mqtt.pass" maxlength="63" placeholder="(sin cambios)"></label>
<label class="f"><span>Ra&iacute;z de t&oacute;picos</span><input data-k="mqtt.base" maxlength="63" id="mqBase"><small>Vac&iacute;o = <span class="mono" id="mqBaseEff"></span></small></label>
<label class="f"><span>Estado cada (s)</span><input type="number" min="5" max="3600" data-k="mqtt.statusS"><small>Adem&aacute;s se publica al instante cuando cambia un estado.</small></label>
<label class="chk"><input type="checkbox" data-k="mqtt.retain"><span>Retener los datos</span></label>
<label class="chk"><input type="checkbox" data-k="mqtt.cmd"><span>Aceptar lecturas y escrituras por &lt;ra&iacute;z&gt;/cmd</span></label>
</div>
<h3>T&oacute;picos</h3><div class="tw"><table class="mono"><tbody id="tTopics"></tbody></table></div></div></div>
<div class="card"><h2>API REST</h2>
<p style="margin-top:0;color:var(--mute)">Lectura y escritura por HTTP con respuesta JSON. Autenticaci&oacute;n con el usuario administrador (HTTP Basic) o con una clave de API en la cabecera <span class="mono">X-API-Key</span>.</p>
<div class="bar"><span>Clave de API: <b id="apiState">-</b></span><button class="btn gh sm" onclick="apiGen()">Generar nueva</button><button class="btn gh sm" onclick="apiClear()">Quitar</button></div>
<p class="mono hide" id="apiNew" style="background:var(--panel2);padding:8px;border-radius:8px;word-break:break-all"></p>
<h3>Ejemplos</h3><div class="tw"><table class="mono"><tbody id="tApi"></tbody></table></div></div>
<div class="actions"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>
</section>

<!-- ============ DIAGNOSTICO ============ -->
<section id="p-diag" class="hide">
<h1>Diagn&oacute;stico</h1><p class="sub">Herramientas de puesta en marcha. Comparten el bus con el tr&aacute;fico de los clientes.</p>
<div class="card"><div class="fg"><label class="f"><span>Puerto para las herramientas</span><select id="dPort"></select></label></div></div>

<div class="card" id="cDetect"><h2>Detecci&oacute;n autom&aacute;tica de la configuraci&oacute;n serie</h2>
<p style="margin-top:0;color:var(--mute)">Busca baudios y paridad (1200 a 115200, N/E/O). <b>Activa:</b> consulta los Unit ID indicados en cada combinaci&oacute;n (30 a 60 s). <b>Pasiva:</b> solo escucha el tr&aacute;fico de otro maestro ya presente en el bus, sin transmitir.</p>
<div class="fg"><label class="f"><span>M&eacute;todo</span><select id="detMode"><option value="0">Activa (consultar equipos)</option><option value="1">Pasiva (escuchar el bus)</option></select></label>
<label class="f" id="detIdsL"><span>Unit ID a probar</span><input id="detIds" value="1-10"><small>Si conoce el ID del equipo, ind&iacute;quelo: es mucho m&aacute;s r&aacute;pido.</small></label></div>
<div class="bar" style="margin-top:12px"><button class="btn" id="detBtn" onclick="detect()">Detectar</button><button class="btn acc hide" id="detApply" onclick="detApply()">Aplicar al puerto</button><span id="detTxt" class="mono"></span></div>
<div class="prog" style="margin-top:12px"><i id="detBar"></i></div></div>

<div class="card"><h2>Lectura / escritura directa</h2><div class="fg">
<label class="f"><span>Esclavo (Unit ID RTU)</span><input type="number" min="0" max="247" id="tUid" value="1"></label>
<label class="f"><span>Funci&oacute;n</span><select id="tFc"><option value="3">03 Leer holding registers</option><option value="4">04 Leer input registers</option><option value="1">01 Leer coils</option><option value="2">02 Leer entradas discretas</option><option value="6">06 Escribir un registro</option><option value="16">16 Escribir registros</option><option value="5">05 Escribir un coil</option><option value="15">15 Escribir coils</option></select></label>
<label class="f"><span>Direcci&oacute;n (base 0)</span><input type="number" min="0" max="65535" id="tAddr" value="0"></label>
<label class="f" id="tQtyL"><span>Cantidad</span><input type="number" min="1" max="125" id="tQty" value="10"></label>
<label class="f hide" id="tValL"><span>Valor(es)</span><input id="tVal" placeholder="123  /  1,2,3"></label>
</div><div class="bar" style="margin-top:12px"><button class="btn" onclick="runTest(this)">Ejecutar</button><span id="tRes" class="mono"></span></div>
<div id="tOut" style="margin-top:12px"></div></div>

<div class="card"><h2>Escanear el bus</h2><div class="fg">
<label class="f"><span>Desde</span><input type="number" min="1" max="247" id="sFrom" value="1"></label>
<label class="f"><span>Hasta</span><input type="number" min="1" max="247" id="sTo" value="247"></label>
<label class="f"><span>Timeout por equipo (ms)</span><input type="number" min="20" max="2000" id="sTmo" value="100"></label>
</div><div class="bar" style="margin-top:12px"><button class="btn" id="sBtn" onclick="scan()">Escanear</button><span id="sTxt" class="mono"></span></div>
<div class="prog" style="margin:12px 0"><i id="sBar"></i></div><div class="chips" id="sFound"></div>
<small style="color:var(--mute)">Consulta FC03 registro 0 en cada Unit ID; cualquier respuesta (incluida una excepci&oacute;n) indica un esclavo. Toque uno para explorarlo.</small></div>

<div class="card"><h2>Explorar un equipo</h2>
<p style="margin-top:0;color:var(--mute)">Lee la identificaci&oacute;n del fabricante (FC43, si el equipo la soporta) y prueba qu&eacute; rangos de registros existen, por bloques.</p>
<div class="fg"><label class="f"><span>Esclavo</span><input type="number" min="1" max="247" id="xUid" value="1"></label>
<label class="f"><span>Tama&ntilde;o de bloque</span><input type="number" min="1" max="10000" id="xStep" value="100"></label>
<label class="f"><span>Hasta la direcci&oacute;n</span><input type="number" min="1" max="65535" id="xMax" value="10000"></label></div>
<div class="bar" style="margin-top:12px"><button class="btn" id="xBtn" onclick="discover()">Explorar</button><span id="xTxt" class="mono"></span></div>
<div class="prog" style="margin:12px 0"><i id="xBar"></i></div><div id="xOut"></div></div>

<div class="card"><div class="bar" style="justify-content:space-between;margin-bottom:10px"><h2 style="margin:0">Monitor de tr&aacute;fico</h2>
<div class="bar"><button class="btn" id="mBtn" onclick="monToggle()">Iniciar</button><button class="btn gh" onclick="monClear()">Limpiar</button><button class="btn gh" onclick="monCsv()">CSV</button><a class="btn gh" href="/api/pcap" download>Wireshark (.pcapng)</a></div></div>
<div class="mon"><table><thead><tr><th>Hora</th><th>Dir.</th><th>Pto</th><th class="num">Bytes</th><th>Trama (hex)</th><th>Detalle</th></tr></thead><tbody id="mBody"><tr><td colspan="6" class="empty">Monitor detenido.</td></tr></tbody></table></div>
<small style="color:var(--mute)">Wireshark: Preferencias &rarr; Protocols &rarr; DLT_USER &rarr; agregue DLT 147 = <span class="mono">mbrtu</span> y DLT 148 = <span class="mono">mbtcp</span>.</small></div>
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
<label class="f"><span>Apagar tras (min)</span><input type="number" min="0" max="240" data-k="sys.oledSleep"><small>0 = siempre encendida. Se enciende sola si cambia un estado.</small></label>
<label class="f"><span>Brillo (0-255)</span><input type="number" min="0" max="255" data-k="sys.oledContrast"></label>
</div></div>
<div class="actions" style="position:static"><button class="btn acc" onclick="save()">Guardar y reiniciar</button></div>
<div class="card"><h2>Firmware</h2>
<p style="margin-top:0">Versi&oacute;n instalada: <b id="fwNow">-</b></p>
<div class="bar"><input type="file" id="fwFile" accept=".bin" style="max-width:320px"><button class="btn" onclick="fwUpload(this)">Subir firmware</button></div>
<div class="prog" style="margin:12px 0"><i id="fwBar"></i></div>
<div class="bar"><label class="chk"><input type="checkbox" data-k="sys.otaAuto"><span>Buscar actualizaciones en GitHub al arrancar</span></label></div>
<div class="bar"><button class="btn gh" onclick="ota(false)">Buscar actualizaci&oacute;n</button><button class="btn gh" onclick="ota(true)">Instalar desde GitHub</button><span id="otaTxt" class="mono"></span></div>
</div>
<div class="card"><h2>Respaldo de configuraci&oacute;n</h2><div class="bar">
<a class="btn gh" href="/api/backup" download>Descargar respaldo</a>
<input type="file" id="rsFile" accept=".json" style="max-width:260px"><button class="btn gh" onclick="restore()">Restaurar</button></div>
<small style="color:var(--mute)">El respaldo incluye las claves. Gu&aacute;rdelo en un lugar seguro.</small></div>
<div class="card"><h2>Mantenimiento</h2><div class="bar">
<button class="btn gh" onclick="wizOpen()">Abrir el asistente</button><button class="btn gh" onclick="reboot()">Reiniciar</button><button class="btn red" onclick="factory()">Restaurar valores de f&aacute;brica</button></div></div>
</section>
</main></div>

<!-- ============ ASISTENTE ============ -->
<div class="ov" id="wiz"><div class="wz">
<div class="wzh"><div class="logo">ay<b>safi</b></div><div>Asistente de puesta en marcha</div><div class="hsp"></div><a onclick="wizSkip()">Omitir</a></div>
<ol class="steps" id="wSteps"><li>1. Equipo</li><li>2. Red LAN</li><li>3. Puerto serie</li><li>4. Equipos del bus</li><li>5. Listo</li></ol>
<div class="wzb">
<section data-w="0"><h2>Bienvenido</h2><p style="color:var(--mute);margin-top:0">En cinco pasos el conversor queda listo: identificaci&oacute;n, red, puerto serie con detecci&oacute;n autom&aacute;tica, b&uacute;squeda de equipos y resumen para imprimir.</p>
<div class="fg"><label class="f"><span>Nombre del equipo</span><input data-wk="sys.name" maxlength="31"><small>Ej.: Tablero bombas Sector 3</small></label>
<label class="f"><span>Nueva clave de administrador</span><input type="password" id="wPass" maxlength="31" placeholder="m&iacute;n. 6 caracteres"></label>
<label class="f"><span>Repetir clave</span><input type="password" id="wPass2" maxlength="31"></label></div>
<p class="note" id="wPassNote">La clave actual es la de f&aacute;brica (admin): c&aacute;mbiela antes de instalar el equipo.</p></section>

<section data-w="1"><h2>Red LAN</h2><p style="color:var(--mute);margin-top:0">Conexi&oacute;n de servicio para Modbus TCP. Actual: <b class="mono" id="wLanNow">-</b></p>
<div class="fg"><label class="f"><span>Modo</span><select data-wk="net.eth.dhcp" data-t="b"><option value="1">Autom&aacute;tico (DHCP)</option><option value="0">IP fija (manual)</option></select></label>
<label class="f"><span>Nombre de host</span><input data-wk="net.host" maxlength="31"></label></div>
<div class="fg" data-wif="!w.net.eth.dhcp" style="margin-top:12px">
<label class="f"><span>Direcci&oacute;n IP</span><input data-wk="net.eth.ip" placeholder="192.168.1.50"></label>
<label class="f"><span>M&aacute;scara de red</span><input data-wk="net.eth.mask" placeholder="255.255.255.0"></label>
<label class="f"><span>Puerta de enlace</span><input data-wk="net.eth.gw" placeholder="192.168.1.1"></label>
<label class="f"><span>DNS primario</span><input data-wk="net.eth.dns1" placeholder="(= puerta de enlace)"></label></div></section>

<section data-w="2"><h2>Puerto serie</h2><p style="color:var(--mute);margin-top:0">&iquest;D&oacute;nde est&aacute; conectado el equipo? Si no conoce su configuraci&oacute;n, det&eacute;ctela autom&aacute;ticamente.</p>
<div class="fg"><label class="f"><span>Puerto</span><select id="wPort"><option value="0">Puerto 1 &mdash; RS-485</option><option value="1">Puerto 2 &mdash; RS-232</option></select></label>
<label class="f"><span>Unit ID a probar</span><input id="wIds" value="1-10"></label></div>
<div class="bar" style="margin:12px 0"><button class="btn" id="wDetBtn" onclick="wizDetect()">Detectar autom&aacute;ticamente</button><span id="wDetTxt" class="mono"></span></div>
<div class="prog" style="margin-bottom:12px"><i id="wDetBar"></i></div>
<div class="fg"><label class="f"><span>Baudios</span><select id="wBaud"></select></label>
<label class="f"><span>Paridad</span><select id="wPar"><option value="N">Ninguna</option><option value="E">Par</option><option value="O">Impar</option></select></label>
<label class="f"><span>Bits de parada</span><select id="wStop"><option value="1">1</option><option value="2">2</option></select></label></div></section>

<section data-w="3"><h2>Equipos del bus</h2><p style="color:var(--mute);margin-top:0">B&uacute;squeda de esclavos en el puerto elegido.</p>
<div class="fg"><label class="f"><span>Desde</span><input type="number" id="wsFrom" value="1" min="1" max="247"></label><label class="f"><span>Hasta</span><input type="number" id="wsTo" value="20" min="1" max="247"></label></div>
<div class="bar" style="margin:12px 0"><button class="btn" id="wScanBtn" onclick="wizScan()">Buscar equipos</button><span id="wScanTxt" class="mono"></span></div>
<div class="prog" style="margin-bottom:12px"><i id="wScanBar"></i></div>
<div class="tw"><table><thead><tr><th>Unit ID</th><th>Equipo</th><th>Registros encontrados</th><th></th></tr></thead><tbody id="wDevs"><tr><td colspan="4" class="empty">Todav&iacute;a no se ha buscado.</td></tr></tbody></table></div></section>

<section data-w="4"><h2>Listo</h2><p style="color:var(--mute);margin-top:0">Revise el resumen. Al finalizar se guarda la configuraci&oacute;n y el equipo se reinicia.</p>
<div id="wSum"></div></section>
</div>
<div class="wzf"><button class="btn gh" id="wPrint" onclick="window.print()">Imprimir ficha</button><div class="hsp"></div><button class="btn gh" id="wBack" onclick="wizGo(-1)">Atr&aacute;s</button><button class="btn acc" id="wNext" onclick="wizGo(1)">Siguiente</button></div>
</div></div>

<div class="toast" id="toast"></div>
<div class="ov" id="ov"><div class="ovb"><div class="spin"></div><h2 id="ovT">Reiniciando</h2><p id="ovM" style="margin:0;color:var(--mute)">Reconectando con el equipo&hellip;</p></div></div>
<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
let C=null,S=null,page='dash',prevReq=null,prevT=0,monOn=false,monSince=0,monRows=[],toolT=null,learnData=[],tagsData={};
const BAUDS=[1200,2400,4800,9600,14400,19200,38400,57600,115200,230400];
const FC={1:'Leer coils',2:'Leer entradas',3:'Leer holding',4:'Leer input',5:'Escribir coil',6:'Escribir registro',15:'Escribir coils',16:'Escribir registros',8:'Diagn&oacute;stico',17:'ID esclavo',23:'Leer/escribir',43:'Identificaci&oacute;n'};
const EXC={1:'Funci&oacute;n ilegal / solo lectura',2:'Direcci&oacute;n ilegal',3:'Valor ilegal',4:'Falla del esclavo',5:'ACK',6:'Esclavo ocupado',10:'Ruta no disponible',11:'Esclavo no responde'};
const ST={'OK':'OK','FALLAS':'FALLAS','DESCONECTADO':'DESCONECTADO','SIN TRAFICO':'IDLE','APAGADO':'IDLE','LAN DESCONECTADA':'LAN'};
const STXT={'OK':'Transmisi&oacute;n normal en todos los puertos','FALLAS':'Hay timeouts o errores en parte de las transacciones','DESCONECTADO':'Ning&uacute;n esclavo responde en al menos un puerto','SIN TRAFICO':'Sin transacciones en los &uacute;ltimos 30 s','LAN DESCONECTADA':'Cable de red desconectado: los clientes Modbus TCP no llegan al equipo','APAGADO':'No hay puertos habilitados'};
function toast(m,bad){const t=$('#toast');t.innerHTML=m;t.className='toast on'+(bad?' bad':'');clearTimeout(t._h);t._h=setTimeout(()=>t.className='toast',3400)}
async function api(p,o={}){if(o.body&&typeof o.body!=='string'&&!(o.body instanceof FormData)){o.body=JSON.stringify(o.body);o.headers={'Content-Type':'application/json'}}
const r=await fetch(p,o);let j={};try{j=await r.json()}catch(e){}if(!r.ok||j.ok===false)throw new Error(j.error||j.result||('HTTP '+r.status));return j}
const get=(o,p)=>p.split('.').reduce((a,k)=>a==null?a:a[k],o);
function set(o,p,v){const k=p.split('.');let a=o;for(let i=0;i<k.length-1;i++){if(a[k[i]]==null)a[k[i]]={};a=a[k[i]]}a[k[k.length-1]]=v}
const esc=s=>String(s==null?'':s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function dur(s){if(s<0)return '-';const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return d?d+'d '+h+'h':h?h+'h '+m+'m':m+'m '+(s%60)+'s'}
const n=v=>Number(v||0).toLocaleString('es');
const stTag=s=>`<span class="tag t-${ST[s]||'IDLE'}">${esc(s)}</span>`;
const ipRe=/^((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)$/;
const opt=(v,t,cur)=>`<option value="${v}"${String(v)===String(cur)?' selected':''}>${t}</option>`;

// ---------- puertos (formularios generados) ----------
function portForm(i){const w=i==0?'<b>IO5</b> &larr; RO, <b>IO17</b> &rarr; DI, <b>IO33 (485_EN)</b> &rarr; DE+/RE. Termine el bus con 120&nbsp;&Omega; en ambos extremos.':'MAX3232: <b>IO35</b> &larr; R1OUT, <b>IO4</b> &rarr; T1IN. Tambi&eacute;n admite un m&oacute;dulo RS-485 <b>autodireccional</b> (sin DE).';
const k=f=>`data-k="ports.${i}.${f}"`;
return `<div class="card"><div class="bar" style="justify-content:space-between"><h2 style="margin:0">Puerto ${i+1} &middot; <span id="pn${i}"></span> <span id="ps${i}"></span></h2>
<label class="chk"><input type="checkbox" ${k('en')}><span>Habilitado</span></label></div>
<div data-if="c.ports[${i}].en"><h3>L&iacute;nea</h3><div class="fg">
<label class="f"><span>Nombre</span><input ${k('name')} maxlength="15"></label>
<label class="f"><span>Baudios</span><select ${k('baud')} data-t="n">${BAUDS.map(b=>opt(b,b)).join('')}</select></label>
<label class="f"><span>Bits de datos</span><select ${k('data')} data-t="n"><option value="8">8</option><option value="7">7</option></select></label>
<label class="f"><span>Paridad</span><select ${k('parity')}><option value="N">Ninguna</option><option value="E">Par (Even)</option><option value="O">Impar (Odd)</option></select></label>
<label class="f"><span>Bits de parada</span><select ${k('stop')} data-t="n"><option value="1">1</option><option value="2">2</option></select></label>
<label class="f"><span>&nbsp;</span><button class="btn gh" type="button" onclick="goDetect(${i})">Detectar autom&aacute;ticamente</button></label></div>
<h3>Temporizaci&oacute;n</h3><div class="fg">
<label class="f"><span>Timeout de respuesta (ms)</span><input type="number" min="20" max="10000" ${k('timeout')}></label>
<label class="f"><span>Reintentos</span><input type="number" min="0" max="5" ${k('retries')}><small>Ante timeout o CRC inv&aacute;lido.</small></label>
<label class="f"><span>Fin de trama (ms)</span><input type="number" min="0" max="500" ${k('charTo')}><small>0 = autom&aacute;tico (t3.5). Actual: <b id="gap${i}">-</b></small></label>
<label class="f"><span>Pausa entre peticiones (ms)</span><input type="number" min="0" max="5000" ${k('turn')}></label>
<label class="f"><span>Pausa tras broadcast (ms)</span><input type="number" min="0" max="5000" ${k('bcast')}></label></div>
<h3>Enrutamiento por Unit ID</h3><div class="fg">
<label class="f"><span>Unit ID TCP desde</span><input type="number" min="1" max="247" ${k('uidFrom')}></label>
<label class="f"><span>hasta</span><input type="number" min="1" max="247" ${k('uidTo')}></label>
<label class="f"><span>Offset</span><input type="number" min="-246" max="246" ${k('uidOff')}><small>Esclavo RTU = Unit ID + offset.</small></label></div>
<h3>Esclavos ca&iacute;dos</h3><div class="fg">
<label class="f"><span>Marcar ca&iacute;do tras (timeouts seguidos)</span><input type="number" min="0" max="50" ${k('offAfter')}><small>0 = desactivado. Ca&iacute;do: se responde 0x0B al instante, sin frenar al resto.</small></label>
<label class="f"><span>Reintentar cada (s)</span><input type="number" min="1" max="3600" ${k('offRetry')}></label></div>
<h3>Cach&eacute; / agente</h3><div class="fg">
<label class="chk"><input type="checkbox" ${k('cache')}><span>Responder lecturas desde la tabla de sondeo</span></label>
<label class="f"><span>Antig&uuml;edad m&aacute;xima del dato (ms)</span><input type="number" min="50" max="60000" ${k('maxAge')}><small>M&aacute;s viejo que esto: la lectura va al esclavo.</small></label></div>
<div class="note info" style="margin:14px 0 0">${w}</div></div></div>`}
function goDetect(i){show('diag');$('#dPort').value=i+1;$('#cDetect').scrollIntoView({behavior:'smooth'})}

// ---------- binder ----------
function fill(){$$('[data-k]').forEach(e=>{const v=get(C,e.dataset.k);if(e.type==='checkbox')e.checked=!!v;else if(e.dataset.t==='b')e.value=v?'1':'0';else e.value=v==null?'':v});renderTables();cond()}
function readEl(e){if(e.type==='checkbox')return e.checked;if(e.dataset.t==='b')return e.value==='1';if(e.dataset.t==='n'||e.type==='number')return Number(e.value);return e.value.trim()}
function collect(){const c=JSON.parse(JSON.stringify(C));$$('[data-k]').forEach(e=>set(c,e.dataset.k,readEl(e)));c.polls=readRows('#tPolls');c.ro=readRows('#tRo');return c}
function cond(){const c=collect();$$('[data-if]').forEach(e=>{let ok=true;try{ok=new Function('c','return '+e.dataset.if)(c)}catch(x){}e.classList.toggle('hide',!ok)});routes(c)}
document.addEventListener('change',e=>{if(!e.target.dataset.k)return;
if(e.target.dataset.k==='ports.1.en'&&e.target.checked){const t1=document.querySelector('[data-k="ports.0.uidTo"]'),f2=+document.querySelector('[data-k="ports.1.uidFrom"]').value;
if(+t1.value>=f2){t1.value=f2-1;toast('Rango del puerto 1 ajustado a 1-'+(f2-1)+' para no superponerse con el puerto 2')}}cond()});
function readRows(sel){return $$(sel+' tr[data-r]').map(tr=>{const o={};tr.querySelectorAll('[data-f]').forEach(e=>o[e.dataset.f]=readEl(e));return o})}
function routes(c){const t=$('#tRoutes');if(!t)return;let h='';
if(c.mb.uidMode==1)h=`<tr><td>todos</td><td>P1 ${esc(c.ports[0].name)}</td><td>${c.mb.fixUid}</td></tr>`;
else c.ports.forEach((p,i)=>{if(!p.en)return;const o=p.uidOff||0;h+=`<tr><td>${p.uidFrom} &ndash; ${p.uidTo}</td><td>P${i+1} ${esc(p.name)}</td><td>${o?(Math.max(1,p.uidFrom+o)+' &ndash; '+Math.min(247,p.uidTo+o)+' (offset '+(o>0?'+':'')+o+')'):'igual al Unit ID'}</td></tr>`});
if(c.mb.uid0Bc)h+='<tr><td>0</td><td>todos los puertos</td><td>broadcast</td></tr>';
t.innerHTML=h||'<tr><td colspan=3 class=empty>Sin puertos habilitados</td></tr>'}

// ---------- tablas de sondeo / solo lectura ----------
const portSel=(f,v)=>`<select data-f="${f}" data-t="n">${[0,1].map(i=>opt(i,'P'+(i+1),v)).join('')}</select>`;
function pollRow(p,i){return `<tr data-r><td><input type="checkbox" data-f="en"${p.en?' checked':''}></td><td><input data-f="name" maxlength="19" value="${esc(p.name)}" placeholder="p.ej. medidor1"></td><td>${portSel('port',p.port)}</td>
<td><input type="number" data-f="uid" min="1" max="247" value="${p.uid}"></td><td><select data-f="fc" data-t="n">${[3,4,1,2].map(f=>opt(f,'FC0'+f,p.fc)).join('')}</select></td>
<td><input type="number" data-f="addr" min="0" max="65535" value="${p.addr}"></td><td><input type="number" data-f="qty" min="1" max="2000" value="${p.qty}"></td>
<td><input type="number" data-f="period" min="100" max="60000" value="${p.period}"></td><td><input type="checkbox" data-f="mqtt"${p.mqtt?' checked':''}></td>
<td class="mono vals" id="pv${i}">-</td><td><button class="btn gh sm" onclick="rowDel(this)">&times;</button></td></tr>`}
function roRow(r){return `<tr data-r><td><input type="checkbox" data-f="en"${r.en?' checked':''}></td><td>${portSel('port',r.port)}</td>
<td><input type="number" data-f="uidFrom" min="1" max="247" value="${r.uidFrom}"></td><td><input type="number" data-f="uidTo" min="1" max="247" value="${r.uidTo}"></td>
<td><input type="number" data-f="addrFrom" min="0" max="65535" value="${r.addrFrom}"></td><td><input type="number" data-f="addrTo" min="0" max="65535" value="${r.addrTo}"></td>
<td><button class="btn gh sm" onclick="rowDel(this)">&times;</button></td></tr>`}
function renderTables(){const P=C.polls||[],R=C.ro||[];
$('#tPolls').innerHTML=P.length?P.map(pollRow).join(''):'<tr><td colspan=11 class=empty>Sin lecturas. Agregue una o use el aprendizaje.</td></tr>';
$('#tRo').innerHTML=R.length?R.map(roRow).join(''):'<tr><td colspan=7 class=empty>Sin reglas: todas las escrituras est&aacute;n permitidas.</td></tr>'}
function rowDel(b){const tr=b.closest('tr'),tb=tr.parentNode;tr.remove();if(tb.id==='tPolls')C.polls=readRows('#tPolls');else C.ro=readRows('#tRo')}
function pollAdd(p){C.polls=readRows('#tPolls');if(C.polls.length>=24){toast('M&aacute;ximo 24 lecturas',1);return}C.polls.push(Object.assign({en:true,name:'',port:0,uid:1,fc:3,addr:0,qty:10,period:1000,mqtt:false},p||{}));renderTables()}
function roAdd(){C.ro=readRows('#tRo');if(C.ro.length>=8){toast('M&aacute;ximo 8 reglas',1);return}C.ro.push({en:true,port:0,uidFrom:1,uidTo:1,addrFrom:0,addrTo:65535});renderTables()}
async function refreshTags(){try{const j=await api('/api/tags');j.tags.forEach(t=>{const e=$('#pv'+t.i);if(!e)return;
e.innerHTML=t.v?(`<span class="tag ${t.q=='ok'?'ok':'w'}">${esc(t.q)}</span> `+t.v.slice(0,10).join(', ')+(t.v.length>10?' &hellip;':'')+` <small style="color:var(--mute)">${t.age} ms</small>`):`<span class="tag ${t.q=='sin dato'?'':'bad'}">${esc(t.q)}${t.exc?' '+t.exc:''}</span>`})}catch(e){}}
async function refreshLearn(){try{const j=await api('/api/learn');learnData=j.items;$('#lBtn').textContent=j.en?'Detener aprendizaje':'Aprender';
$('#tLearn').innerHTML=j.items.length?j.items.map((l,i)=>`<tr><td>P${l.port+1}</td><td>${l.uid}</td><td>FC0${l.fc}</td><td>${l.addr}</td><td>${l.qty}</td><td class=num>${n(l.count)}</td><td class=num>${l.period?l.period+' ms':'-'}</td><td><button class="btn gh sm" onclick="learnAdd(${i})">Agregar</button></td></tr>`).join(''):`<tr><td colspan=8 class=empty>${j.en?'Aprendiendo&hellip; esperando lecturas de los clientes.':'Sin datos. Pulse Aprender mientras el SCADA est&aacute; sondeando.'}</td></tr>`}catch(e){}}
async function learnToggle(){const on=$('#lBtn').textContent==='Aprender';await api('/api/learn',{method:'POST',body:{en:on}});refreshLearn()}
async function learnClear(){await api('/api/learn',{method:'POST',body:{clear:true}});refreshLearn()}
function learnAdd(i){const l=learnData[i];pollAdd({port:l.port,uid:l.uid,fc:l.fc,addr:l.addr,qty:l.qty,period:Math.max(200,Math.round((l.period||1000)/100)*100),name:'u'+l.uid+'_'+l.addr});toast('Agregada a la tabla de sondeo (guarde para aplicar)')}
function learnAddAll(){learnData.forEach((l,i)=>learnAdd(i))}

// ---------- navegacion ----------
function show(p){page=p;$$('nav a[data-p]').forEach(a=>a.classList.toggle('on',a.dataset.p===p));$$('main>section').forEach(s=>s.classList.toggle('hide',s.id!=='p-'+p));location.hash=p;
if(p!=='diag'&&monOn)monToggle();if(p==='poll'){refreshTags();refreshLearn()}if(p==='iot')iotRender()}
$$('nav a[data-p]').forEach(a=>a.onclick=()=>show(a.dataset.p));

// ---------- estado ----------
async function refresh(){try{S=await api('/api/status')}catch(e){$('#hState').textContent='sin conexión';$('#hState').className='pill s-LAN';return}
const g=S.gw,st=g.stats,e=S.net.eth,sk=ST[S.state]||'IDLE';
$('#hName').textContent=S.name;$('#hLink').textContent=e.link?('LAN '+e.speed+'M'):'LAN desconectada';$('#hLink').className='pill '+(e.link?'':'s-LAN');
$('#hState').textContent=S.state;$('#hState').className='pill s-'+sk;
$('#banner').className='banner s-'+sk;$('#bState').textContent=S.state;$('#bText').innerHTML=STXT[S.state]||'';
$('#bPorts').innerHTML=g.ports.map((p,i)=>p.en?`<span>P${i+1} ${esc(p.name)}: ${esc(p.state)}</span>`:'').join('');
$('#defPass').classList.toggle('hide',!S.defPass);
$('#kEth').textContent=e.up?e.ip:(e.link?'sin IP':'sin cable');$('#kEthS').textContent=e.link?(e.speed+' Mbps '+(e.fd?'full':'half')+'-duplex'):'Conecte el cable de red';
$('#kSrv').textContent=g.listening?(C?(C.mb.proto?'RTU/TCP':'TCP')+' :'+C.mb.port:'activo'):'detenido';
$('#kSrvS').textContent=g.ports.filter(p=>p.en).map((p,i)=>p.name+' '+p.fmt).join(' · ');
$('#kCli').textContent=g.clients.length+(C?' / '+C.mb.maxCli:'');$('#kCliS').textContent=n(st.conn)+' conexiones, '+n(st.rejected)+' rechazadas';
const now=Date.now();if(prevReq!=null&&now>prevT){$('#kRate').textContent=((st.tcpReq-prevReq)*1000/(now-prevT)).toFixed(1)}prevReq=st.tcpReq;prevT=now;
$('#kReq').textContent=n(st.tcpReq)+' peticiones';
const tot=st.ok+st.timeout+st.crc+st.bad;$('#kOk').textContent=tot?(100*st.ok/tot).toFixed(1)+'%':'-';$('#kOkS').textContent=n(st.exc)+' excepciones del esclavo';
$('#kLat').textContent=st.lat+' ms';$('#kLatS').textContent=n(st.polls)+' sondeos propios';
g.ports.forEach((p,i)=>{const x=$('#gap'+i);if(x)x.textContent=(p.gapUs/1000).toFixed(1)+' ms';const pn=$('#pn'+i),ps=$('#ps'+i);if(pn)pn.textContent=p.name;if(ps)ps.innerHTML=p.en?stTag(p.state):''});
$('#kErr').textContent=n(st.timeout)+' / '+n(st.crc);$('#kErrS').textContent=n(st.gwExc)+' excep. pasarela, '+n(st.fast)+' rápidas';
$('#kCache').textContent=n(st.cache);$('#kCacheS').textContent=tot+st.cache?(100*st.cache/(tot+st.cache)).toFixed(0)+'% de las lecturas':'-';
$('#kUp').textContent=dur(S.uptime);$('#kHeap').textContent='memoria libre '+Math.round(S.heap/1024)+' kB';
$('#tSlaves').innerHTML=g.slaves.length?g.slaves.map(s=>`<tr><td>P${s.port+1}</td><td><b>${s.uid}</b></td><td>${esc(s.ident||'')}</td><td>${stTag(s.state)}</td><td class=num>${n(s.req)}</td><td class=num>${n(s.ok)}</td><td class=num>${n(s.exc)}</td><td class=num>${n(s.tout)}</td><td class=num>${n(s.err)}</td><td class=num>${n(s.fast)}</td><td class=num>${s.lat} ms</td></tr>`).join(''):'<tr><td colspan=11 class=empty>Todavía no hay tráfico hacia los puertos.</td></tr>';
$('#tClients').innerHTML=g.clients.length?g.clients.map(c=>`<tr><td class=mono>${c.ip}:${c.port}</td><td class=num>${dur(c.up)}</td><td class=num>${dur(c.idle)}</td><td class=num>${n(c.reqs)}</td></tr>`).join(''):'<tr><td colspan=4 class=empty>Sin clientes conectados.</td></tr>';
const ap=S.net.ap;
$('#tIf').innerHTML=`<tr><th>LAN</th><td class=mono>${e.up?e.ip+' / '+e.mask:'-'}</td><td class=mono>gw ${e.gw}</td><td>${e.link?'<span class="tag ok">enlace</span>':'<span class="tag bad">sin cable</span>'}</td></tr>`+
`<tr><th>Portal WiFi</th><td class=mono>${ap.up?ap.ip:'-'}</td><td>${esc(ap.ssid)}</td><td>${ap.up?'<span class="tag ok">'+ap.sta+' conectados</span>':'<span class="tag">apagado</span>'}</td></tr>`+
`<tr><th>MQTT</th><td colspan=2>${esc(S.mqtt)}</td><td></td></tr><tr><th>Nombre</th><td class=mono colspan=3>${esc(S.net.host)}.local</td></tr>`;
$('#nLink').textContent=e.link?(e.speed+' Mbps '+(e.fd?'full':'half')):'sin cable';$('#nIp').textContent=e.ip+(e.up?(e.dhcp?' (DHCP)':' (fija)'):'');
$('#nMask').textContent=e.mask;$('#nGw').textContent=e.gw;$('#nDns').textContent=e.dns;$('#nMac').textContent=e.mac;
$('#mqState').textContent=S.mqtt;$('#mqState').className='tag '+(S.mqtt=='conectado'?'ok':(S.mqtt=='desactivado'?'':'w'));
$('#fwNow').textContent='v'+S.fw;$('#sysSub').textContent=S.product+' v'+S.fw+' · ID '+S.chip+' · mínimo de memoria '+Math.round(S.minHeap/1024)+' kB';
$('#otaTxt').textContent=S.ota.state||'';$('#wLanNow').textContent=e.up?e.ip+' / '+e.mask+' gw '+e.gw:'sin enlace'}

async function resetStats(){await api('/api/stats/reset',{method:'POST'});prevReq=null;toast('Contadores reiniciados');refresh()}
function validate(c){
if(!c.net.eth.dhcp){for(const k of ['ip','mask','gw'])if(!ipRe.test(c.net.eth[k]||'')){toast('LAN: revise '+({ip:'la dirección IP',mask:'la máscara',gw:'la puerta de enlace'})[k],1);return 'net'}
for(const k of ['dns1','dns2'])if(c.net.eth[k]&&!ipRe.test(c.net.eth[k])){toast('LAN: DNS inválido',1);return 'net'}}
if(c.net.ap.pass&&c.net.ap.pass.length<8){toast('La clave del AP debe tener al menos 8 caracteres',1);return 'net'}
const en=c.ports.map((p,i)=>[p,i]).filter(x=>x[0].en);
for(const [p,i] of en)if(p.uidFrom>p.uidTo){toast('Puerto '+(i+1)+': rango de Unit ID invertido',1);return 'ser'}
if(en.length==2&&c.mb.uidMode==0){const a=en[0][0],b=en[1][0];if(a.uidFrom<=b.uidTo&&b.uidFrom<=a.uidTo){toast('Los rangos de Unit ID de los puertos se superponen',1);return 'ser'}}
return null}
async function save(){const c=collect();const p1=$('#newPass').value,p2=$('#newPass2').value;if(p1!==p2){toast('Las claves no coinciden',1);show('sys');return}
const bad=validate(c);if(bad){show(bad);return}
if(!confirm('Se guardará la configuración y el equipo se reiniciará. ¿Continuar?'))return;
try{await api('/api/config',{method:'POST',body:c});waitReboot(c)}catch(e){toast('Error: '+e.message,1)}}
function waitReboot(c,msg){$('#wiz').classList.remove('on');$('#ov').classList.add('on');if(msg)$('#ovM').innerHTML=msg;
if(c&&!c.net.eth.dhcp&&c.net.eth.ip&&c.net.eth.ip!==location.hostname&&S&&S.net.eth.ip===location.hostname){$('#ovM').innerHTML='La IP cambi&oacute;. Abra <a href="http://'+c.net.eth.ip+'/">http://'+c.net.eth.ip+'/</a>';return}
setTimeout(function poll(){fetch('/api/status',{cache:'no-store'}).then(r=>{if(r.ok)location.reload();else setTimeout(poll,1500)}).catch(()=>setTimeout(poll,1500))},4000)}
async function reboot(){if(!confirm('¿Reiniciar el equipo?'))return;await api('/api/reboot',{method:'POST'});waitReboot()}
async function factory(){if(prompt('Se borrará toda la configuración (red incluida). Escriba FABRICA para confirmar')!=='FABRICA')return;await api('/api/factory',{method:'POST'});waitReboot(null,'Valores de f&aacute;brica. Si la IP cambi&oacute;, con&eacute;ctese al WiFi del equipo y abra http://192.168.4.1')}

// ---------- MQTT / API ----------
function iotRender(){if(!C)return;const b=C.mqtt.baseEff||'aysafi/'+C.net.host;$('#mqBaseEff').textContent='aysafi/'+C.net.host;const base=C.mqtt.base||'aysafi/'+C.net.host;
$('#tTopics').innerHTML=[['&lt;&mdash;',base+'/status','estado (retenido; LWT {"online":false})'],['&lt;&mdash;',base+'/data/&lt;nombre&gt;','valores de las lecturas marcadas MQTT'],['&mdash;&gt;',base+'/cmd','{"id":1,"uid":1,"fc":3,"addr":0,"qty":10}'],['&lt;&mdash;',base+'/cmd/resp','respuesta de cada comando']].map(r=>`<tr><td>${r[0]}</td><td>${r[1]}</td><td style="font-family:inherit;color:var(--mute)">${r[2]}</td></tr>`).join('');
$('#apiState').textContent=C.sys.apiKeySet?'configurada':'no configurada';const h=location.host,k=C.sys.apiKeySet?' -H "X-API-Key: &lt;clave&gt;"':' -u admin:&lt;clave&gt;';
$('#tApi').innerHTML=[`curl${k} "http://${h}/api/read?uid=1&amp;fc=3&amp;addr=0&amp;qty=10"`,`curl${k} -X POST http://${h}/api/write -d '{"uid":1,"fc":6,"addr":10,"value":123}'`,`curl${k} http://${h}/api/tags`].map(x=>`<tr><td style="word-break:break-all">${x}</td></tr>`).join('')}
async function apiGen(){const a=new Uint8Array(18);crypto.getRandomValues(a);const k=[...a].map(b=>b.toString(16).padStart(2,'0')).join('');
if(!confirm('Se guardará una nueva clave de API (reinicia). Anótela: no se vuelve a mostrar.'))return;
$('#apiNew').textContent=k;$('#apiNew').classList.remove('hide');try{await api('/api/config',{method:'POST',body:{sys:{apiKey:k}}});alert('Clave de API:\n\n'+k);waitReboot()}catch(e){toast(e.message,1)}}
async function apiClear(){if(!confirm('¿Quitar la clave de API? (reinicia)'))return;try{await api('/api/config',{method:'POST',body:{sys:{apiKeyClear:true}}});waitReboot()}catch(e){toast(e.message,1)}}

// ---------- herramientas ----------
async function tool(body,onTick){if(toolT){toast('Hay otra herramienta en curso',1);return null}
await api('/api/tool',{method:'POST',body});return new Promise(res=>{toolT=setInterval(async()=>{let j;try{j=await api('/api/tool')}catch(e){return}onTick(j);if(!j.active){clearInterval(toolT);toolT=null;res(j)}},600)})}
async function toolStop(){await api('/api/tool',{method:'POST',body:{kind:'stop'}}).catch(()=>{})}
const pct=j=>j.total?(100*j.progress/j.total)+'%':'0%';
$('#detMode').onchange=()=>$('#detIdsL').classList.toggle('hide',$('#detMode').value=='1');
async function detect(){if(toolT){toolStop();return}$('#detApply').classList.add('hide');$('#detBtn').textContent='Detener';
try{const j=await tool({kind:'detect',port:+$('#dPort').value,passive:$('#detMode').value=='1',ids:$('#detIds').value},j=>{$('#detBar').style.width=pct(j);$('#detTxt').textContent=j.status});
if(j&&j.found){$('#detApply').classList.remove('hide');toast('Configuración encontrada: '+j.baud+' 8'+j.parity+'1')}}catch(e){toast(e.message,1)}$('#detBtn').textContent='Detectar'}
async function detApply(){try{await api('/api/tool',{method:'POST',body:{kind:'apply'}});C=await api('/api/config');fill();$('#detApply').classList.add('hide');toast('Aplicado y guardado en el puerto '+$('#dPort').value)}catch(e){toast(e.message,1)}}
$('#tFc').onchange=()=>{const f=+$('#tFc').value,w=f==5||f==6||f==15||f==16;$('#tQtyL').classList.toggle('hide',w);$('#tValL').classList.toggle('hide',!w)};
async function runTest(b,q){const f=q?q.fc:+$('#tFc').value;q=q||{port:+$('#dPort').value,uid:+$('#tUid').value,fc:f,addr:+$('#tAddr').value,qty:+$('#tQty').value};
const vs=$('#tVal').value.split(/[,; ]+/).filter(x=>x!=='').map(Number);if(f==5||f==6)q.value=vs[0]||0;if(f==15||f==16)q.values=vs;
if(b)b.disabled=true;$('#tRes').textContent='...';try{const j=await fetch('/api/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(q)}).then(r=>r.json());
$('#tRes').innerHTML=(j.ok?'<span class="tag ok">'+esc(j.result)+'</span>':'<span class="tag bad">'+(j.exception?('Excepci&oacute;n '+j.exception+': '+(EXC[j.exception]||'')):esc(j.result||j.error))+'</span>')+' '+(j.lat||0)+' ms';
let h='';if(j.values){h='<div class=tw><table><thead><tr><th>Direcci&oacute;n</th><th class=num>Decimal</th><th class=num>Con signo</th><th class=num>Hex</th><th class=num>Binario</th></tr></thead><tbody>'+j.values.map((v,i)=>`<tr><td>${q.addr+i}</td><td class=num>${v}</td><td class=num>${f<=2?v:(v>32767?v-65536:v)}</td><td class="num mono">${f<=2?'':'0x'+v.toString(16).toUpperCase().padStart(4,'0')}</td><td class="num mono">${f<=2?'':v.toString(2).padStart(16,'0').replace(/(.{4})/g,'$1 ')}</td></tr>`).join('')+'</tbody></table></div>'}
if(j.raw)h+='<p class=mono style="color:var(--mute)">Respuesta: '+j.raw+'</p>';$('#tOut').innerHTML=h}catch(e){$('#tRes').textContent=e.message}if(b)b.disabled=false}
async function scan(){if(toolT){toolStop();return}$('#sBtn').textContent='Detener';
try{await tool({kind:'scan',port:+$('#dPort').value,from:+$('#sFrom').value,to:+$('#sTo').value,timeout:+$('#sTmo').value},j=>{$('#sBar').style.width=pct(j);$('#sTxt').textContent=j.status;
$('#sFound').innerHTML=(j.found||[]).map(u=>`<span class=chip onclick="$('#xUid').value=${u};$('#tUid').value=${u};discover()">ID ${u}</span>`).join('')})}catch(e){toast(e.message,1)}$('#sBtn').textContent='Escanear'}
function rangesHtml(j,wiz){if(!j.ranges||!j.ranges.length)return '<span style="color:var(--mute)">sin rangos (o el equipo no responde FC 1-4)</span>';
return j.ranges.map(r=>{const q=Math.min(r.to-r.from+1,r.fc<=2?64:20);return `<div style="margin:3px 0"><span class="tag">FC0${r.fc}</span> <span class=mono>${r.from} &ndash; ${r.to}</span> ${wiz?'':`<button class="btn gh sm" onclick="runTest(null,{port:${j.port+1},uid:${j.uid},fc:${r.fc},addr:${r.from},qty:${q}});window.scrollTo(0,0)">Leer</button> <button class="btn gh sm" onclick="pollAdd({port:${j.port},uid:${j.uid},fc:${r.fc},addr:${r.from},qty:${Math.min(r.to-r.from+1,r.fc<=2?2000:125)},name:'u${j.uid}_${r.from}'});toast('Agregada a Sondeo y cach&eacute; (guarde para aplicar)')">+ Sondeo</button>`}</div>`}).join('')}
async function discover(){if(toolT){toolStop();return}$('#xBtn').textContent='Detener';$('#xOut').innerHTML='';
try{const j=await tool({kind:'discover',port:+$('#dPort').value,uid:+$('#xUid').value,step:+$('#xStep').value,max:+$('#xMax').value},j=>{$('#xBar').style.width=pct(j);$('#xTxt').textContent=j.status});
if(j)$('#xOut').innerHTML=`<p><b>Equipo:</b> ${esc([j.vendor,j.product,j.rev].filter(x=>x).join(' · ')||'sin identificación (FC43 no soportada)')}</p><p style="margin:6px 0 4px"><b>Rangos que responden</b> <small style="color:var(--mute)">(bloques de ${j.step})</small></p>`+rangesHtml(j)}catch(e){toast(e.message,1)}$('#xBtn').textContent='Explorar'}

// ---------- monitor ----------
async function monToggle(){monOn=!monOn;$('#mBtn').textContent=monOn?'Pausar':'Iniciar';await api('/api/monitor',{method:'POST',body:{en:monOn}}).catch(()=>{});if(monOn){if(!monRows.length)$('#mBody').innerHTML='';monPoll()}}
async function monClear(){monRows=[];$('#mBody').innerHTML='<tr><td colspan=6 class=empty>Sin tráfico.</td></tr>';await api('/api/monitor',{method:'POST',body:{clear:true}}).catch(()=>{})}
function decode(r){const b=r.h.split(' ').map(x=>parseInt(x,16));if(r.k==='EVT')return esc(r.x||'');let o=(r.k==='TCP>'||r.k==='>TCP')&&C&&C.mb.proto==0?6:0;const u=b[o],f=b[o+1];if(f==null)return esc(r.x||'');
let s='UID '+u+' · '+(f&128?'<span class="tag bad">EXC '+(EXC[b[o+2]]||b[o+2])+'</span>':(FC[f]||'FC '+f));if(r.x)s+=' · '+esc(r.x);return s}
async function monPoll(){if(!monOn)return;try{const j=await api('/api/monitor?since='+monSince);monSince=j.last;
if(j.items.length){const t0=Date.now()-(S?S.uptime*1000:0);const html=j.items.map(r=>{monRows.push(r);const d=new Date(t0+r.t);
return `<tr><td class=mono>${d.toLocaleTimeString('es',{hour12:false})}.${String(r.t%1000).padStart(3,'0')}</td><td class=dir style="color:${r.k[0]=='>'?'var(--acc)':(r.k=='EVT'?'var(--bad)':'var(--ok)')}">${r.k}</td><td>${r.p?'P'+r.p:''}</td><td class=num>${r.n||''}</td><td class="mono h">${r.h}</td><td>${decode(r)}</td></tr>`}).reverse().join('');
const tb=$('#mBody');tb.insertAdjacentHTML('afterbegin',html);while(tb.rows.length>500)tb.deleteRow(-1)}}catch(e){}setTimeout(monPoll,700)}
function monCsv(){const t='seq;ms;dir;puerto;bytes;hex;nota\n'+monRows.map(r=>[r.s,r.t,r.k,r.p||'',r.n,r.h,r.x||''].join(';')).join('\n');const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([t],{type:'text/csv'}));a.download='monitor-modbus.csv';a.click()}

// ---------- sistema ----------
function fwUpload(b){const f=$('#fwFile').files[0];if(!f){toast('Seleccione un archivo .bin',1);return}if(!confirm('Instalar '+f.name+' ('+Math.round(f.size/1024)+' kB)?'))return;
const x=new XMLHttpRequest(),fd=new FormData();fd.append('firmware',f,f.name);b.disabled=true;
x.upload.onprogress=e=>{if(e.lengthComputable)$('#fwBar').style.width=(100*e.loaded/e.total)+'%'};
x.onload=()=>{b.disabled=false;let j={};try{j=JSON.parse(x.responseText)}catch(e){}if(x.status==200&&j.ok)waitReboot(null,'Firmware instalado. Reiniciando&hellip;');else toast('Error: '+(j.error||x.status),1)};
x.onerror=()=>{b.disabled=false;toast('Error de red',1)};x.open('POST','/api/fw');x.send(fd)}
async function ota(apply){if(apply&&!confirm('Descargar e instalar la última versión publicada en GitHub? El equipo necesita salida a internet por la LAN.'))return;
try{await api('/api/ota',{method:'POST',body:{apply}});toast(apply?'Instalando si hay una versión nueva...':'Consultando GitHub...');
let k=0;const t=setInterval(async()=>{await refresh();if(++k>60||/al dia|error|disponible/.test(S.ota.state))clearInterval(t);if(/reiniciando/.test(S.ota.state)){clearInterval(t);waitReboot(null,'Instalando actualizaci&oacute;n&hellip;')}},1500)}catch(e){toast(e.message,1)}}
async function restore(){const f=$('#rsFile').files[0];if(!f){toast('Seleccione el archivo de respaldo',1);return}if(!confirm('Reemplazar TODA la configuración con '+f.name+'?'))return;
try{await api('/api/restore',{method:'POST',body:await f.text()});waitReboot()}catch(e){toast('Error: '+e.message,1)}}

// ---------- asistente ----------
let W=null,wStep=0,wFound=[];
function wizOpen(){if(!C)return;W=JSON.parse(JSON.stringify(collect()));wStep=0;wFound=[];
$$('[data-wk]').forEach(e=>{const v=get(W,e.dataset.wk);if(e.dataset.t==='b')e.value=v?'1':'0';else e.value=v==null?'':v});
$('#wBaud').innerHTML=BAUDS.map(b=>opt(b,b)).join('');$('#wPass').value=$('#wPass2').value='';$('#wPassNote').classList.toggle('hide',!(S&&S.defPass));
wizPortLoad();wizRender();$('#wiz').classList.add('on')}
function wizCollect(){$$('[data-wk]').forEach(e=>set(W,e.dataset.wk,readEl(e)));const p=+$('#wPort').value;W.ports[p].en=true;W.ports[p].baud=+$('#wBaud').value;W.ports[p].parity=$('#wPar').value;W.ports[p].stop=+$('#wStop').value;W.ports[p].data=8}
function wizPortLoad(){const p=W.ports[+$('#wPort').value];$('#wBaud').value=p.baud;$('#wPar').value=p.parity;$('#wStop').value=p.stop}
$('#wPort').onchange=wizPortLoad;
function wizCond(){wizCollect();$$('[data-wif]').forEach(e=>{let ok=true;try{ok=new Function('w','return '+e.dataset.wif)(W)}catch(x){}e.classList.toggle('hide',!ok)})}
document.addEventListener('change',e=>{if(e.target.dataset&&e.target.dataset.wk)wizCond()});
function wizRender(){$$('[data-w]').forEach(s=>s.classList.toggle('hide',+s.dataset.w!==wStep));$$('#wSteps li').forEach((l,i)=>{l.className=i==wStep?'on':(i<wStep?'done':'')});
$('#wBack').classList.toggle('hide',wStep==0);$('#wPrint').classList.toggle('hide',wStep!=4);$('#wNext').textContent=wStep==4?'Finalizar y reiniciar':'Siguiente';wizCond();if(wStep==4)wizSummary()}
async function wizGo(d){if(toolT){toast('Espere a que termine la herramienta en curso',1);return}wizCollect();
if(d>0){if(wStep==0){const a=$('#wPass').value,b=$('#wPass2').value;if(a||b||(S&&S.defPass)){if(a.length<6){toast('La clave debe tener al menos 6 caracteres',1);return}if(a!==b){toast('Las claves no coinciden',1);return}W.sys.pass=a}}
if(wStep==1&&!W.net.eth.dhcp){for(const k of ['ip','mask','gw'])if(!ipRe.test(W.net.eth[k]||'')){toast('Revise la dirección IP, máscara y puerta de enlace',1);return}}
if(wStep==2){const p=+$('#wPort').value;try{await api('/api/port/apply',{method:'POST',body:{port:p+1,cfg:W.ports[p]}})}catch(e){toast('No se pudo aplicar el puerto: '+e.message,1);return}}
if(wStep==4){W.sys.wizard=true;try{await api('/api/config',{method:'POST',body:W});waitReboot(W)}catch(e){toast('Error: '+e.message,1)}return}}
wStep=Math.max(0,Math.min(4,wStep+d));wizRender()}
async function wizSkip(){if(!confirm('¿Cerrar el asistente? Puede abrirlo después desde el menú.'))return;if(toolT)await toolStop();$('#wiz').classList.remove('on');api('/api/wizard/done',{method:'POST'}).catch(()=>{})}
async function wizDetect(){if(toolT){toolStop();return}$('#wDetBtn').textContent='Detener';
try{const j=await tool({kind:'detect',port:+$('#wPort').value+1,passive:false,ids:$('#wIds').value},j=>{$('#wDetBar').style.width=pct(j);$('#wDetTxt').textContent=j.status});
if(j&&j.found){$('#wBaud').value=j.baud;$('#wPar').value=j.parity;$('#wStop').value=1;$('#wsFrom').value=Math.max(1,j.uid);toast('Encontrado: '+j.baud+' 8'+j.parity+'1, Unit ID '+j.uid)}
else if(j)toast('No se encontró: revise el cableado (A/B) y el Unit ID, o configure a mano',1)}catch(e){toast(e.message,1)}$('#wDetBtn').textContent='Detectar automáticamente'}
function wizDevs(){$('#wDevs').innerHTML=wFound.length?wFound.map((d,i)=>`<tr><td><b>${d.uid}</b></td><td>${esc(d.ident||'-')}</td><td>${d.ranges?rangesHtml(d,true):'-'}</td><td><button class="btn gh sm" onclick="wizIdent(${i})">Identificar</button></td></tr>`).join(''):'<tr><td colspan=4 class=empty>Ningún equipo respondió en ese rango.</td></tr>'}
async function wizScan(){if(toolT){toolStop();return}$('#wScanBtn').textContent='Detener';
try{const j=await tool({kind:'scan',port:+$('#wPort').value+1,from:+$('#wsFrom').value,to:+$('#wsTo').value,timeout:150},j=>{$('#wScanBar').style.width=pct(j);$('#wScanTxt').textContent=j.status});
if(j){wFound=(j.found||[]).map(u=>({uid:u,port:+$('#wPort').value}));wizDevs()}}catch(e){toast(e.message,1)}$('#wScanBtn').textContent='Buscar equipos'}
async function wizIdent(i){const d=wFound[i];try{const j=await tool({kind:'discover',port:d.port+1,uid:d.uid,step:100,max:2000},j=>{$('#wScanBar').style.width=pct(j);$('#wScanTxt').textContent='ID '+d.uid+': '+j.status});
if(j){d.ident=[j.vendor,j.product,j.rev].filter(x=>x).join(' ')||'sin FC43';d.ranges=j.ranges;d.step=j.step;wizDevs()}}catch(e){toast(e.message,1)}}
function wizSummary(){const p=+$('#wPort').value,pc=W.ports[p],e=W.net.eth;
$('#wSum').innerHTML=`<table><tbody><tr><th>Equipo</th><td>${esc(W.sys.name)}</td></tr><tr><th>Modelo</th><td>ayModbusConverter v${S?S.fw:''} &middot; ID ${S?S.chip:''}</td></tr>
<tr><th>LAN</th><td class=mono>${e.dhcp?'DHCP (actual '+(S?S.net.eth.ip:'-')+')':esc(e.ip)+' / '+esc(e.mask)+' gw '+esc(e.gw)+(e.dns1?' dns '+esc(e.dns1):'')}</td></tr>
<tr><th>Nombre de red</th><td class=mono>${esc(W.net.host)}.local</td></tr><tr><th>Modbus TCP</th><td class=mono>puerto ${W.mb.port} &middot; ${W.mb.proto?'RTU sobre TCP':'MBAP'} &middot; hasta ${W.mb.maxCli} clientes</td></tr>
<tr><th>Puerto serie</th><td class=mono>P${p+1} ${esc(pc.name)} &middot; ${pc.baud} 8${pc.parity}${pc.stop} &middot; Unit ID ${pc.uidFrom}-${pc.uidTo}${pc.uidOff?' offset '+pc.uidOff:''}</td></tr>
<tr><th>Equipos</th><td>${wFound.length?wFound.map(d=>'ID '+d.uid+(d.ident?' ('+esc(d.ident)+')':'')).join(', '):'sin búsqueda'}</td></tr>
<tr><th>Portal WiFi</th><td class=mono>${esc(W.net.ap.ssid)}</td></tr><tr><th>Fecha</th><td>${new Date().toLocaleString('es')}</td></tr></tbody></table>`}

// ---------- inicio ----------
(async()=>{$('#portForms').innerHTML=[0,1].map(portForm).join('');$('#dPort').innerHTML='<option value="1">Puerto 1</option><option value="2">Puerto 2</option>';
try{C=await api('/api/config');fill()}catch(e){toast('No se pudo leer la configuración',1)}
$('#dPort').innerHTML=C?C.ports.map((p,i)=>`<option value="${i+1}">Puerto ${i+1} &mdash; ${esc(p.name)}${p.en?'':' (apagado)'}</option>`).join(''):$('#dPort').innerHTML;
const h=location.hash.slice(1);if(h&&$('#p-'+h))show(h);await refresh();if(S&&!S.wizard)wizOpen();
setInterval(()=>{if(['dash','sys','ser','net','iot'].includes(page)||$('#wiz').classList.contains('on'))refresh();if(page==='poll'){refreshTags();refreshLearn()}},1500)})();
</script></body></html>)UI";
