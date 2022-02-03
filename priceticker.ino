#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

String ssid;
String password;

using namespace websockets;
WebsocketsClient client;
DynamicJsonDocument data(512);
String prevPrice;

const int regLatch = 0;
const int regClock = 2;
const int regData = 3;

const int digits[10] = {126, 96, 61, 121, 99, 91, 95, 112, 127, 123};
int digit;

void setup() {
    Serial.begin(115200);
    EEPROM.begin(512);
    WIFIsetup();
    pinMode(regLatch, OUTPUT);
    pinMode(regClock, OUTPUT);
    pinMode(regData, OUTPUT);
    digitalWrite(regLatch, LOW);
    clear();
    digitalWrite(regLatch, HIGH);
    client.onMessage(recv);
    connect();
}

void WIFIsetup() {
    WiFi.mode(WIFI_STA);
    readWifiLogin();
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        setWifiLogin();
        ESP.restart();
    }
    ArduinoOTA.begin();
}

void readWifiLogin() {
    int pos = 0;
    ssid = readEEPROM(0, &pos);
    password = readEEPROM(pos, &pos);
}

void setWifiLogin() {
    Serial.setTimeout(60000);
    delay(1000);
    Serial.println("Set SSID: ");
    String SSID = Serial.readStringUntil('\n');
    Serial.println("Set PSK: ");
    String PSK = Serial.readStringUntil('\n');
    Serial.println("PSK: "+PSK+", SSID: "+SSID);
    writeEEPROM(SSID + '\n' + PSK + '\n');
}


void writeEEPROM(String str) {
    for ( int i=0; i < str.length() ; i++) {
        EEPROM.write(i, str[i]);
        delay(500);
    }
    if ( EEPROM.commit() ) {
        Serial.println("Wifi credentials saved");
    }
    else {
        Serial.println("Saving Wifi credentials failed");
    }
}

String readEEPROM(int addr, int *pos) {
    String str;
    char val;
    do {
        val = EEPROM.read(addr);
        str += val;
        addr++;
        delay(500);
    } while (val != '\n' && addr<64);
    *pos = addr;
    str.remove( str.length()-1 );
    return str;
}
    
void connect() {
    client.connect(F("wss://ws.bitstamp.net:443"));
    client.send(F("{\"event\": \"bts:subscribe\", \"data\": {\"channel\": \"live_trades_btcusd\"}}"));
}    

void loop() {
    ArduinoOTA.handle();
    if ( ((int)millis() / 1000)%10==0 && !client.ping() ) {
        connect();
    }
    if ( client.available() ) {
        client.poll();
    }
}

void recv(WebsocketsMessage msg) {
    String price = parseJson( msg.data() );
    showPrice(price);
}
    
String parseJson(String message) {
    deserializeJson(data, message);
    const char* price = data["data"]["price_str"];
    return parsePriceString(price);
}

String parsePriceString(String price) {
    int decimalIndex = price.lastIndexOf('.');
    price = price.substring(0, decimalIndex);
    return price;
}

void showPrice(String price) {
    if ( price != prevPrice ) {
        digitalWrite(regLatch, LOW);
        clear();
        pushData(price);
        digitalWrite(regLatch, HIGH);
        prevPrice = price;
    }
}

void pushData(String price) {
    int len = price.length();
    for (int i = 0; i < len; i++ ) {
        digit = digits[ (int)price[i]-48 ];
        display(digit);
    }
}

void display(int digit) {
    shiftOut(regData, regClock, MSBFIRST, digit);
}

void clear() {
    for (int i=0;i<8;i++) {
        display(0);
    }
}
