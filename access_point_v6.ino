// Daniel Nalepa, Carter Hill, Adam Slamani
// ESP32 Cocktail Dispenser Web Server with real pumps via L298N drivers
// Access Point version with Flash Storage

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h> // For flash storage

// Create web server on port 80
WebServer server(80);

// LCD Screen 
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Hardware configuration for the six pumps
const int inPins[12] = {3,4,5,6,7,8,9,10,A0,A1,A2,A3};

// Flow rates for each pump in mL/sec (calibrated)
const float pumpFlowRate[6] = {3.3, 3.3, 3.3, 3.3, 1.6, 1.6};

// State tracking 
int pumpVolumes[6] = {0,0,0,0,0,0};
bool pumpRunning[6] = {false,false,false,false,false,false};
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

Preferences prefs; // Flash storage object

// ----------------- Helper Functions -----------------
String getUptime() {
  unsigned long ms = millis() - startTime;
  unsigned long s = ms / 1000;
  unsigned long m = s / 60;
  unsigned long h = m / 60;
  s %= 60;
  m %= 60;
  char buf[32];
  sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

void hardwareStopPump(int idx) {
  digitalWrite(inPins[2*idx], LOW);
  digitalWrite(inPins[2*idx + 1], LOW);
}

void hardwareStartPump(int idx, uint8_t speed = 255) {
  digitalWrite(inPins[2*idx], LOW);
  digitalWrite(inPins[2*idx + 1], HIGH);
}

void hardwareReversePump(int idx) {
  digitalWrite(inPins[2*idx], HIGH);
  digitalWrite(inPins[2*idx + 1], LOW);
}

void startPump(int idx, int volume_mL) {
  if(idx<0||idx>=6||volume_mL<=0) return;
  pumpVolumes[idx] = volume_mL;
  pumpRunning[idx] = true;
  unsigned long duration = (unsigned long)((volume_mL / pumpFlowRate[idx])*1000.0);
  pumpEndTime[idx] = millis() + duration;
  hardwareStartPump(idx);
}

void stopPump(int idx) {
  if(idx<0||idx>=6) return;
  pumpRunning[idx] = false;
  pumpVolumes[idx] = 0;
  pumpEndTime[idx] = 0;
  hardwareStopPump(idx);
}

// ----------------- Flash Storage -----------------
void saveCocktailsToFlash() {
  prefs.begin("cocktails", false);
  prefs.putInt("count", cocktailCount);
  for(int i=0;i<cocktailCount;i++){
    DynamicJsonDocument doc(512);
    doc["name"] = storedCocktails[i].name;
    JsonArray vols = doc.createNestedArray("volumes");
    JsonArray names = doc.createNestedArray("names");
    for(int j=0;j<6;j++){
      vols.add(storedCocktails[i].volumes[j]);
      names.add(storedCocktails[i].pumpNames[j]);
    }
    String jsonStr;
    serializeJson(doc,jsonStr);
    prefs.putString(("cocktail"+String(i)).c_str(), jsonStr);
  }
  prefs.end();
}

void loadCocktailsFromFlash() {
  prefs.begin("cocktails", true);
  cocktailCount = prefs.getInt("count",0);
  for(int i=0;i<cocktailCount;i++){
    String jsonStr = prefs.getString(("cocktail"+String(i)).c_str(),"");
    if(jsonStr.length()>0){
      DynamicJsonDocument doc(512);
      deserializeJson(doc,jsonStr);
      storedCocktails[i].name = doc["name"].as<String>();
      JsonArray vols = doc["volumes"];
      JsonArray names = doc["names"];
      for(int j=0;j<6;j++){
        storedCocktails[i].volumes[j] = vols[j];
        storedCocktails[i].pumpNames[j] = names[j].as<String>();
      }
    }
  }
  prefs.end();
}

// ----------------- Web Handlers -----------------
void handleRoot() {
  server.send(200,"text/html", R"rawliteral(
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
.redBtn { background:red; color:white; }
.redBtn:hover { background:#cc0000; }
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
<button type="button" class="redBtn" onclick="cleanseTubing()">Cleanse Tubing</button>
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
    let s=document.getElementById('pump'+i);
    let v=document.getElementById('vol'+i);
    v.textContent=s.value;
    s.oninput=()=>{v.textContent=s.value;}
  }
}

async function fetchStatus(){
  try{
    let resp=await fetch('/status');
    let data=await resp.json();
    document.getElementById('uptime').textContent=data.uptime;
    document.getElementById('ip').textContent=data.ip;
    document.getElementById('wifi').textContent=data.wifi;
    for(let i=0;i<6;i++){
      document.getElementById('p'+(i+1)).textContent=data.pumps[i]?'Running':'Idle';
    }
  }catch(e){console.log(e);}
}

async function fetchCocktails(){
  try{
    let resp=await fetch('/getCocktails');
    let data=await resp.json();
    let list=document.getElementById('cocktailList');
    list.innerHTML='';
    data.forEach(c=>{
      let opt=document.createElement('option');
      opt.value=c.name;
      opt.text=c.name;
      list.appendChild(opt);
    });
  }catch(e){console.log(e);}
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
  let obj={name:document.getElementById('cocktailName').value,volumes:[],names:[]};
  for(let i=1;i<=6;i++){
    obj.names.push(document.getElementById('n'+i).value);
    obj.volumes.push(Number(document.getElementById('c'+i).value));
  }
  await fetch('/saveCocktail',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(obj)
  });
  fetchCocktails();
}

async function dispenseStored(){
  let sel=document.getElementById('cocktailList').value;
  if(sel) await fetch('/dispenseCocktail?name='+encodeURIComponent(sel));
  fetchStatus();
}

async function deleteStored(){
  let sel=document.getElementById('cocktailList').value;
  if(sel) await fetch('/deleteCocktail?name='+encodeURIComponent(sel),{method:'POST'});
  fetchCocktails();
}

async function cleanseTubing(){
  await fetch('/cleanse');
  fetchStatus();
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

void handleStatus() {
  String wifiStatus="Access Point";
  String json="{\"uptime\":\""+getUptime()+"\",\"ip\":\""+WiFi.softAPIP().toString()+"\",\"wifi\":\""+wifiStatus+"\",\"pumps\":[";
  for(int i=0;i<6;i++){
    json+=pumpRunning[i]?"true":"false";
    if(i<5) json+=",";
  }
  json+="]}";
  server.send(200,"application/json",json);
}

void handleGetCocktails(){
  String json="[";
  for(int i=0;i<cocktailCount;i++){
    json+="{\"name\":\""+storedCocktails[i].name+"\",\"volumes\":[";
    for(int j=0;j<6;j++){
      json+=String(storedCocktails[i].volumes[j]);
      if(j<5) json+=",";
    }
    json+="],\"names\":[";
    for(int j=0;j<6;j++){
      json+="\""+storedCocktails[i].pumpNames[j]+"\"";
      if(j<5) json+=",";
    }
    json+="]}";
    if(i<cocktailCount-1) json+=",";
  }
  json+="]";
  server.send(200,"application/json",json);
}

void handleSaveCocktail(){
  if(cocktailCount>=MAX_COCKTAILS){server.send(200,"text/plain","Max cocktails reached");return;}
  if(!server.hasArg("plain")){server.send(400);return;}
  String body=server.arg("plain");
  DynamicJsonDocument doc(1024);
  deserializeJson(doc,body);
  String name=doc["name"];
  JsonArray vols=doc["volumes"];
  JsonArray names=doc["names"];
  storedCocktails[cocktailCount].name=name;
  for(int i=0;i<6;i++){
    storedCocktails[cocktailCount].volumes[i]=vols[i];
    storedCocktails[cocktailCount].pumpNames[i]=names[i].as<String>();
  }
  cocktailCount++;
  saveCocktailsToFlash();
  server.send(200,"text/plain","OK");
}

void handleDispense(){
  for(int i=0;i<6;i++){
    if(server.hasArg("m"+String(i+1))){
      int v=server.arg("m"+String(i+1)).toInt();
      if(v>0) startPump(i,v); else stopPump(i);
    }
  }
  server.send(200,"text/plain","OK");
}

void handleDispenseCocktail(){
  if(!server.hasArg("name")){server.send(400);return;}
  String name=server.arg("name");
  for(int i=0;i<cocktailCount;i++){
    if(storedCocktails[i].name==name){
      for(int j=0;j<6;j++){
        int vol=storedCocktails[i].volumes[j];
        if(vol>0) startPump(j,vol);
      }
      server.send(200,"text/plain","OK");return;
    }
  }
  server.send(404,"text/plain","Cocktail not found");
}

void handleDeleteCocktail(){
  if(!server.hasArg("name")){server.send(400);return;}
  String name=server.arg("name");
  for(int i=0;i<cocktailCount;i++){
    if(storedCocktails[i].name==name){
      for(int j=i;j<cocktailCount-1;j++) storedCocktails[j]=storedCocktails[j+1];
      cocktailCount--;
      saveCocktailsToFlash();
      server.send(200,"text/plain","Deleted");return;
    }
  }
  server.send(404,"text/plain","Not found");
}

void handleCleanse(){
  unsigned long duration=5000;
  unsigned long endTime=millis()+duration;
  for(int i=0;i<6;i++){
    hardwareReversePump(i);
    pumpRunning[i]=true;
    pumpEndTime[i]=endTime;
  }
  server.send(200,"text/plain","Cleaning in progress");
}

// ----------------- Setup & Loop -----------------
void setupPins(){
  for(int i=0;i<12;i++){
    pinMode(inPins[i],OUTPUT);
    digitalWrite(inPins[i],LOW);
  }
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  setupPins();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Cocktail Machine");
  lcd.setCursor(0,1);
  lcd.print("Starting AP...");
  delay(3000);

  const char* ap_ssid = "CocktailMaker";
  const char* ap_pass = "mixdrink123";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid,ap_pass);
  Serial.println("AP started!");
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());

  lcd.clear();
  lcd.setCursor(0,0); lcd.print("AP Enabled");
  lcd.setCursor(0,1); lcd.print("SSID:"); lcd.print(ap_ssid);
  lcd.setCursor(0,2); lcd.print("PASS:"); lcd.print(ap_pass);
  lcd.setCursor(0,3); lcd.print("IP:"); lcd.print(WiFi.softAPIP().toString());

  startTime = millis();
  loadCocktailsFromFlash();

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/getCocktails", handleGetCocktails);
  server.on("/saveCocktail", HTTP_POST, handleSaveCocktail);
  server.on("/dispense", handleDispense);
  server.on("/dispenseCocktail", handleDispenseCocktail);
  server.on("/deleteCocktail", HTTP_POST, handleDeleteCocktail);
  server.on("/cleanse", handleCleanse);
  server.begin();
  Serial.println("Web server started");
}

void loop(){
  server.handleClient();
  unsigned long now=millis();
  for(int i=0;i<6;i++){
    if(pumpRunning[i] && now>=pumpEndTime[i]){
      stopPump(i);
    }
  }
}
