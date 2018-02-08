// ESP8266 NMEA 0183 bridge and filter for GPS data
// by Pierre Schmitz

// Disclaimer: Don't use this application for life support systems,
// navigation or any other situations where system failure may affect
// user or environmental safety.

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

// config: ////////////////////////////////////////////////////////////

#define GPS_BAUD 9600
#define PLOTTER_BAUD 4800
#define packTimeout 5 // ms (if nothing more on UART, then send packet)
#define bufferSize 255

//#define STATIC_IP_ADDR

// For NMEA Filtering
const int NMEA_MAX_SIZE(6);
const int NMEA_HEADER_LENGTH(5);
const int NMEA_PARITY_LENGTH(2);
bool usableSentence(false);
int charCount(0);
int readCh(0);
byte parityIn(0);
byte parityOut(0);
char text[NMEA_MAX_SIZE];
enum TypSentencePart { Header, Body, Parity, Other };
TypSentencePart SentencePart (Other);

// For WiFi Station
const char *ssid = "pups";  // Your ROUTER SSID
const char *pw = "pups"; // and WiFi PASSWORD
#ifdef STATIC_IP_ADDR
IPAddress staticIP(192, 168, 1, 75);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
#endif

// For TCP connection
WiFiServer server(port);
WiFiClient client;

// For UDP connection
const unsigned int localUdpPort = 10120; // 10110 is official TCP and UDP NMEA 0183 Navigational Data Port
const unsigned int remoteUdpPort = 10120;
IPAddress remoteUDPIp(192, 168, 1, 255);

//////////////////////////////////////////////////////////////////////////

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

uint8_t buf1[bufferSize];
uint8_t i1 = 0;

uint8_t buf2[bufferSize];
uint8_t i2 = 0;


int fromHex(char a)
{
  if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else
    return a - '0';
}


void setup() {

  delay(500);

  // start 1st serial connection to dump WIFI status messages over USB
  Serial.begin(GPS_BAUD);
  while (!Serial) ; // wait for Serial Monitor to open
//  Serial.println("Serial connection established");

  // start 2nd serial connection to GPS Plotter via pin GPIO2/D4 (no receive possible)
  Serial1.begin(PLOTTER_BAUD);
  while (!Serial1) ; // wait for Serial1 Monitor to open

  // STATION mode (ESP connects to router and gets an IP)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pw);
#ifdef STATIC_IP_ADDR
  WiFi.config(staticIP, gateway, subnet);
#endif
  Serial.println("Before WiFi connection");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  
  Serial.println("Connected?");
  Serial.printf("WiFi connected with local IP address: ");
  Serial.println(WiFi.localIP());
  udp.begin(localUdpPort); // start UDP server
  udp.beginPacket(remoteUDPIp, remoteUdpPort); // start UDP Packet

  // swap serial port from USB to attached GPS: GPIO15/D8 (TX) and GPIO13/D7 (RX)
  delay(2000);
  Serial.swap();
}


void loop() {

  // if there’s UDP data available, read a packet
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    udp.read(buf1, bufferSize);
    // and then send it to GPS:
    Serial.write(buf1, packetSize);
  }

  readCh = Serial.read();
  if (readCh > 0) {
    udp.write(readCh);
    switch (SentencePart) {
      case Header:
        if (charCount < NMEA_HEADER_LENGTH)  { // Header not yet complete
          text[charCount++] = readCh;
          switch (charCount) {
            case 1:
              parityIn = (byte)readCh;
              parityOut = 'G';
              break;
            case 2:
              parityIn ^= (byte)readCh;
              parityOut ^= 'P';
              break;
            default:
              parityIn ^= (byte)readCh;
              parityOut ^= (byte)readCh;
              break;
          }
        }
        else { // Header complete
          text[charCount++] = 0;
          usableSentence = 0;
          if ((text[2] == 'G') && (text[3] == 'G') && (text[4] == 'A')) usableSentence = 1; // If header is xxGGA
          if ((text[2] == 'R') && (text[3] == 'M') && (text[4] == 'C')) usableSentence = 1; // If header is xxRMC
          charCount = 0;
          if (usableSentence) {
            Serial1.printf("$GP");
            Serial1.print(text[2]);
            Serial1.print(text[3]);
            Serial1.print(text[4]);
            text[charCount++] = readCh; // The character (,) of this loop must be read, otherwise it is lost
            Serial1.printf(",");
            parityIn ^= (byte)readCh;
            parityOut ^= (byte)readCh;
            SentencePart = Body;
          }
          else SentencePart = Other;
        }
        break;
      case Body:
        if (readCh == '*')  { // End of Body reached
          charCount = 0;
          SentencePart = Parity;
        }
        else { // Body not complete yet
          Serial1.write(readCh);
          parityIn ^= (byte)readCh;
          parityOut ^= (byte)readCh;
        }
        break;
      case Parity:
        if (charCount < NMEA_PARITY_LENGTH)  { // Parity not yet complete
          text[charCount++] = readCh;
        }
        else { // Parity complete
          text[charCount++] = 0;
          byte checksum = 16 * fromHex(text[0]) + fromHex(text[1]);
          if (checksum == parityIn) { // Parity OK, sentence can be written with new parity
            Serial1.printf("*");
            Serial1.print(parityOut, HEX);
          }
          else { // Parity NOK, sentence will be witten with parity '00'
            Serial1.printf("*00");
          }
          Serial1.printf("\r\n");
          parityIn = 0;
          charCount = 0;
          SentencePart = Other;
        }
        break;
      case Other:
        switch (readCh) {
          case '$': // Now begins a new NMEA sentence
            charCount = 0;
            SentencePart = Header;
            break;
          case '\n': // Now begins a new line and therefore a new UDP Packet
            udp.endPacket();
            udp.beginPacket(remoteUDPIp, remoteUdpPort);
            break;
        }
        break;
    }
  }
}
