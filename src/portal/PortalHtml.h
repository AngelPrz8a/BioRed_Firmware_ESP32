#pragma once
#include <Arduino.h>

// Portal completo en una sola página: sin LittleFS, sin fuentes ni librerías externas.
const char PORTAL_HTML[] PROGMEM = R"HTML(<!doctype html><html lang=es><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>ESP32 WiFi Modular</title>
<style>
:root{--bg:#f3f6f3;--cd:#fff;--tx:#16221a;--mu:#66786c;--bd:#d9e3db;--ac:#2d8a4b;--er:#c0392b}
@media(prefers-color-scheme:dark){:root{--bg:#0d130f;--cd:#141c16;--tx:#e2eee5;--mu:#8a9b8f;--bd:#243127;--ac:#4cb36d;--er:#e0675a}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.45 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:480px;margin:0 auto;padding:10px 14px 48px}
h1{font-size:19px;margin:0}
header,.row{display:flex;justify-content:space-between;align-items:center;gap:10px}
header{padding:8px 0}
nav{display:flex;margin:6px 0 4px;border-bottom:1px solid var(--bd)}
nav button{flex:1;background:none;border:0;border-bottom:2px solid transparent;padding:10px 2px;color:var(--mu);font:inherit;cursor:pointer}
nav button.on{color:var(--tx);border-color:var(--ac);font-weight:600}
.card{background:var(--cd);border:1px solid var(--bd);border-radius:10px;padding:12px 14px;margin:10px 0}
.card h2{font-size:14px;margin:0 0 8px;font-weight:600}
.card h2 small{font-weight:400;margin-left:6px}
.vals{display:grid;grid-template-columns:1fr 1fr;gap:8px 12px}
.vals span{display:flex;flex-direction:column;font-size:12px;color:var(--mu)}
.vals b{font-size:22px;color:var(--tx);font-weight:600;line-height:1.2}
.row{padding:6px 0}
label{display:block;font-size:13px;color:var(--mu);margin:12px 0 4px}
label.row{color:var(--tx);font-size:15px;margin:0;padding:6px 0}
input:not([type=checkbox]),select{width:100%;padding:9px 10px;border:1px solid var(--bd);border-radius:8px;background:var(--bg);color:var(--tx);font:inherit}
input:focus,button:focus-visible{outline:2px solid var(--ac);outline-offset:1px}
input[type=number].pin{width:76px}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--ac)}
.p{width:100%;margin-top:14px;padding:11px;border:0;border-radius:8px;background:var(--ac);color:#fff;font:inherit;font-weight:600;cursor:pointer}
.d{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--er);margin:0 4px 0 12px}
.d.ok{background:var(--ac)}
small,.mu{color:var(--mu);font-size:13px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--tx);color:var(--bg);padding:9px 14px;border-radius:8px;opacity:0;transition:opacity .2s;pointer-events:none;max-width:90%}
#toast.show{opacity:1}
.pad{display:grid;grid-template-columns:repeat(3,54px);gap:6px;justify-content:center;margin:14px 0 4px}
.pad button{height:54px;border:1px solid var(--bd);border-radius:10px;background:var(--cd);color:var(--tx);font-size:18px;cursor:pointer}
.pad button:active{background:var(--bd)}
img{width:100%;border-radius:8px;background:var(--bd);min-height:140px;display:block}
.btns{display:flex;gap:8px;margin-top:10px}.btns button{flex:1;padding:9px;border:1px solid var(--bd);border-radius:8px;background:var(--cd);color:var(--tx);font:inherit;cursor:pointer}
</style></head><body><main>
<header><h1>ESP32 WiFi Modular</h1><span id=pills class=mu></span></header>
<nav id=nav><button data-t=estado class=on>Estado</button><button data-t=sensores>Sensores</button><button data-t=red>Red</button><button data-t=conexion>Conexión</button><button data-t=cam id=bcam hidden>Cámara</button></nav>

<section id=estado><div id=sens></div><div class=card id=info></div></section>

<section id=sensores hidden>
<div class=card id=slist></div>
<p class=mu>Al guardar, la placa se reinicia para aplicar los cambios.</p>
<button class=p onclick=gSens()>Guardar y reiniciar</button></section>

<section id=red hidden><div class=card>
<div class=row><span class=mu>Red actual</span><b id=rnow>—</b></div>
<div class=btns><button onclick=redes()>Buscar redes</button></div>
<label for=lista>Redes encontradas</label>
<select id=lista onchange="if(this.value)$('#ssid').value=this.value"><option value="">Redes disponibles tras la búsqueda</option></select>
<label for=ssid>Nombre de la red</label><input id=ssid autocomplete=off autocapitalize=off>
<label for=wpass>Contraseña</label><input id=wpass type=password placeholder="Sin cambios">
<button class=p onclick=gRed()>Guardar y reiniciar</button></div></section>

<section id=conexion hidden>
<div class=card><h2>API HTTPS</h2>
<label class=row>Enviar lecturas<input type=checkbox id=aon></label>
<label for=aurl>URL</label><input id=aurl inputmode=url>
<label for=akey>Key de dispositivo</label><input id=akey type=password placeholder="Sin cambios">
<label for=aint>Intervalo (segundos)</label><input id=aint type=number min=10></div>
<div class=card><h2>MQTT</h2>
<label class=row>Publicar lecturas<input type=checkbox id=mon></label>
<label for=mhost>Servidor</label><input id=mhost placeholder="192.168.1.50" autocapitalize=off>
<label for=mport>Puerto</label><input id=mport type=number>
<label for=muser>Usuario (opcional)</label><input id=muser autocapitalize=off>
<label for=mpass>Contraseña (opcional)</label><input id=mpass type=password placeholder="Sin cambios">
<label for=mraiz>Raíz de los topics</label><input id=mraiz autocapitalize=off>
<label for=mmod>Nombre de esta estación</label><input id=mmod autocapitalize=off>
<label for=mint>Intervalo (segundos)</label><input id=mint type=number min=2>
<p class=mu id=mtop></p></div>
<div class=card><h2>Acceso al portal</h2>
<label for=ppass>Contraseña (usuario admin)</label><input id=ppass type=password placeholder="Sin cambios"></div>
<button class=p onclick=gCon()>Guardar cambios</button></section>

<section id=cam hidden><div class=card>
<img id=img alt="Vista de la cámara">
<div class=btns><button onclick=foto()>Tomar foto</button><button onclick=live(1)>Ver en vivo</button><button onclick=live(0)>Detener</button></div>
<p class=mu>Mientras la vista en vivo está abierta, el portal no responde a otras acciones.</p>
<div id=pad><div class=pad>
<span></span><button onclick="mv('ARRIBA')" aria-label=Arriba>▲</button><span></span>
<button onclick="mv('IZQ')" aria-label=Izquierda>◄</button><span></span><button onclick="mv('DER')" aria-label=Derecha>►</button>
<span></span><button onclick="mv('ABAJO')" aria-label=Abajo>▼</button><span></span></div>
<label for=pasos>Pasos por pulsación</label><select id=pasos><option>10<option selected>20<option>50<option>100</select></div>
</div></section>
<div id=toast></div></main>
<script>
const $=q=>document.querySelector(q),$$=q=>document.querySelectorAll(q),v=i=>$('#'+i).value.trim();
const J=(u,o)=>fetch(u,o).then(r=>{if(!r.ok)throw r.status;return r.json()});
const P=(u,b)=>J(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});
const dot=k=>'<i class="d'+(k?' ok':'')+'"></i>';
let C=null,T,L=0;
function toast(m){const t=$('#toast');t.textContent=m;t.className='show';clearTimeout(T);T=setTimeout(()=>t.className='',2800)}
function tab(n){$$('section').forEach(s=>s.hidden=s.id!=n);$$('nav button').forEach(b=>b.className=b.dataset.t==n?'on':'');if(n=='red')redes();if(n!='cam')live(0);if(n=='cam')foto()}
$('#nav').onclick=e=>{if(e.target.dataset.t)tab(e.target.dataset.t)};
const up=s=>Math.floor(s/3600)+' h '+Math.floor(s%3600/60)+' min';
async function estado(){
 if(L)return;
 try{const d=await J('/api/estado');
  $('#pills').innerHTML=dot(d.wifi.ok)+'WiFi'+dot(d.api.on&&d.api.ultimo==201)+'API'+dot(d.mqtt.on&&d.mqtt.ok)+'MQTT';
  $('#sens').innerHTML=d.sensores.filter(s=>s.on).map(s=>'<div class=card><h2>'+s.nombre+'<small>'+s.estado+'</small></h2><div class=vals>'+Object.entries(s.datos).map(([k,x])=>'<span>'+k+'<b>'+x+'</b></span>').join('')+'</div></div>').join('')||'<div class=card><p class=mu>No hay sensores activos. La activación se realiza en la pestaña Sensores.</p></div>';
  $('#info').innerHTML=[['Dirección',d.wifi.ip+(d.wifi.ap?' (modo configuración)':'')],['Señal',d.wifi.ok?d.wifi.rssi+' dBm':'sin red'],['Encendida',up(d.uptime)],['Memoria libre',Math.round(d.heap/1024)+' KB'],['API',d.api.on?(d.api.ultimo==201?d.api.enviados+' envíos':d.api.ultimo?'error '+d.api.ultimo:'esperando'):'apagada'],['MQTT',d.mqtt.on?(d.mqtt.ok?d.mqtt.publicados+' publicados':d.mqtt.error||'conectando'):'apagado'],['Versión',d.version]].map(([k,x])=>'<div class=row><span class=mu>'+k+'</span><span>'+x+'</span></div>').join('');
  $('#bcam').hidden=!(d.camara||d.motores);$('#pad').hidden=!d.motores;
 }catch(e){$('#pills').innerHTML=dot(0)+'Sin conexión'}
}
async function cargar(){
 C=await J('/api/config');
 $('#slist').innerHTML=C.sensores.map((s,i)=>'<div class=row><label class=row><input type=checkbox id=s'+i+(s.on?' checked':'')+'>'+s.nombre+'</label><span class=mu>GPIO <input class=pin type=number id=p'+i+' min=0 max=48 value='+s.pin+'></span></div>').join('');
 $('#rnow').textContent=C.wifi.ssid||'—';$('#ssid').value=C.wifi.ssid;
 const a=C.api,m=C.mqtt,s=(i,x)=>$('#'+i).value=x;
 $('#aon').checked=a.on;s('aurl',a.url);s('aint',a.intervalo);$('#akey').placeholder=a.keySet?'Guardada (sin cambios)':'Clave de dispositivo para autenticación';
 $('#mon').checked=m.on;s('mhost',m.host);s('mport',m.port);s('muser',m.user);s('mraiz',m.raiz);s('mmod',m.modulo);s('mint',m.intervalo);
 $('#mpass').placeholder=m.passSet?'Guardada (sin cambios)':'';$('#ppass').placeholder=C.portalPassSet?'Guardada (sin cambios)':'Sin contraseña';
 $('#mtop').textContent='Publica en '+m.raiz+'/'+m.modulo+'/<sensor> y '+m.raiz+'/'+m.modulo+'/estado';
}
const reinicia=()=>{toast('Reiniciando, vuelve a abrir el portal en unos segundos');setTimeout(()=>location.reload(),10000)};
const fallo=()=>toast('No se pudo guardar');
const gSens=()=>P('/api/config',{sensores:C.sensores.map((s,i)=>({id:s.id,on:$('#s'+i).checked,pin:+$('#p'+i).value}))}).then(reinicia).catch(fallo);
const gRed=()=>v('ssid')?P('/api/config',{wifi:{ssid:v('ssid'),pass:$('#wpass').value}}).then(reinicia).catch(fallo):toast('Debe seleccionarse una red');
const gCon=()=>P('/api/config',{api:{on:$('#aon').checked,url:v('aurl'),key:v('akey'),intervalo:+v('aint')},mqtt:{on:$('#mon').checked,host:v('mhost'),port:+v('mport'),user:v('muser'),pass:$('#mpass').value,raiz:v('mraiz'),modulo:v('mmod'),intervalo:+v('mint')},portalPass:$('#ppass').value}).then(()=>{toast('Cambios guardados');['akey','mpass','ppass'].forEach(i=>$('#'+i).value='');cargar();estado()}).catch(fallo);
let R=0;
async function redes(){
 if(R)return;R=1;toast('Buscando redes…');
 for(let i=0;i<8;i++){
  const d=await J('/api/redes').catch(()=>null);
  if(!d){R=0;toast('No se pudo buscar redes');return}
  if(!d.escaneando){
   $('#lista').replaceChildren(new Option(d.redes.length?'Debe seleccionarse una red':'No se encontraron redes',''),...d.redes.map(r=>new Option(r.ssid+' ('+r.rssi+' dBm)',r.ssid)));
   R=0;toast(d.redes.length+' redes encontradas');return}
  await new Promise(r=>setTimeout(r,2000));
 }
 R=0;toast('La búsqueda excedió el tiempo de espera');
}
function foto(){L=0;$('#img').src='/resources/cam/cam-lo.jpg?t='+Date.now()}
function live(on){const i=$('#img');if(on){L=1;i.src='/resources/cam/live?t='+Date.now()}else{L=0;i.removeAttribute('src')}}
async function mv(d){try{const r=await J('/api/mover?dir='+d+'&pasos='+$('#pasos').value);toast(r.ok?'Movimiento completado':r.error)}catch(e){toast('No se pudo mover')}}
document.addEventListener('keydown',e=>{if($('#cam').hidden||/INPUT|SELECT/.test(e.target.tagName))return;const m={ArrowUp:'ARRIBA',ArrowDown:'ABAJO',ArrowLeft:'IZQ',ArrowRight:'DER'}[e.key];if(m){e.preventDefault();mv(m)}});
cargar().catch(()=>{});estado();setInterval(estado,5000);
</script></body></html>)HTML";
