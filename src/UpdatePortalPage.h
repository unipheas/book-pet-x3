#pragma once

inline constexpr char kBookPetUpdatePortalPage[] = R"BOOKPET_HTML(
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Book Pet Update</title>
<style>
  :root{color-scheme:light;--ink:#171713;--paper:#f5f1df;--line:#27271f;--soft:#e5dec2}
  *{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);
  font:16px/1.45 ui-monospace,SFMono-Regular,Menlo,monospace}
  main{max-width:680px;margin:auto;padding:28px 18px 60px}
  h1{font-size:30px;margin:0 0 4px}h2{font-size:18px;margin:0 0 14px}
  .pet{font-size:34px;float:right}.sub{margin:0 0 24px;color:#4c4b40}
  section{border:2px solid var(--line);padding:18px;margin:16px 0;background:#fffdf3}
  label{display:block;font-weight:700;margin:11px 0 5px}
  input{width:100%;font:inherit;padding:12px;border:2px solid var(--line);background:white}
  button{width:100%;margin-top:14px;padding:13px;border:2px solid var(--line);
  background:var(--ink);color:white;font:700 16px inherit;cursor:pointer}
  button:disabled{opacity:.45;cursor:not-allowed}
  progress{width:100%;height:18px;margin-top:14px;accent-color:var(--ink)}
  #status{white-space:pre-wrap;padding:12px;background:var(--soft);min-height:52px}
  small{display:block;margin-top:10px;color:#565448}
</style>
<main>
  <span class="pet">◉ᴗ◉</span>
  <h1>Book Pet Update</h1>
  <p class="sub">A private maintenance page served directly by your X3.</p>

  <section>
    <h2>Install an update file</h2>
    <input id="file" type="file" accept=".bin,application/octet-stream">
    <button id="upload">Verify and install</button>
    <progress id="progress" max="100" value="0"></progress>
    <small>Use the signed <b>book-pet-x3-update.bin</b> from an official release.
    Pet progress is stored separately and is preserved.</small>
  </section>

  <section>
    <h2>Get the latest official version</h2>
    <label for="ssid">Home Wi-Fi name</label>
    <input id="ssid" autocomplete="username">
    <label for="password">Home Wi-Fi password</label>
    <input id="password" type="password" autocomplete="current-password">
    <button id="official">Check and install</button>
    <small>The password is used in memory for this update only. Book Pet does
    not save it or send it anywhere.</small>
  </section>

  <section>
    <h2>Device status</h2>
    <div id="status">Connecting to Book Pet…</div>
  </section>
</main>
<script>
const $=id=>document.getElementById(id);
const hex=buf=>[...new Uint8Array(buf)].map(x=>x.toString(16).padStart(2,'0')).join('');
let busy=false;
async function poll(){
  try{
    const s=await fetch('/api/status',{cache:'no-store'}).then(r=>r.json());
    $('status').textContent=`${s.title}\n${s.detail}\nVersion ${s.version}${s.signed?' · signed updates required':' · developer mode'}`;
    if(!busy)$('progress').value=s.progress||0;
    if(/FAILED|BLOCKED|CANCELLED|UP TO DATE/.test(s.title)){
      busy=false;$('upload').disabled=false;$('official').disabled=false;
    }
  }catch(e){}
}
setInterval(poll,1000);poll();
$('upload').onclick=async()=>{
  const file=$('file').files[0];
  if(!file)return alert('Choose a Book Pet update file first.');
  if(!confirm(`Install ${file.name}? Keep the X3 powered until it restarts.`))return;
  busy=true;$('upload').disabled=true;$('official').disabled=true;
  $('status').textContent='Calculating SHA-256…';
  const sha=hex(await crypto.subtle.digest('SHA-256',await file.arrayBuffer()));
  const xhr=new XMLHttpRequest();
  xhr.open('POST',`/api/upload?size=${file.size}&sha256=${sha}`);
  xhr.upload.onprogress=e=>{if(e.lengthComputable)$('progress').value=e.loaded/e.total*100};
  xhr.onload=()=>{$('status').textContent=xhr.responseText||'Upload finished';
    if(xhr.status>=400){busy=false;$('upload').disabled=false;$('official').disabled=false}poll()};
  xhr.onerror=()=>{$('status').textContent='Upload connection failed';busy=false;
    $('upload').disabled=false;$('official').disabled=false};
  xhr.send(file);
};
$('official').onclick=async()=>{
  const ssid=$('ssid').value.trim(),password=$('password').value;
  if(!ssid)return alert('Enter your home Wi-Fi name.');
  if(!confirm('Connect briefly and install the latest official Book Pet?'))return;
  busy=true;$('upload').disabled=true;$('official').disabled=true;
  try{
    const body=new URLSearchParams({ssid,password});
    const r=await fetch('/api/official',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    $('status').textContent=await r.text();
    if(!r.ok){busy=false;$('upload').disabled=false;$('official').disabled=false}
    poll();
  }catch(e){
    $('status').textContent='Book Pet connection was interrupted';
    busy=false;$('upload').disabled=false;$('official').disabled=false;
  }
};
</script>
</html>
)BOOKPET_HTML";
