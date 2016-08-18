#include <Arduino.h>
#include <stdlib.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include <FS.h>

// demo code for IOT class
// todo handle post method to set LED color
// Need to modify webcode

const char* ssid = "mySSID";
const char* password = "myPassword";

// How many leds are in the strip?
// for fast led ws2812
#define NUM_LEDS 1
#define DATA_PIN 2

CRGB leds[NUM_LEDS];  // Fastled object
String fname;
byte gRed;
byte gGreen;
byte gBlue;

ESP8266WebServer server(80);// server object

const int led = 14;  // NodeMCU onboard LED

void SetLedsOn()
{
	leds[0].setRGB(gRed, gGreen, gBlue);
	FastLED.show();
}

void SetLedsOff()
{
	leds[0] = CRGB::Black;
	FastLED.show();
}

void CycleColors()
{
	long CurMills;
	static uint8_t hue = 0;
	FastLED.showColor(CHSV(hue++, 255, 255));
	//delay(10);
}

// helper function to return the proper content type ID to client
String getContentType(String filename)
{
	if (server.hasArg("download")) return "application/octet-stream";
	else if (filename.endsWith(".htm")) return "text/html";
	else if (filename.endsWith(".html")) return "text/html";
	else if (filename.endsWith(".css")) return "text/css";
	else if (filename.endsWith(".js")) return "application/javascript";
	else if (filename.endsWith(".png")) return "image/png";
	else if (filename.endsWith(".gif")) return "image/gif";
	else if (filename.endsWith(".jpg")) return "image/jpeg";
	else if (filename.endsWith(".ico")) return "image/x-icon";
	else if (filename.endsWith(".xml")) return "text/xml";
	else if (filename.endsWith(".pdf")) return "application/x-pdf";
	else if (filename.endsWith(".zip")) return "application/x-zip";
	else if (filename.endsWith(".gz")) return "application/x-gzip";
	return "text/plain";
}

// browse SPI flash FS & send file to web client
bool SPIflashServer(String path)
{

	if (path.endsWith("/")) path += "index.html";
	bool retVal;
	String contentType = getContentType(path);
	String pathWithGz = path + ".gz";
	if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
		if (SPIFFS.exists(pathWithGz))
			path += ".gz";
		File webfile = SPIFFS.open(path, "r");
		server.streamFile(webfile, contentType);
		webfile.close();
		retVal = true;
	}
	else
		// give 404 if not found
		// html is't on ESP8266 flash
	{
		digitalWrite(led, 1);
		String message = "Error 404\n\n";
		message += "URI: ";
		message += server.uri();
		message += "\nMethod: ";
		message += (server.method() == HTTP_GET) ? "GET" : "POST";
		message += "\nArguments: ";
		message += server.args();
		message += "\n";
		for (uint8_t i = 0; i<server.args(); i++) {
			message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
		}
		server.send(404, "text/plain", message);
		digitalWrite(led, 0);
		retVal = false;
	}
	return retVal;
}

// Web request handlers follow

// default handler, open &  stream index.html
void handleRoot() {
	digitalWrite(led, 1);
	bool status = SPIflashServer("/");
	SetLedsOn();
	digitalWrite(led, 0);
}

// handler for anthing other the index.html
// also deals with get args
bool handleAll(String path) {
	bool retVal;
	// test code to let us know we hit this one
	Serial.println("handleFileRead: " + path);
	// if we have arguments when getting page .....
	if (server.method() == HTTP_GET && server.args() > 0)
	{
		Serial.println("Get");
		String parmList = "args are #";
		int parms = server.args();
		parmList += (char)parms + " : ";
		for (int i = 0; i < parms; i++)
		{
			parmList += server.argName(i) + " = " + server.arg(i) + " ' ";
			// arguments from get are type string, we need a uint8 for fastled
			if (server.argName(i) == "red")
			{
				String sRed = server.arg(i);
				gRed = (byte)atoi(sRed.c_str());
			}
			if (server.argName(i) == "green")
			{
				String sGreen = server.arg(i);
				gGreen = (byte)atoi(sGreen.c_str());
			}
			if (server.argName(i) == "blue")
			{
				String sBlue = server.arg(i);
				gBlue = (byte)atoi(sBlue.c_str());
			}
		}
		Serial.println(parmList);
		leds[0].setRGB(gRed, gGreen, gBlue);
		FastLED.show();

	}
	retVal = SPIflashServer(path);
	return retVal;
}


void setup(void) {
	pinMode(led, OUTPUT);
	digitalWrite(led, 0);
	Serial.begin(115200);
	gRed = 128;
	gBlue = 128;
	gGreen = 128;
	WiFi.begin(ssid, password);
	Serial.println("");

	FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);

	// open fs and show files in flash
	// test code for serial console
	bool filetest = SPIFFS.begin();
	if (filetest)
	{
		Dir dir = SPIFFS.openDir("/");
		while (dir.next())
		{
			Serial.print(dir.fileName());
			fname = dir.fileName();
			File f = dir.openFile("r");
			Serial.print("  -  ");
			Serial.println(f.size());
		}
		Serial.println("check fileloop");  //test code for dirs
	}
	else
	{
		Serial.println("filesystem not open");
	}

	// Wait for connection
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	// more console test code
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	// Maybe remove, not needed since mDNS never works in windows
	if (MDNS.begin("esp8266"))
	{
		Serial.println("MDNS responder started");
	}

	// server event handling, just pass to handler functions
	// if at root ie no page requested
	server.on("/", handleRoot);

	// test code to remove, inline handler
	server.on("/inline", []() {
		server.send(200, "text/plain", "this works as well");
		SetLedsOff();
	});

	//server.onNotFound(handleAll(server.uri())); // this doesn't work
	// not a clue why the code has to be this way, funky server libr !!!
	server.onNotFound([]() {
		if (!handleAll(server.uri()))
			server.send(404, "text/plain", "FileNotFound");
	});

	server.begin();
	Serial.println("HTTP server started");
}

// main loop for this is just the server listner
void loop(void)
{
	server.handleClient();
	//MDNS.update();
}
