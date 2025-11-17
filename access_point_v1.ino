// Daniel Nalepa, Carter Hill, Adam Slamani
// ESP32 Cocktail Dispenser Web Server with dynamic single-page GUI
// Access Point version

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h> // Needed for JSON parsing in saveCocktail

// Create web server on port 80
WebServer server(80);

// Simulated pump states and stored cocktails
int pumpVolumes[6] = {0, 0, 0, 0, 0, 0};
bool pumpRunning[6] = {false, false, false, false, false, false};

// timing for simulated 1 mL/sec dispensing
unsigned long pumpEndTime[6] = {0,0,0,0,0,0};

struct Cocktail {
  String name;
  int volumes[6];
  String pumpNames[6];
};
#define MAX_COCKTAILS 10
Cocktail storedCocktails[MAX_COCKTAILS];
int cocktailCount = 0;

unsigned long startTime;

/////////////////////////
// Helper Functions
/////////////////////////
String getUptime() {
  unsigned long ms = millis() - startTime;
  unsigned long s = ms / 1000;
  unsigned long m = s / 60;
  unsigned long h = m / 60;
  s = s % 60;
  m = m % 60;
  char buf[32];
  sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

/////////////////////////
// Web Handlers
/////////////////////////

void handleRoot() {
  server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>ESP32 Cocktail Dispenser</title>
<style>
body { font-family: Arial; margin:20px; background:#f4f4f9; }
h1 { color:#333; }
section { background:#fff; padding:15px; margin-bottom:20px; border-radius:10px; box-shadow:0 0 5px #aaa; }
label { display:block; margin-top:10px; }
input[type=range] { width:100%; }
button { margin-top:10px; padding:10px 20px; border:none; background:#4CAF50; color:white; border-radius:5px; cursor:pointer; }
button:hover { background:#45a049; }
select { width:100%; padding:5px; margin-top:5px; }
#status span { font-weight:bold; }
</style>
</head>
<body>
<h1>ESP32 Cocktail Dispenser</h1>

<section>
<h2>Manual Control</h2>
<form id="manualForm">
<label>Pump 1: <span id="vol1">0</span> mL</label>
<input type="range" min="0" max="100" value="0" id="pump1">
<label>Pump 2: <span id="vol2">0</span> mL</label>
<input type="range" min="0" max="100" value="0" id="pump2">
<label>Pump 3: <span id="vol3">0</span> mL</label>
<input type="range" min="0" max="100" value="0" id="pump3">
<label>Pump 4: <span id="vol4">0</span> mL</label>
<input type="range" min="0" max="100" value="0" id="pump4">
<label>Pump 5: <span id="vol5">0</span> mL</label>
<input type="range" min="0" max="100" value="0" id="pump5">
<label>Pump 6: <span id="vol6">0</span> mL</label>
<input type="range" min="0" max="100" value="0" id="pump6">
<button type="button" onclick="dispenseManual()">Dispense</button>
</form>
</section>

<section>
<h2>Create Custom Cocktail</h2>
<form id="saveForm">
<label>Cocktail Name:</label>
<input type="text" id="cocktailName" placeholder="Enter name">
<label>Pump Liquid Names (1-6)</label>
<input type="text" id="n1" placeholder="Pump 1 name">
<input type="text" id="n2" placeholder="Pump 2 name">
<input type="text" id="n3" placeholder="Pump 3 name">
<input type="text" id="n4" placeholder="Pump 4 name">
<input type="text" id="n5" placeholder="Pump 5 name">
<input type="text" id="n6" placeholder="Pump 6 name">
<label>Pump Volumes (0-100 mL)</label>
<input type="number" id="c1" min="0" max="100" value="0">
<input type="number" id="c2" min="0" max="100" value="0">
<input type="number" id="c3" min="0" max="100" value="0">
<input type="number" id="c4" min="0" max="100" value="0">
<input type="number" id="c5" min="0" max="100" value="0">
<input type="number" id="c6" min="0" max="100" value="0">
<button type="button" onclick="saveCocktail()">Save Cocktail</button>
</form>
</section>

<section>
<h2>Stored Cocktails</h2>
<select id="cocktailList" size="5"></select><br>
<button onclick="dispenseStored()">Dispense Selected</button>
<button onclick="deleteStored()">Delete Selected</button>
</section>

<section>
<h2>About / Status</h2>
<div id="status">
<p>IP Address: <span id="ip">-</span></p>
<p>Uptime: <span id="uptime">-</span></p>
<p>WiFi: <span id="wifi">-</span></p>
<p>Pumps:</p>
<ul>
<li>Pump 1: <span id="p1">Idle</span></li>
<li>Pump 2: <span id="p2">Idle</span></li>
<li>Pump 3: <span id="p3">Idle</span></li>
<li>Pump 4: <span id="p4">Idle</span></li>
<li>Pump 5: <span id="p5">Idle</span></li>
<li>Pump 6: <span id="p6">Idle</span></li>
</ul>
</div>
</section>

<script>
function updateSliders() {
  for(let i=1;i<=6;i++){
    let s = document.getElementById('pump'+i);
    let v = document.getElementById('vol'+i);
    v.textContent = s.value;
    s.oninput = ()=>{v.textContent = s.value;}
  }
}

async function fetchStatus(){
  try{
    let resp = await fetch('/status');
    let data = await resp.json();
    document.getElementById('uptime').textContent = data.uptime;
    document.getElementById('ip').textContent = data.ip;
    document.getElementById('wifi').textContent = data.wifi;
    for(let i=0;i<6;i++){
      document.getElementById('p'+(i+1)).textContent = data.pumps[i] ? 'Running':'Idle';
    }
  }catch(e){ console.log(e);}
}

async function fetchCocktails(){
  try{
    let resp = await fetch('/getCocktails');
    let data = await resp.json();
    let list = document.getElementById('cocktailList');
    list.innerHTML='';
    data.forEach(c=>{
      let opt = document.createElement('option');
      opt.value=c.name;
      opt.text=c.name;
      list.appendChild(opt);
    });
  }catch(e){ console.log(e);}
}

async function dispenseManual(){
  let params=[];
  for(let i=1;i<=6;i++){
    params.push('m'+i+'='+document.getElementById('pump'+i).value);
  }
  await fetch('/dispense?'+params.join('&'));
  fetchStatus();
}

async function saveCocktail(){
  let obj={
    name: document.getElementById('cocktailName').value,
    volumes:[],
    names:[]
  };
  for(let i=1;i<=6;i++){
    obj.names.push(document.getElementById('n'+i).value);
    obj.volumes.push(Number(document.getElementById('c'+i).value));
  }
  await fetch('/saveCocktail', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify(obj)
  });
  fetchCocktails();
}

async function dispenseStored(){
  let sel = document.getElementById('cocktailList').value;
  if(sel) await fetch('/dispenseCocktail?name='+encodeURIComponent(sel));
  fetchStatus();
}

async function deleteStored(){
  let sel = document.getElementById('cocktailList').value;
  if(sel) await fetch('/deleteCocktail?name='+encodeURIComponent(sel),{method:'POST'});
  fetchCocktails();
}

updateSliders();
setInterval(fetchStatus,1000);
setInterval(fetchCocktails,5000);
fetchStatus();
fetchCocktails();
</script>

</body>
</html>
)rawliteral");
}

/////////////////////////
// Status & Cocktail Handlers
/////////////////////////
void handleStatus() {
  String wifiStatus = "Access Point";
  String json = "{";
  json += "\"uptime\":\"" + getUptime() + "\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"wifi\":\"" + wifiStatus + "\",";
  json += "\"pumps\":[";
  for(int i=0;i<6;i++){
    json += pumpRunning[i] ? "true":"false";
    if(i<5) json+=",";
  }
  json += "]}";
  server.send(200,"application/json",json);
}

void handleGetCocktails() {
  String json = "[";
  for(int i=0;i<cocktailCount;i++){
    json += "{\"name\":\""+storedCocktails[i].name+"\",\"volumes\":[";
    for(int j=0;j<6;j++){
      json += String(storedCocktails[i].volumes[j]);
      if(j<5) json += ",";
    }
    json += "],\"names\":[";
    for(int j=0;j<6;j++){
      json += "\"" + storedCocktails[i].pumpNames[j] + "\"";
      if(j<5) json += ",";
    }
    json += "]}";
    if(i<cocktailCount-1) json+=",";
  }
  json += "]";
  server.send(200,"application/json",json);
}

void handleSaveCocktail() {
  if(cocktailCount>=MAX_COCKTAILS){ server.send(200,"text/plain","Max cocktails reached"); return; }
  if(!server.hasArg("plain")){ server.send(400); return; }
  String body = server.arg("plain");
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, body);
  String name = doc["name"];
  JsonArray vols = doc["volumes"];
  JsonArray names = doc["names"];
  storedCocktails[cocktailCount].name = name;
  for(int i=0;i<6;i++){
    storedCocktails[cocktailCount].volumes[i] = vols[i];
    storedCocktails[cocktailCount].pumpNames[i] = names[i].as<String>();
  }
  cocktailCount++;
  server.send(200,"text/plain","OK");
}

void handleDispense() {
  for(int i=0;i<6;i++){
    if(server.hasArg("m"+String(i+1))){
      int v = server.arg("m"+String(i+1)).toInt();
      pumpVolumes[i] = v;
      if(v>0){
        pumpRunning[i] = true;
        pumpEndTime[i] = millis() + (unsigned long)v * 1000UL;
      }
    }
  }
  server.send(200,"text/plain","OK");
}

void handleDispenseCocktail() {
  if(!server.hasArg("name")){ server.send(400); return; }
  String name = server.arg("name");
  for(int i=0;i<cocktailCount;i++){
    if(storedCocktails[i].name == name){
      for(int j=0;j<6;j++){
        pumpVolumes[j] = storedCocktails[i].volumes[j];
        if(pumpVolumes[j] > 0){
          pumpRunning[j] = true;
          pumpEndTime[j] = millis() + (unsigned long)pumpVolumes[j] * 1000UL;
        }
      }
      server.send(200,"text/plain","OK");
      return;
    }
  }
  server.send(404,"text/plain","Cocktail not found");
}

void handleDeleteCocktail() {
  if(!server.hasArg("name")){ server.send(400); return; }
  String name = server.arg("name");
  for(int i=0;i<cocktailCount;i++){
    if(storedCocktails[i].name == name){
      for(int j=i;j<cocktailCount-1;j++) storedCocktails[j]=storedCocktails[j+1];
      cocktailCount--;
      server.send(200,"text/plain","Deleted");
      return;
    }
  }
  server.send(404,"text/plain","Not found");
}

/////////////////////////
// Setup & Loop
/////////////////////////
void setup() {
  Serial.begin(115200);
  delay(1000);

  // === Start ESP32 as Access Point ===
  const char* ap_ssid = "CocktailMaker_AP";
  const char* ap_pass = "mixdrink123";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
  Serial.println("Access Point started!");
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_pass);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  startTime = millis();

  // Web server routes
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/getCocktails", handleGetCocktails);
  server.on("/saveCocktail", HTTP_POST, handleSaveCocktail);
  server.on("/dispense", handleDispense);
  server.on("/dispenseCocktail", handleDispenseCocktail);
  server.on("/deleteCocktail", HTTP_POST, handleDeleteCocktail);

  server.begin();
  Serial.println("Web server started");



}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  for(int i=0;i<6;i++){
    if(pumpRunning[i]){
      if(now >= pumpEndTime[i]){
        pumpRunning[i] = false;
        pumpVolumes[i] = 0;
        pumpEndTime[i] = 0;
      }
    }
  }
}
