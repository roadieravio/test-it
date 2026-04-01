#include <M5Unified.h>
#include "WiFi.h"
#include "esp_wifi.h"
#if __has_include("esp_wpa2.h")
#include "esp_wpa2.h"
#define HAS_WPA2_ENTERPRISE 1
#else
#define HAS_WPA2_ENTERPRISE 0
#endif
#include "WiFiClient.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"
#include "Update.h"

#define D_WIFI_SSID ""
#define D_WIFI_PASS ""
#define D_WIFI_USER ""
#define D_WIFI_IDENTITY D_WIFI_USER

#define FIRMWARE_VERSION "1.0.0"
#define BITCOIN_PRICE_ENDPOINT "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd"
#define FIRMWARE_UPDATE_ENDPOINT "http://your-server.com/firmware/check"
#define FIRMWARE_BINARY_ENDPOINT "http://your-server.com/firmware/bin"



static uint32_t g_bitcoinPriceUsd = 0;
static uint32_t g_lastBitcoinCheckMs = 0;
static const uint32_t BITCOIN_CHECK_INTERVAL_MS = 60000;

static uint32_t g_lastFirmwareCheckMs = 0;
static const uint32_t FIRMWARE_CHECK_INTERVAL_MS = 300000;
static char g_latestFirmwareVersion[32] = FIRMWARE_VERSION;

static void performFirmwareUpdate();


static void fetchBitcoinPrice() {
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("Bitcoin fetch: WiFi not connected");
		return;
	}

	WiFiClientSecure client;
	client.setInsecure();
	
	HTTPClient http;
	http.setTimeout(5000);
	
	if (!http.begin(client, BITCOIN_PRICE_ENDPOINT)) {
		Serial.println("Bitcoin fetch: Failed to begin HTTP");
		return;
	}
	
	int httpCode = http.GET();
	
	if (httpCode != HTTP_CODE_OK) {
		Serial.printf("Bitcoin fetch: HTTP error %d\n", httpCode);
		http.end();
		return;
	}
	
	String payload = http.getString();
	http.end();
	
	Serial.printf("Bitcoin response: %s\n", payload.c_str());
	
	int pos = payload.indexOf("\"usd\":");
	if (pos != -1) {
		pos += 6;
		int endPos = payload.indexOf('}', pos);
		if (endPos == -1) endPos = payload.indexOf(',', pos);
		if (endPos == -1) endPos = payload.length();
		
		String priceStr = payload.substring(pos, endPos);
		priceStr.trim();
		
		float price = strtof(priceStr.c_str(), nullptr);
		g_bitcoinPriceUsd = (uint32_t)price;
		Serial.printf("Bitcoin price parsed: $%lu USD\n", (unsigned long)g_bitcoinPriceUsd);
	} else {
		Serial.println("Bitcoin: Could not find \"usd\" in response");
	}
}

static void checkFirmwareUpdate() {
	if (WiFi.status() != WL_CONNECTED) {
		return;
	}

	HTTPClient http;
	http.setTimeout(5000);
	
	if (http.begin(FIRMWARE_UPDATE_ENDPOINT)) {
		int httpCode = http.GET();
		if (httpCode == HTTP_CODE_OK) {
			String payload = http.getString();
			
			int pos = payload.indexOf("\"version\":");
			if (pos != -1) {
				pos += 10;
				int endPos = payload.indexOf('"', pos);
				String newVersion = payload.substring(pos + 1, endPos);
				
				strncpy(g_latestFirmwareVersion, newVersion.c_str(), sizeof(g_latestFirmwareVersion) - 1);
				
				if (newVersion != FIRMWARE_VERSION) {
					Serial.printf("New firmware available: %s (current: %s)\n",
						newVersion.c_str(), FIRMWARE_VERSION);
					performFirmwareUpdate();
				}
			}
		}
		http.end();
	}
}

static void performFirmwareUpdate() {
	Serial.println("Starting firmware update...");
	
	WiFiClient client;
	HTTPClient http;
	
	if (!http.begin(client, FIRMWARE_BINARY_ENDPOINT)) {
		Serial.println("Failed to connect to firmware endpoint");
		return;
	}
	
	int httpCode = http.GET();
	if (httpCode != HTTP_CODE_OK) {
		Serial.printf("Failed to download firmware: HTTP %d\n", httpCode);
		http.end();
		return;
	}
	
	int contentLength = http.getSize();
	if (contentLength <= 0) {
		Serial.println("Invalid content length");
		http.end();
		return;
	}
	
	if (!Update.begin(contentLength)) {
		Serial.println("Not enough space to begin OTA");
		http.end();
		return;
	}
	
	WiFiClient * stream = http.getStreamPtr();
	size_t written = Update.writeStream(*stream);
	
	if (written != contentLength) {
		Serial.printf("Written only %zu / %d bytes\n", written, contentLength);
		Update.abort();
		http.end();
		return;
	}
	
	if (!Update.end()) {
		Serial.printf("OTA Failed: %s\n", Update.errorString());
		return;
	}
	
	if (!Update.isFinished()) {
		Serial.println("OTA did not complete successfully");
		return;
	}
	
	Serial.println("OTA Update completed successfully. Rebooting...");
	http.end();
	delay(1000);
	ESP.restart();
}



static void connectToWifi() {
	WiFi.disconnect(true);
	WiFi.mode(WIFI_STA);

#if HAS_WPA2_ENTERPRISE
	esp_wifi_sta_wpa2_ent_set_identity(reinterpret_cast<const uint8_t *>(D_WIFI_IDENTITY),
										 strlen(D_WIFI_IDENTITY));
	esp_wifi_sta_wpa2_ent_set_username(reinterpret_cast<const uint8_t *>(D_WIFI_USER),
										 strlen(D_WIFI_USER));
	esp_wifi_sta_wpa2_ent_set_password(reinterpret_cast<const uint8_t *>(D_WIFI_PASS),
										 strlen(D_WIFI_PASS));

	esp_wifi_sta_wpa2_ent_enable();
	WiFi.begin(D_WIFI_SSID);
#else
	WiFi.begin(D_WIFI_SSID, D_WIFI_PASS);
#endif
}

void setup() {

//M5stickC
M5.begin() ;
M5.Lcd.setRotation(3) ;

//WiFi
connectToWifi() ;
Serial.print("connecting") ;
delay(5000) ;
//
while(WiFi.status()!=WL_CONNECTED){
Serial.print(".") ;
delay(500) ;
}
Serial.println();

//接続が完了したらIPアドレスをシリアルモニタに表示
Serial.println("\nwifi Connected") ;
Serial.println(WiFi.localIP()) ;

//
M5.Lcd.setTextSize(2);
M5.Lcd.println("wifi OK") ;
M5.Lcd.println(WiFi.localIP()) ;
M5.Lcd.println("") ;



M5.Lcd.printf("FW: %s\n", FIRMWARE_VERSION);

fetchBitcoinPrice();
checkFirmwareUpdate();
}

void loop() {


const uint32_t now = millis();

	if ((now - g_lastBitcoinCheckMs) >= BITCOIN_CHECK_INTERVAL_MS) {
		g_lastBitcoinCheckMs = now;
		fetchBitcoinPrice();
		M5.Lcd.setCursor(0, 100);
		M5.Lcd.printf("BTC: $%-10lu\n", (unsigned long)g_bitcoinPriceUsd);
	}

	if ((now - g_lastFirmwareCheckMs) >= FIRMWARE_CHECK_INTERVAL_MS) {
		g_lastFirmwareCheckMs = now;
		checkFirmwareUpdate();
	}

	delay(10);

}