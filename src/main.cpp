#include <Arduino.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SoftwareSerial.h>
#include <PZEM004Tv30.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define DEBUG_MODE
#define SW1 D3

const char* ssid = "Ashera's-Net";
const char* password = "07190008";

AsyncWebServer server(80);
AsyncEventSource events("/events");

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

void handle_root(AsyncWebServerRequest *request);
void handle_api(AsyncWebServerRequest *request);

void setup() {
    start_sequence();

    pinMode(SW1, INPUT_PULLUP);

    wifi_setup();

    server.on("/", HTTP_GET, handle_root);
    server.on("/api", HTTP_GET, handle_api);

    events.onConnect([](AsyncEventSourceClient *client){

    #ifdef DEBUG
        Serial.println("Client connected to event stream");
    #endif

    });

    server.addHandler(&events);

    server.begin();

    #ifdef DEBUG_MODE
        Serial.println("Webserver started");
    #endif
}

void loop() {
    static uint32_t lastStream = 0;

    if(millis() - lastStream >= 1000){

        lastStream = millis();

        stream_data();
    }

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

String uptime_string(){

    uint32_t sec = millis() / 1000;

    uint16_t days = sec / 86400;
    sec %= 86400;

    uint8_t hours = sec / 3600;
    sec %= 3600;

    uint8_t mins = sec / 60;
    sec %= 60;

    char buf[32];

    snprintf(buf,
             sizeof(buf),
             "%ud %02uh %02um %02us",
             days,
             hours,
             mins,
             sec);

    return String(buf);
}

void stream_data(){

    String json = "{";

    json += "\"voltage\":" + String(voltage,1) + ",";
    json += "\"current\":" + String(current,2) + ",";
    json += "\"power\":" + String(active_power,0) + ",";
    json += "\"energy\":" + String(energy_consumption,3) + ",";
    json += "\"frequency\":" + String(frequency,1) + ",";
    json += "\"pf\":" + String(power_factor,2) + ",";
    json += "\"cost\":" + String(estimated_cost,0) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"uptime\":\"" + uptime_string() + "\"";

    json += "}";

    events.send(json.c_str(), "update", millis());
}

void handle_root(AsyncWebServerRequest *request){

    String html = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>IoT Power Monitor</title>

<style>

body{
    background:#111;
    color:white;
    font-family:Arial;
    margin:0;
    padding:20px;
}

h1{
    text-align:center;
}

.grid{
    display:grid;
    grid-template-columns:repeat(auto-fit,minmax(150px,1fr));
    gap:10px;
}

.card{
    background:#1c1c1c;
    border-radius:12px;
    padding:15px;
}

.label{
    font-size:14px;
    opacity:0.7;
}

.value{
    font-size:28px;
    margin-top:10px;
}

.footer{
    margin-top:20px;
    opacity:0.7;
    font-size:14px;
}

</style>

</head>

<body>

<h1>IoT Power Monitor</h1>

<div class="grid">

<div class="card">
<div class="label">Voltage</div>
<div class="value" id="voltage">0</div>
</div>

<div class="card">
<div class="label">Current</div>
<div class="value" id="current">0</div>
</div>

<div class="card">
<div class="label">Power</div>
<div class="value" id="power">0</div>
</div>

<div class="card">
<div class="label">Energy</div>
<div class="value" id="energy">0</div>
</div>

<div class="card">
<div class="label">Frequency</div>
<div class="value" id="frequency">0</div>
</div>

<div class="card">
<div class="label">Power Factor</div>
<div class="value" id="pf">0</div>
</div>

<div class="card">
<div class="label">Estimated Bill</div>
<div class="value" id="cost">0</div>
</div>

<div class="card">
<div class="label">WiFi RSSI</div>
<div class="value" id="rssi">0</div>
</div>

</div>

<div class="footer">

<div>IP: <span id="ip"></span></div>
<div>Uptime: <span id="uptime"></span></div>

</div>

<script>

if (!!window.EventSource) {

    var source = new EventSource('/events');

    source.addEventListener('update', function(e) {

        var data = JSON.parse(e.data);

        document.getElementById("voltage").innerHTML =
            data.voltage + " V";

        document.getElementById("current").innerHTML =
            data.current + " A";

        document.getElementById("power").innerHTML =
            data.power + " W";

        document.getElementById("energy").innerHTML =
            data.energy + " kWh";

        document.getElementById("frequency").innerHTML =
            data.frequency + " Hz";

        document.getElementById("pf").innerHTML =
            data.pf;

        document.getElementById("cost").innerHTML =
            "Rp " + data.cost;

        document.getElementById("rssi").innerHTML =
            data.rssi + " dBm";

        document.getElementById("ip").innerHTML =
            data.ip;

        document.getElementById("uptime").innerHTML =
            data.uptime;

    });

}

</script>

</body>
</html>

)rawliteral";

    request->send(200, "text/html", html);
}

void handle_api(AsyncWebServerRequest *request){

    String json = "{";

    json += "\"voltage\":" + String(voltage) + ",";
    json += "\"current\":" + String(current) + ",";
    json += "\"power\":" + String(active_power) + ",";
    json += "\"energy\":" + String(energy_consumption) + ",";
    json += "\"frequency\":" + String(frequency) + ",";
    json += "\"pf\":" + String(power_factor);

    json += "}";

    request->send(200, "application/json", json);
}