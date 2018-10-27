#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <base64.h> // for http basic auth
#include <FS.h>
#include <SPIFFS.h>
#include <esp8266-google-home-notifier.h>

const char *ssid = "<YOUR-WIFI-SSID>";
const char *password = "<YOUR-WIFI-PASSWORD>";

// VoiceText Web API
const String tts_url = "https://api.voicetext.jp/v1/tts";
const String tts_user = "<VOICETEXT-API-KEY>"; // 取得した VoiceText Web API のキー
const String tts_pass = "";   // ここは空で良い
const String mp3file = "test.mp3";
String tts_parms = "&speaker=hikari&volume=200&speed=120&format=mp3";

// google-home-notifier
GoogleHomeNotifier ghn;
const char displayName[] = "ダイニング ルーム"; // GoogleHomeの名前に変える

WebServer server(80);
String head = "<meta charset=\"utf-8\">";

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("");
    Serial.print("connecting to Wi-Fi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("connected.");
    Serial.println("IP address: " + WiFi.localIP().toString());

    // WebServer
    server.on("/voice", handleVoice);
    server.on("/" + mp3file, handlePlay);
    server.begin();

    // google-home-notifier
    Serial.println("connecting to Google Home...");
    if (ghn.device(displayName, "ja") != true)
    {
        Serial.println(ghn.getLastError());
        return;
    }
    Serial.print("found Google Home(");
    Serial.print(ghn.getIPAddress());
    Serial.print(":");
    Serial.print(ghn.getPort());
    Serial.println(")");

    text2speech("開始します");
}

void loop()
{
    server.handleClient();
}

void handlePlay()
{
    String path = server.uri();
    SPIFFS.begin();
    if (SPIFFS.exists(path))
    {
        Serial.println("handlePlay: sending " + path);
        File file = SPIFFS.open(path, "r");
        server.streamFile(file, "audio/mp3");
        file.close();
    }
    else
    {
        server.send(200, "text/html", head + "<h1>text2speech</h1><br>SPIFFS file not found:" + path);
    }
    SPIFFS.end();
}

void handleVoice()
{
    Serial.println("handleVoice start.");
    if (server.hasArg("text"))
    {
        String text = server.arg("text");
        text2speech(text);
        server.send(200, "text/html", head + "<h1>text2speech</h1><br>" + text);
    }
    Serial.println("handleVoice done.");
}

// text to speech
void text2speech(String text)
{
    Serial.println("text to speech:" + text);

    if ((WiFi.status() == WL_CONNECTED))
    { //Check the current connection status

        HTTPClient http; // Initialize the client library
        size_t size = 0; // available streaming data size

        http.begin(tts_url); //Specify the URL

        Serial.println();
        Serial.println("Starting connection to tts server...");

        //request header for VoiceText Web API
        String auth = base64::encode(tts_user + ":" + tts_pass);
        http.addHeader("Authorization", "Basic " + auth);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String request = String("text=") + URLEncode(text.c_str()) + tts_parms;
        http.addHeader("Content-Length", String(request.length()));

        SPIFFS.begin();
        File f = SPIFFS.open("/" + mp3file, FILE_WRITE);
        if (f)
        {
            //Make the request
            int httpCode = http.POST(request);
            if (httpCode > 0)
            {
                if (httpCode == HTTP_CODE_OK)
                {
                    http.writeToStream(&f);

                    String mp3url = "http://" + WiFi.localIP().toString() + "/" + mp3file;
                    Serial.println("GoogleHomeNotifier.play() start");
                    if (ghn.play(mp3url.c_str()) != true)
                    {
                        Serial.print("GoogleHomeNotifier.play() error:");
                        Serial.println(ghn.getLastError());
                        return;
                    }
                }
            }
            else
            {
                Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            }
            f.close();
        }
        else
        {
            Serial.println("SPIFFS open error!!");
        }
        http.end();
        SPIFFS.end();
    }
    else
    {
        Serial.println("Error in WiFi connection");
    }
}

// from http://hardwarefun.com/tutorials/url-encoding-in-arduino
// modified by chaeplin
String URLEncode(const char *msg)
{
    const char *hex = "0123456789ABCDEF";
    String encodedMsg = "";

    while (*msg != '\0')
    {
        if (('a' <= *msg && *msg <= 'z') || ('A' <= *msg && *msg <= 'Z') ||
            ('0' <= *msg && *msg <= '9') || *msg == '-' || *msg == '_' || *msg == '.' || *msg == '~')
        {
            encodedMsg += *msg;
        }
        else
        {
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 0xf];
        }
        msg++;
    }
    return encodedMsg;
}
