#include <AltSoftSerial.h>

// AltSoftSerial always uses these pins:
//
// Board          Transmit              Receive         PWM Unusable
// -----          --------              -------         ------------
// Arduino Uno        9                   8               10
// Arduino Pro Mini   9 (silbern im Eck)  8(der daneben)  10(auf der anderen Seite im Eck)

AltSoftSerial altSerial;

const int NMEA_MAX_SIZE(6);
const int NMEA_HEADER_LENGTH(5);
const int NMEA_PARITY_LENGTH(2);

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
  Serial.begin(4800);
  while (!Serial) ; // wait for Arduino Serial Monitor to open
  altSerial.begin(9600);
}

bool usableSentence(false);
int charCount(0);
int readCh(0);
byte parityIn(0);
byte parityOut(0);
char text[NMEA_MAX_SIZE];
enum TypSentencePart { Header, Body, Parity, Other };
TypSentencePart SentencePart (Other);

void loop() {
  readCh = altSerial.read();
  if (readCh > 0) {
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
          // Serial.print("Header: ");
          // Serial.println(text);
          usableSentence = 0;
          if ((text[2] == 'G') && (text[3] == 'G') && (text[4] == 'A')) usableSentence = 1; // If header is xxGGA
          if ((text[2] == 'R') && (text[3] == 'M') && (text[4] == 'C')) usableSentence = 1; // If header is xxRMC
          //if (usableSentence) Serial.print(text);
          charCount = 0;
          if (usableSentence) {
            Serial.print("$GP");
            Serial.print(text[2]);
            Serial.print(text[3]);
            Serial.print(text[4]);
            text[charCount++] = readCh; // The character (,) of this loop must be read, otherwise it is lost
            Serial.print(",");
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
          Serial.write(readCh);
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
            Serial.print("*");
            Serial.print(parityOut, HEX);
          }
          else { // Parity NOK, sentence will be witten with parity '00'
            Serial.print("*00");
          }
          Serial.print("\r\n");
          parityIn = 0;
          charCount = 0;
          SentencePart = Other;
        }
        break;
      case Other:
        if (readCh == '$')  { // Now begins a new NMEA sentence
          //Serial.print("Now begins a new NMEA sentence\n\r");
          charCount = 0;
          SentencePart = Header;
        }
        break;
    }
  }
}

