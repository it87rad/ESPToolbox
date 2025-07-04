#include <Arduino.h>

#include "uEspConfigLib.h"
#include "uEspConfigLibFSLittlefs.h"
#include "uTimerLib.h"
#include <Wire.h>
#include "uRTCLib.h"

// ==================== start of TUNEABLE PARAMETERS ====================

#define SERIAL_BAUDS 115200
bool serial_debug = false; // This only affects to start, when loading config it can be changed as will



// ==================== end of TUNEABLE PARAMETERS ====================

uEspConfigLibFSInterface * configFs;
uEspConfigLib * config;
volatile uint8_t doReset = 0;

char currentWifiMode = '-';
byte clientNameLen = 0;

volatile int blinkPin = -1;
volatile bool blinking = false;
volatile bool blink_status = 0;

String scanResults;
volatile bool scanStart = false;
volatile bool scanning = false;

#ifdef ARDUINO_ARCH_ESP32
    #include <HTTPUpdateServer.h>
    WebServer server(80);
    HTTPUpdateServer httpUpdater;
#else
    #include <ESP8266HTTPUpdateServer.h>
    ESP8266WebServer server(80);
    ESP8266HTTPUpdateServer httpUpdater;
#endif

WiFiClient espClient;

uRTCLib rtc;


// =======================================================
// ==================== FUNCTIONALITY ====================
// =======================================================


// ==================== Function definitions ====================
void handleDefault();
void handleConfig();
void handleSaveConfig();
void handleBlink();
void handleBlinkoff();
void handleI2cscan();
void handleWifiScan();
void handleWifiScanResult();
void handleWifiScanResult();
void handleRTCSet();
void handleRTCGet();

void setup_web(void);
void setup_wifi();
void setup_config();
void setup_timer();
void setup();
void loop();
void blink_timed_function();
void doWifiScan();

// ==================== Debug functionality ====================
void uDebugLibInit() {
    if (serial_debug) {
        Serial.begin(SERIAL_BAUDS);
        while (!Serial) ; // wait for serial port to connect. Needed for native USB
    }
}
#define DEBUG_PRINT(x) { if (serial_debug) Serial.print(x); }
#define DEBUG_PRINTLN(x) { if (serial_debug) Serial.println(x); }



// ==================== Blink functionality (also on loop) ====================
void blink_timed_function() {
	blink_status = !blink_status;
}



void handleBlink() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text", "");
    yield();
    bool error = true;
    if (server.hasArg("pin")) {
        if (server.arg("pin") != "") {
            blinkPin = atoi(server.arg("pin").c_str());
            pinMode(blinkPin, OUTPUT);
            blinking = true;
            server.sendContent("Blink started on pin " + server.arg("pin") + "\n");
            error = false;
        }
    }
    if (error) {
        server.sendContent("ERROR: blink pin not specified\n");
    }
}
void handleBlinkoff() {
    blinkPin = -1;
    blinking = false;
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text", "");
    yield();
    server.sendContent("Blink stopped\n");
}


// ==================== WiFi scanner ====================
void doWifiScan() {
    scanResults.clear();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    if (n == 0) {
        scanResults = "WiFi scanner: no networks found\n";
    } else {
        scanResults = String(n, 10) + " networks found:\n";
        for (int i = 0; i < n; i++) {
            scanResults += " - " + WiFi.SSID(i) + " - Channel: " + WiFi.channel(i) + " - RSSI: " + WiFi.RSSI(i) + " - Encription: ";
            switch (WiFi.encryptionType(i)) {
                case ENC_TYPE_WEP: scanResults += "WEP"; break;
                case ENC_TYPE_TKIP: scanResults += "WPA/PSK"; break;
                case ENC_TYPE_CCMP: scanResults += "WPA2/PSK"; break;
                case ENC_TYPE_NONE: scanResults += "NONE"; break;
                case ENC_TYPE_AUTO: scanResults += "AUTO (WPA/WPA2/PSK)"; break;
                default: scanResults += "Unknown"; break;
            }
            yield();
            scanResults += "\n";
        }
    }
    setup_wifi();
    setup_web();
    scanning = false;
}

void handleWifiScanResult() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text", "");
    yield();
    if (scanning)  {
        server.sendContent("...scanning WiFi...");
    } else {
        server.sendContent(scanResults);
    }


}

void handleWifiScan() {
    scanning = true;
    scanStart = true;
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text", "");
    yield();
    server.sendContent("WiFi scan started, waiting for results\n");
}

// ==================== I2C scanner ====================

void handleI2cscan() {
    bool errorCall = true;
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text", "");
    yield();

    if (server.hasArg("sda") && server.hasArg("scl")) {
        if (server.arg("sda") != "" && server.arg("scl") != "") {
            errorCall = false;
            blinkPin = -1;
            blinking = false;
            server.sendContent("Blink stopped.\n");
            Wire.begin(atoi(server.arg("sda").c_str()), atoi(server.arg("scl").c_str()));
            byte error, address;
            int nDevices = 0;
            server.sendContent("Scanning I2C bus using sda=" + server.arg("sda") + " and scl=" + server.arg("scl") + "...\n");
            yield();

            for(address = 1; address < 127; address++ ) {
                // The i2c_scanner uses the return value of
                // the Write.endTransmisstion to see if
                // a device did acknowledge to the address.
                Wire.beginTransmission(address);
                error = Wire.endTransmission();
                yield();
                if (error == 0) {
                    server.sendContent("I2C device found at address 0x");
                    server.sendContent(String(address, 16));
                    server.sendContent("!\n");
                    nDevices++;
                } else if (error==4) {
                    server.sendContent("Unknown error at address 0x");
                    server.sendContent(String(address, 16));
                    server.sendContent("\n");
                }    
            }
            if (nDevices == 0) {
                server.sendContent("No I2C devices found\n");
            }
            server.sendContent("\n-done-\n");
        }
    }
    if (errorCall) {
        server.sendContent("ERROR: sda and/or scl pins not specified\n");
    }

}


// ==================== RTC ====================

void handleRTCSet() {
    bool errorCall = true;
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text", "");
    yield();

    if (server.hasArg("sda") && server.hasArg("scl") && server.hasArg("dy") && server.hasArg("dm") && server.hasArg("dd") && server.hasArg("dw") && server.hasArg("th") && server.hasArg("tm") && server.hasArg("ts") && server.hasArg("ad")) {
        if (server.arg("sda") != "" && server.arg("scl") != "" && server.arg("dy") != "" && server.arg("dm") != "" && server.arg("dd") != "" && server.arg("dw") != "" && server.arg("th") != "" && server.arg("tm") != "" && server.arg("ts") != "" && server.arg("ad") != "") {
            errorCall = false;
            blinkPin = -1;
            blinking = false;
            server.sendContent("Blink stopped.\n");
            server.sendContent("RTC - Claring flags and setting date and time.\n");
            Wire.begin(atoi(server.arg("sda").c_str()), atoi(server.arg("scl").c_str()));
            rtc.set_rtc_address(strtol(server.arg("ad").c_str(), NULL, 0));
            rtc.set_model(server.arg("md") == "1" ? URTCLIB_MODEL_DS1307 : URTCLIB_MODEL_DS3231);
            rtc.refresh();
            rtc.set_12hour_mode(false);
            server.sendContent(" - 24h mode set.\n");
            rtc.lostPowerClear();
            server.sendContent(" - LostPower flag cleared.\n");
            rtc.enableBattery();
            server.sendContent(" - Battery enabled.\n");
            rtc.set(atoi(server.arg("ts").c_str()), atoi(server.arg("tm").c_str()), atoi(server.arg("th").c_str()), atoi(server.arg("dw").c_str()), atoi(server.arg("dd").c_str()), atoi(server.arg("dm").c_str()), atoi(server.arg("dy").c_str()));
            server.sendContent(" - RTC set.\n");
        }
    }
    if (errorCall) {
        server.sendContent("ERROR: needed parameters not specified\n");
    }
}


void handleRTCGet() {
    bool errorCall = true;
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text", "");
    yield();

    if (server.hasArg("sda") && server.hasArg("scl") && server.hasArg("ad")) {
        if (server.arg("sda") != "" && server.arg("scl") != "" && server.arg("ad") != "") {
            errorCall = false;
            blinkPin = -1;
            blinking = false;
            server.sendContent("Blink stopped.\n");
            Wire.begin(atoi(server.arg("sda").c_str()), atoi(server.arg("scl").c_str()));
            rtc.set_rtc_address(strtol(server.arg("ad").c_str(), NULL, 0));
            rtc.set_model(server.arg("md") == "1" ? URTCLIB_MODEL_DS1307 : URTCLIB_MODEL_DS3231);
            rtc.refresh();

            // We use global object, refreshed each second in loop function: rtc.refresh();
            server.sendContent("RTC data:\n - Date (y/m/d): " + String(rtc.year(), DEC) + "/" + String(rtc.month(), DEC) + "/" + String(rtc.day(), DEC) + "\n");
            yield();

            server.sendContent(" - Time (h:m:s): " + String(rtc.hour(), DEC) + ":" + String(rtc.minute(), DEC) + ":" + String(rtc.second(), DEC) + "\n");
            yield();

            server.sendContent(" - DayOfWeeK (1=Sunday): " + String(rtc.dayOfWeek(), DEC) + "\n");
            yield();

            server.sendContent(" - Mode: " + String(rtc.hourModeAndAmPm() == 0 ? "24h" : "12h"));
            if (rtc.hourModeAndAmPm() == 1) {
                server.sendContent(" (am)");
            }
            if (rtc.hourModeAndAmPm() == 2) {
                server.sendContent(" (pm)");
            }
            server.sendContent("\n");
            yield();

            server.sendContent(" - Temperature: " + String(((float) rtc.temp() / 100), 2) + "\n");
            yield();

            if (rtc.getEOSCFlag()) {
                server.sendContent(" - Oscillator will not use VBAT when VCC cuts off. Time will not increment without VCC!\n");
            } else {
                server.sendContent(" - Oscillator will use VBAT when VCC cuts off.\n");
            }
            yield();

            if (rtc.lostPower()) {
                server.sendContent(" - Lost power status: POWER FAILED.\n");
            } else {
                server.sendContent(" - Lost power status: POWER OK.\n");
            }
            yield();

            server.sendContent(" - Aging register value: " + String(rtc.agingGet(), DEC) + "\n");
            yield();
        }
    }
    if (errorCall) {
        server.sendContent("ERROR: sda, scl and/or address not specified\n");
    }
}



// ==================== ESP web and update ====================
void handleDefault() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    yield();
    server.sendContent("<html><head><title>ESPToolbox</title>");
    yield();
    server.sendContent("<script>\nfunction out(t) {var el=document.getElementById('app-output');el.innerText=(t+\"\\n----------\\n\"+el.innerText);};\nfunction get(uri,cb) {var xhr=new XMLHttpRequest();xhr.onreadystatechange=function(){if(xhr.readyState==4){if (xhr.status>=200 && xhr.status<300){cb(xhr.response+\"\\n\");}else{cb(\"ERROR ON LAST REQUEST\\n\");}}};xhr.responseType='text';xhr.timeout=1000;xhr.open(\"GET\", uri, true);xhr.send();}\nfunction wifiP(t) { var text = t.substring(0, 18);if (text == \"WiFi scan started,\" || text == \"...scanning WiFi..\" || text == \"ERROR ON LAST REQU\") { out('WiFi scanner in progress...'); window.setTimeout(function(){get('/wifiscanresult', wifiP);},1000);}else{out(t);}};\n</script>");
    yield();
    server.sendContent("</head><body><h2>Toolbox for ESP8266 and ESP32 microcontrollers.</h2>");
    yield();
    if (doReset == 1 && server.hasArg("saved") && server.arg("saved") == "1") {
        server.sendContent("<p><b>Configuration saved, resetting device</b></p>");
        doReset = 2;    
    }
    server.sendContent("<br><a href=\"#\" onclick=\"get('/wifiscan', wifiP);\">Scan WiFis</a><br><hr>");
    yield();
    server.sendContent("Pin (default 2): <input type=\"number\" id=\"app-pin\" value=\"2\"> <a href=\"#\" onclick=\"get('/blink?pin=' + document.getElementById('app-pin').value, out);\">Blink this pin (stops last pin blinking)</a> - <a href=\"#\" onclick=\"get('/blinkoff', out);\">Stop pin blinking</a><br><hr>");
    yield();
    server.sendContent("sda pin (default 4): <input type=\"number\" id=\"app-sda\" value=\"4\"> | scl pin (default 5): <input type=\"number\" id=\"app-scl\" value=\"5\"> <a href=\"#\" onclick=\"get('/i2cscan?sda=' + document.getElementById('app-sda').value + '&scl=' + document.getElementById('app-scl').value, out);\">Perform I2C scan</a><br><hr>");
    yield();

    server.sendContent("<p><b>RTC</b><br>");
    yield();
    server.sendContent("sda pin (default 4): <input type=\"number\" id=\"app-sdar\" value=\"4\"> | scl pin (default 5): <input type=\"number\" id=\"app-sclr\" value=\"5\"><br>");
    yield();
    server.sendContent("Day: <input type=\"number\" id=\"app-dayr\"> | Month: <input type=\"number\" id=\"app-monthr\"> | Year (2 digits): <input type=\"number\" id=\"app-yearr\"> | DayOfWeek (0-Sunday, 6-Saturday): <input type=\"number\" id=\"app-dowr\"><br>");
    yield();
    server.sendContent("Hour: <input type=\"number\" id=\"app-hourr\"> | Minute: <input type=\"number\" id=\"app-minuter\"> | Second: <input type=\"number\" id=\"app-secondr\"><br>");
    yield();
    server.sendContent("RTC is DS1307? Write 1 here: <input type=\"number\" id=\"app-modelr\"> | RTC I2C address (default 0x68): <input type=\"text\" id=\"app-addrr\" value=\"0x68\"><br>");
    yield();
    server.sendContent("<a href=\"#\" onclick=\"get('/rtcget?sda=' + document.getElementById('app-sdar').value + '&scl=' + document.getElementById('app-sclr').value + '&ad=' + document.getElementById('app-addrr').value + '&md=' + document.getElementById('app-modelr').value, out);\">GetRTC data</a> - <a href=\"#\" onclick=\"get('/rtcset?sda=' + document.getElementById('app-sdar').value + '&scl=' + document.getElementById('app-sclr').value + '&dy=' + document.getElementById('app-yearr').value + '&dm=' + document.getElementById('app-monthr').value + '&dd=' + document.getElementById('app-dayr').value + '&dw=' + document.getElementById('app-dowr').value + '&th=' + document.getElementById('app-hourr').value + '&tm=' + document.getElementById('app-minuter').value + '&ts=' + document.getElementById('app-secondr').value + '&ad=' + document.getElementById('app-addrr').value + '&md=' + document.getElementById('app-modelr').value, out);\">Clear flags and set</a><br><hr>");
    yield();
    server.sendContent("<p><a href=\"/config\">Configuration</a> - <a href=\"/update\">Upload update</a></p><hr>");
    yield();\
    server.sendContent("<p>Output:</p><pre id=\"app-output\" style=\"border:1px solid #666\"></pre><br><br><br>");
    yield();\
    server.sendContent("<p><b>Important note:</b> All input pins MUST be numeric. Dx, GPIOx are NOT allowed.");
    yield();\
    server.sendContent("<p>Check GitHub page for <a href=\"https://github.com/Naguissa/ESPToolbox\">info</a>, <a href=\"https://github.com/Naguissa/ESPToolbox/issues\">support</a> or <a href=\"https://github.com/Naguissa/ESPToolbox/releases\">updates</a>.</p></body></html>");
    yield();
}

void handleConfig() {
  config->handleConfigRequestHtml(&server, "/config");
}

void handleSaveConfig() {
    config->handleSaveConfig(&server);
    doReset = 1;
}


// ==================== Setup functions ====================


void setup_timer() {
    	TimerLib.setInterval_us(blink_timed_function, 500000);
}

void setup_web() {
    httpUpdater.setup(&server, "/update");
    server.on("/config", HTTP_POST, handleSaveConfig);
    server.on("/config", HTTP_GET, handleConfig);

    server.on("/blink", HTTP_GET, handleBlink);
    server.on("/blinkoff", HTTP_GET, handleBlinkoff);
    server.on("/i2cscan", HTTP_GET, handleI2cscan);

    server.on("/wifiscan", HTTP_GET, handleWifiScan);
    server.on("/wifiscanresult", HTTP_GET, handleWifiScanResult);

    server.on("/rtcget", HTTP_GET, handleRTCGet);
    server.on("/rtcset", HTTP_GET, handleRTCSet);

    server.onNotFound(handleDefault);
    yield();
    server.begin();
    yield();
}

void setup_config() {
    configFs = new uEspConfigLibFSLittlefs("/ESPToolbox.ini", true);
    if (configFs->status() == uEspConfigLibFS_STATUS_FATAL) {
        DEBUG_PRINTLN("  * Error initializing LittleFS");
    }
    config = new uEspConfigLib(configFs);

    config->addOption("wifi_mode", "WiFi mode (C=Client, other=Access Point)", "");
    config->addOption("wifi_ssid", "WiFi SSID", "ESPToolbox");
    config->addOption("wifi_password", "WiFi password", "");
    config->addOption("serial_debug", "Enable serial debug (1=enabled, other=disabled)", "");

    config->loadConfigFile();
    // Set global pointers
    serial_debug = ( (* (char *) config->getPointer("serial_debug")) == '1');
    DEBUG_PRINT("\nSerial debug: ");
    DEBUG_PRINTLN(serial_debug ? 'Y' : 'N');
}


void setup_wifi() {
    char *mode, *ssid, *pass;
    
	server.stop();
	WiFi.disconnect();
    #ifndef ARDUINO_ARCH_ESP32
	    WiFi.setAutoConnect(true);
	#endif
	WiFi.setAutoReconnect(true);
	
    mode = config->getPointer("wifi_mode");
    ssid = config->getPointer("wifi_ssid");
    pass = config->getPointer("wifi_password");

    DEBUG_PRINT("Connecting to WiFi in '");
    DEBUG_PRINT(mode);
    DEBUG_PRINTLN("' mode.");
    DEBUG_PRINT("  - ssid: ");
    DEBUG_PRINTLN(ssid);
    DEBUG_PRINT("  - pass: ");
    DEBUG_PRINTLN(pass);

    if (strcmp(mode, "C") == 0) {
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssid, pass);

		// Wait for connection
		uint8_t i = 0;
		while (WiFi.status() != WL_CONNECTED && i++ < 30) { //wait 30*2 seconds
			Serial.print('.');
			delay(2000);
		}
		if (i == 31) {
	        DEBUG_PRINT("Could not connect to ");
			DEBUG_PRINTLN(ssid);
            return;
		}
        currentWifiMode = 'C';
        DEBUG_PRINT("Connected! IP address: ");
		DEBUG_PRINTLN(WiFi.localIP());
	} else { // Default mode, 'A' (AP)
        WiFi.mode(WIFI_AP);
        WiFi.softAP(ssid, pass);
        currentWifiMode = 'A';
        DEBUG_PRINT("SoftAP created! IP address: ");
        DEBUG_PRINTLN(WiFi.softAPIP());
	}
}


// This section of code runs only once at start-up.
void setup() {
    setup_config();
    uDebugLibInit();
    DEBUG_PRINTLN("setupConfig & debug OK");
    setup_wifi();
    DEBUG_PRINTLN("setup_wifi OK");
    setup_web();
    DEBUG_PRINTLN("setup_web OK");
    setup_timer();
    DEBUG_PRINTLN("setup_timer OK");
    DEBUG_PRINTLN("SETUP END");
}


// The repeating section of the code
void loop() {
    if(doReset == 2) {
        // Little delay to avoid cutting current connection
        for (int i = 0; i < 10; i++) {
            delay(100);
        }
        #ifdef ARDUINO_ARCH_ESP32
            ESP.restart();
        #else
            ESP.reset();
        #endif
    }
    if (blinking) {
        digitalWrite(blinkPin, blink_status);
    }

    if (scanStart) {
        scanStart = false;
        doWifiScan();
    }

    server.handleClient();
    yield();
}
