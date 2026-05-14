#include <Arduino.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <PZEM004Tv30.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define DEBUG_MODE
#define SW1 D3

const char* ssid = "Ashera's-Net";
const char* password = "07190008";

ESP8266WebServer server(80);

SoftwareSerial pzemSerial(D5, D6); // RX, TX
PZEM004Tv30 pzem(pzemSerial);

float electricity_rate = 1444.70; // IDR per kWh
float estimated_cost = 0;

//PZEM-004T READINGS
float voltage = 0;
float current = 0;
float active_power = 0;
float energy_consumption = 0;
float frequency = 0;
float power_factor = 0;

unsigned long lastSensorRead = 0;

static const unsigned char monsterchip_logo_bits[] = {0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x70,0x00,0x00,0x00,0x78,0x00,0x00,0x00,0x7c,0x00,0x00,0x00,0xfe,0x00,0x00,0x00,0xff,0x00,0x00,0x80,0xff,0x00,0x00,0x80,0xff,0x00,0x00,0xc0,0xff,0x01,0x00,0xa0,0xff,0x01,0x00,0xb0,0xff,0x01,0x00,0xb8,0xf7,0x03,0x00,0x7c,0xf3,0x03,0x00,0x7c,0xf1,0x0f,0x00,0x7e,0xe0,0xff,0x00,0x7e,0xe0,0xff,0x1f,0x7e,0xe0,0xff,0xff,0x3e,0xc0,0xff,0x7f,0x3f,0x00,0xff,0x3f,0x3f,0x00,0xe0,0x3f,0x3f,0x00,0xe0,0x1f,0x3e,0x00,0xf0,0x0f,0x7e,0x00,0xf8,0x07,0xfe,0x00,0xfc,0x03,0xfc,0x01,0xf0,0x01,0xfc,0x83,0x0f,0x00,0xf8,0xff,0x7f,0x00,0xf8,0xff,0x3f,0x00,0xf0,0xff,0x1f,0x00,0xc0,0xff,0x07,0x00,0x00,0xff,0x01,0x00};

void start_sequence(void);
void wifi_setup(void);
void read_sensor(void);
void draw_display(void);
void system_debug(void);
void stream_data(void);

void handleRoot();
void handleData();

void setup() {
    start_sequence();
}

void loop() {
    server.handleClient();

    if (millis() - lastSensorRead >= 1000) {

        lastSensorRead = millis();

        #ifdef DEBUG_MODE
            system_debug();
        #else
            read_sensor();
        #endif
    }

    draw_display();
}

void start_sequence(void){
    #ifdef DEBUG_MODE
        Serial.begin(115200);
    #endif

    pinMode(SW1, INPUT_PULLUP);

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, 0, 128, 63);

    u8g2.drawXBM(14, 16, 32, 32, monsterchip_logo_bits);

    u8g2.setFont(u8g2_font_profont29_tr);
    u8g2.drawStr(60, 37, "IoT");

    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(51, 47, "POWER MONITOR");

    u8g2.sendBuffer();

    delay(2500);

    wifi_setup();
}

void wifi_setup(){
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, password);

    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_profont17_tr);
    u8g2.drawStr(6, 29, "CONNECTING TO");
    u8g2.drawStr(33, 45, "WiFi...");
    u8g2.drawLine(0, 31, 127, 31);

    u8g2.sendBuffer();

    #ifdef DEBUG_MODE
        Serial.print("Connecting");
    #endif

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);

        #ifdef DEBUG_MODE
            Serial.print(".");
        #endif
    }

    #ifdef DEBUG_MODE
        Serial.println("");
        Serial.println("Connected!");
        Serial.println(WiFi.localIP());
    #endif

    server.on("/", handleRoot);
    server.on("/data", handleData);

    server.begin();

    String ip = WiFi.localIP().toString();

    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_profont17_tr);
    u8g2.drawStr(24, 20, "CONNECTED");
    u8g2.drawLine(0, 24, 127, 24);
    u8g2.setFont(u8g2_font_profont17_tr);
    u8g2.drawStr(3, 40, "IP ADDRESS:");
    u8g2.drawStr(3, 55, ip.c_str());

    u8g2.sendBuffer();

    delay(2000);
}

void read_sensor(){
    float v = pzem.voltage();
    float i = pzem.current();
    float p = pzem.power();
    float e = pzem.energy();
    float f = pzem.frequency();
    float pf = pzem.pf();

    if (!isnan(v)) voltage = v;
    if (!isnan(i)) current = i;
    if (!isnan(p)) active_power = p;
    if (!isnan(e)) energy_consumption = e;
    if (!isnan(f)) frequency = f;
    if (!isnan(pf)) power_factor = pf;

    estimated_cost = energy_consumption * electricity_rate;

    #ifdef DEBUG_MODE
        Serial.println("==========");

        Serial.print("Voltage: ");
        Serial.println(voltage);

        Serial.print("Current: ");
        Serial.println(current);

        Serial.print("Power: ");
        Serial.println(active_power);

        Serial.print("Energy: ");
        Serial.println(energy_consumption);

        Serial.print("Frequency: ");
        Serial.println(frequency);

        Serial.print("PF: ");
        Serial.println(power_factor);
    #endif
}

void format_voltage(char *buf, float value){

    // Max 4 chars
    // Example:
    // 220V
    // 12V

    snprintf(buf, 8, "%.0fV", value);
}

void format_current(char *buf, float value){

    // Max 4 chars
    // Example:
    // 1.2A
    // 12A
    // 99A

    if(value < 10)
        snprintf(buf, 8, "%.1fA", value);
    else
        snprintf(buf, 8, "%.0fA", value);
}

void format_power(char *buf, float value){

    // Max 4 chars
    // Example:
    // 12W
    // 999W
    // 1kW
    // 2kW

    if(value < 1000){
        snprintf(buf, 8, "%.0fW", value);
    }else{
        snprintf(buf, 8, "%.0fk", value / 1000.0);
    }
}

void format_frequency(char *buf, float value){

    // Max 4 chars
    // Example:
    // 50Hz
    // 60Hz

    snprintf(buf, 8, "%.0fHz", value);
}

void format_energy(char *buf, float value){

    // Max 7 chars
    // Example:
    // 999Wh
    // 1.2kWh
    // 12kWh

    if(value < 1.0){
        snprintf(buf, 16, "%.0fWh", value * 1000.0);
    }else if(value < 10.0){
        snprintf(buf, 16, "%.1fkWh", value);
    }else{
        snprintf(buf, 16, "%.0fkWh", value);
    }
}

void format_pf(char *buf, float value){

    // Example:
    // 0.95

    snprintf(buf, 8, "%.2f", value);
}

void draw_display(){
    char buffer[32];

    u8g2.clearBuffer();

    if(digitalRead(SW1) == 0){
        u8g2.setFont(u8g2_font_profont17_tr);

        String ip = WiFi.localIP().toString();

        u8g2.drawStr(3, 16, "DEVICE IP:");
        u8g2.drawStr(3, 36, ip.c_str());
    }else{
        u8g2.setFont(u8g2_font_profont17_tr);
        u8g2.drawLine(59, 0, 59, 63);

        u8g2.drawStr(3, 16, "V:");
        u8g2.drawStr(3, 30, "I:");
        u8g2.drawStr(3, 44, "P:");
        u8g2.drawStr(3, 58, "F:");
        u8g2.drawStr(63, 16, "ENERGY:");
        u8g2.drawStr(63, 51, "PF:");

        format_voltage(buffer, voltage);
        u8g2.drawStr(21, 16, buffer);

        format_current(buffer, current);
        u8g2.drawStr(21, 30, buffer);

        format_power(buffer, active_power);
        u8g2.drawStr(21, 44, buffer);

        format_frequency(buffer, frequency);
        u8g2.drawStr(21, 58, buffer);

        format_energy(buffer, energy_consumption);
        u8g2.drawStr(63, 30, buffer);

        format_pf(buffer, power_factor);
        u8g2.drawStr(89, 51, buffer);
    }

    u8g2.sendBuffer();
}

void system_debug(){
    static float t = 0;

    t += 0.1;

    voltage = 220.0 + (sin(t) * 5.0);

    current = 1.5 + (sin(t * 0.7) * 0.5);

    active_power = voltage * current * 0.92;

    energy_consumption += active_power / 3600000.0;

    frequency = 50.0 + (sin(t * 0.2) * 0.1);

    power_factor = 0.92 + (sin(t * 0.4) * 0.05);

    if(power_factor > 1.0)
        power_factor = 1.0;

    if(power_factor < 0.0)
        power_factor = 0.0;

    estimated_cost = energy_consumption * electricity_rate;
}

void handleRoot() {

    String html = R"rawliteral(

<!DOCTYPE html>
<html>
<head>

<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>IoT Power Monitor</title>

<style>

body{
    background:#0f172a;
    color:white;
    font-family:Arial;
    text-align:center;
    margin:0;
    padding:20px;
}

h1{
    margin-bottom:30px;
}

.container{
    display:flex;
    flex-wrap:wrap;
    justify-content:center;
    gap:20px;
}

.card{
    background:#1e293b;
    width:250px;
    padding:20px;
    border-radius:20px;
    box-shadow:0 0 20px rgba(0,0,0,0.3);
}

.label{
    font-size:18px;
    opacity:0.8;
}

.value{
    font-size:42px;
    margin-top:10px;
    font-weight:bold;
}

.unit{
    font-size:18px;
}

.footer{
    margin-top:40px;
    opacity:0.6;
}

</style>
</head>

<body>

<h1>⚡ IoT Power Monitor</h1>

<div class="container">

    <div class="card">
        <div class="label">Voltage</div>
        <div class="value">
            <span id="voltage">0</span>
            <span class="unit">V</span>
        </div>
    </div>

    <div class="card">
        <div class="label">Current</div>
        <div class="value">
            <span id="current">0</span>
            <span class="unit">A</span>
        </div>
    </div>

    <div class="card">
        <div class="label">Power</div>
        <div class="value">
            <span id="power">0</span>
            <span class="unit">W</span>
        </div>
    </div>

    <div class="card">
        <div class="label">Energy</div>
        <div class="value">
            <span id="energy">0</span>
            <span class="unit">kWh</span>
        </div>
    </div>

    <div class="card">
        <div class="label">Frequency</div>
        <div class="value">
            <span id="frequency">0</span>
            <span class="unit">Hz</span>
        </div>
    </div>

    <div class="card">
        <div class="label">Power Factor</div>
        <div class="value">
            <span id="pf">0</span>
        </div>
    </div>

    <div class="card">
        <div class="label">Estimated Cost</div>
        <div class="value">
            Rp <span id="cost">0</span>
        </div>
    </div>

</div>

<div class="footer">
    Uptime: <span id="uptime">0</span>
</div>

<script>

function formatUptime(seconds){

    seconds = Number(seconds);

    const days = Math.floor(seconds / 86400);
    seconds %= 86400;

    const hours = Math.floor(seconds / 3600);
    seconds %= 3600;

    const minutes = Math.floor(seconds / 60);
    seconds %= 60;

    return `${days}d ${hours}h ${minutes}m ${seconds}s`;
}

async function updateData(){

    try{

        const response = await fetch('/data');
        const data = await response.json();

        document.getElementById('voltage').innerText =
            data.voltage;

        document.getElementById('current').innerText =
            data.current;

        document.getElementById('power').innerText =
            data.power;

        document.getElementById('energy').innerText =
            data.energy;

        document.getElementById('frequency').innerText =
            data.frequency;

        document.getElementById('pf').innerText =
            data.pf;

        document.getElementById('cost').innerText =
            data.cost;

        document.getElementById('uptime').innerText =
            formatUptime(Number(data.uptime));

    }catch(e){
        console.log(e);
    }
}

setInterval(updateData, 1000);

updateData();

</script>

</body>
</html>

)rawliteral";

    server.send(200, "text/html", html);
}

void handleData() {

    String json = "{";

    json += "\"voltage\":" + String(voltage, 1) + ",";
    json += "\"current\":" + String(current, 2) + ",";
    json += "\"power\":" + String(active_power, 1) + ",";
    json += "\"energy\":" + String(energy_consumption, 3) + ",";
    json += "\"frequency\":" + String(frequency, 1) + ",";
    json += "\"pf\":" + String(power_factor, 2) + ",";
    json += "\"cost\":" + String(estimated_cost, 0) + ",";
    json += "\"uptime\":" + String(millis() / 1000);

    json += "}";

    server.send(200, "application/json", json);
}