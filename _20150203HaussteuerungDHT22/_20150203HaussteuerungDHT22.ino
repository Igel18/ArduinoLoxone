// TcpOptoInput.ino
// Company: KMP Electronics Ltd, Bulgaria
// Web: http://kmpelectronics.eu/
// License: See the GNU General Public License for more details at http://www.gnu.org/copyleft/gpl.html
// Supported boards:
//		KMP DiNo II NETBOARD V1.0. Web: http://kmpelectronics.eu/en-us/products/dinoii.aspx
// Description:
//		Tcp opto input read example. Web: http://www.kmpelectronics.eu/en-us/examples/dinoii/tcpoptoinputscheck.aspx

// Version: 1.0.0
// Date: 24.01.2014
// Author: Plamen Kovandjiev <p.kovandiev@kmpelectronics.eu>

// Version: 2.0.0
// Date: 29.07.2014
// Author: Dominik Prudldo <Dominik.Prudlo@Prudlo.ch
// -Digitale Eingaenge (Opto In)
//    fuer jeden Eingang wird bei Aenderung ein Paket ueber UDP verschickt
// -1Wire Devices
//    fuer jedes 1Wire Device wird bei Aenderung ein Paket ueber UDP verschickt
// -Digitale Ausgaenge (Relais Out)
//    ueber den UDP-Listener wird auf Port 1000 gelauscht ob die Relais sich aendern sollen

// Version: 2.0.1
// Date: 30.07.2014
// Separate IP fuer senden und empfangen
// Debug Texte uebersetzt.
// -Digitale Eingaenge (Opto In)
//   keine Schleifen ueber die eingaenge, einzelne pakete schicken
// -1Wire Devices
//   Adresse nicht mehr uebergeben, da sie nicht ausgewertet werden kann.
// -Digitale Ausgaenge (Relais Out)
//   keine Schleifen ueber die Relais, einzelne pakete empfangen

// Version: 2.0.2
// Date: 31.07.2014
// Debugging entfernt.
// -Digitale Ausgaenge (Relais Out)
//   Rueckgabe fuer Relaisausgaenge ueber UDP immer wenn sich der Wert geaendert hat schicken.

// Version: 2.0.3
// Date: 01.08.2014
// LiveBit jede Minute

// Version: 2.0.4
// Date: 02.09.2014
// LiveBit jede Minute angepasst

// Version: 3.0.0 
// Date: 14.01.2015 
// DHT22 funktion integriert 

// Version: 3.0.1 
// Date: 03.02.2015 
// DHT22 Library geändert auf https://github.com/RobTillaart/Arduino 
// und entsprechend umgebaut

#include <SPI.h>
#include "Ethernet.h"
#include "KmpDinoEthernet.h"

// One Wire headers
#include <OneWire.h>
#include <DallasTemperature.h>

// DHT 22 
#include <dht.h>

// If in debug mode - print debug information in Serial. Comment in production code, this bring performance.
// This method is good for development and verification of results. But increases the amount of code and decreases productivity.
#define DEBUG

// Thermometer Resolution in bits. http://datasheets.maximintegrated.com/en/ds/DS18B20.pdf page 8.
// Bits - CONVERSION TIME. 9 - 93.75ms, 10 - 187.5ms, 11 - 375ms, 12 - 750ms.
#define TEMPERATURE_PRECISION 9

// Enter a MAC address and IP address for your controller below.
byte     macArduino[] = {
  0x90, 0xA2, 0xDA, 0x00, 0xF9, 0x11
};
// IP address des Arduino (lokal)
byte      ipArduino[] = {
  192, 168, 2, 11
};

// IP address der Loxone. 192, 168, 178, 3
byte      ipLoxone[] = {
  192, 168, 178, 3
};

// Gateway.
byte gatewayArduino[] = {
  192, 168, 178, 1
};
// Sub net mask.
byte  subnetArduino[] = {
  255, 255, 0, 0
};

// Port Arduino
unsigned int portArduino = 1011;

// Port Loxone
unsigned int portLoxone = 1011;

// Define constants.
// Pin an dem der DHT22 angeschlossen wird, er ist nicht Dallas-1Wire kompatibel, kann aber trotzdem an dem Port betrieben werden. 
#define DHT22_PIN 5 
const char Chr0 = '0';
const char Chr1 = '1';
char* On = "On";
char* Off = "Off";
// mit diesem Praefix wird ein Datenpaket erkannt
char* Prefix = "FF";
//Opto Out Command
const char CommandInput = 'I';
//Relay Command
const char CommandOutput = 'R';
//OneWire Command
const char CommandOneWire = 'O';
//DHT Temperature Command
const char CommandDHTtemp = 'T'; 
//DHT Humidity Command
const char CommandDHThumi = 'H'; 
//Livebit Command
const char CommandLivebit = 'L';
char CommandSend = '\0';
// Calculate command length.
int CommandLen = strlen(Prefix) + strlen(&CommandOutput) + strlen(&Chr0) ;

//Damit nur pakete geschickt werden wenn sich der wert aendert (traffic vermeiden)
//speicher fuer Optische Eingaenge
bool digitalIn[4];
//speicher fuer Relais Ausgaenge
bool digitalOut[4];
//speicher fuer OneWireDevices
int oneWireDevice[10];

//Zaehler fuer Livebit
int liveCounter;
// Buffer for receiving data.
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(OneWirePin);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Number of one wire temperature devices found.
int numberOfOneWireDevices;

// Temp device address.
DeviceAddress tempDeviceAddress;

// DHT Sensor
dht DHT;
struct
{

  uint32_t ok;
  uint32_t crc_error;
  uint32_t time_out;
  uint32_t connect;
  uint32_t ack_l;
  uint32_t ack_h;
  uint32_t unknown;
} 
stat = { 
  0,0,0,0,0,0,0};


/// <summary>
/// Setup void. Arduino executed first. Initialize DiNo board and prepare Ethernet connection.
/// </summary>
void setup()
{
#ifdef DEBUG
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  //while (!Serial) {
  //	; // wait for serial port to connect. Needed for Leonardo only. If need debug setup() void.
  //}
#endif

  // Init Dino board. Set pins, start W5200.
  DinoInit();

  // Start the Ethernet connection and the server.
  Ethernet.begin(macArduino, ipArduino, gatewayArduino, subnetArduino);
  Udp.begin(portArduino);

#ifdef DEBUG
  Serial.println("UDP listener gestartet.");
  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
#endif

  // Start the One Wire library.
  sensors.begin();

  // Set precision.
  sensors.setResolution(TEMPERATURE_PRECISION);

  // Select available connected to board One Wire devices.
  GethOneWireDevices();

  //Livebitzahler auf null setzten
  liveCounter = 0;
}

/// <summary>
/// Loop void. Arduino executed second.
/// </summary>
void loop()
{
  //LiveBit
  LiveBit();

  // listen for incoming packets.
  int packetSize = Udp.parsePacket();

#ifdef DEBUG
  //Serial.println("Client verbunden.");
  //Serial.print("Empfange Pakete der laenge: ");
  //Serial.println(packetSize);
#endif

  // If client connected On status led.
  OnStatusLed();

  //Wenn Pakete ueber UDP kommen ist es idR. fuer das Relay-> Lesen
  if (packetSize)
  {
    //Pakete lesen
    ReadClientRequest();
  }

  //Relais status auslesen ueber udp schreiben
  WriteClientResponseRelay();

  //Optische ausgaenge ueber udp schreiben
  WriteClientResponseOpto();

  //OneWire ausgaenge ueber udp schreiben
  WriteClientResponseOneWire();

  //DHT ausgaenge ueber udp schreiben
  WriteClientResponseDHT(); 

  // Give the client time to receive the data.
  delay(10);

  // If client disconnected Off status led.
  OffStatusLed();

  //LiveBit zuruecksetzen 
  LiveBitReset(); 
}

void LiveBit()
{
  liveCounter++;
  //Da eine zykluszeit von 10ms programmiert ist
  if (liveCounter >=  300)
  {
    //Udp Paket schicken
    Udp.beginPacket(ipLoxone, portLoxone);
    // Send a reply, to the IP address and port that sent us the packet we received.
    Udp.write(Prefix);

//Befehl Livebit
    Udp.write(CommandLivebit);

    Udp.endPacket();
    
#ifdef DEBUG
    Serial.println("Lifebit geschickt an Loxone");
#endif
  }

#ifdef DEBUG
  Serial.print("Live Counter: ");
  Serial.println(liveCounter);
  Serial.println("---");
#endif
}

void LiveBitReset()
{
  if (liveCounter >=  300)
  {
    //Zaehler zuruecksetzen
    liveCounter = 0;  
  }
}

void ReadClientRequest()
{
  CommandSend = '\0';

  int requestLen = 0;
  int readSize;
  // Read all sended data into pieces RECV_PACKET_MAX_SIZE.
  while ((readSize = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE)) > 0)
  {
    requestLen += readSize;

    // If packet is not valid - read to end.
    if (requestLen != CommandLen)
      continue;

    // Check first 2 chars is prefix.
    // If Prefix not equals first 2 chars not checked any more.
    if (strncmp(packetBuffer, Prefix, 2) > 0)
      continue;

    if (packetBuffer[2] == CommandOutput)
    {
      //Relais ausgaenge schreiben
      ReadClientRequestRelay();
    }
  }
}

/// <summary>
/// WriteClientResponse void. Write html response.
/// </summary>
void WriteClientResponseOpto()
{
  // Add relay data
  for (int i = 0; i < OPTOIN_COUNT; i++)
  {
    //Nur schreiben wenn sich der Zustand der Eingaenge geaendert hat (traffic vermeiden)
    if (digitalIn[i] != GetOptoInStatus(i))
    {
      Udp.beginPacket(ipLoxone, portLoxone);
      // Send a reply, to the IP address and port that sent us the packet we received.
      Udp.write(Prefix);
      //Befehl Optischer Eingang
      Udp.write(CommandInput);
      //Eingangsnummer
      Udp.write(i + 1);
      //Wert schreiben; GetOptoInStatus 0-3
      Udp.write(GetOptoInStatus(i) ? Chr1 : Chr0);
      //Wert Speichern
      digitalIn[i] = GetOptoInStatus(i);
      Udp.endPacket();
    }
  }
}

/// <summary>
/// Read Request relay
/// Command R for relays.
/// </summary>
void ReadClientRequestRelay()
{
  CommandSend = CommandOutput;
  // Position im Datenpaket fuer Ein/Aus information
  int valuePos = 4;

  // Postition im Datenpaket fuer Relais auswahl
  int relaisPos = 3;

  char valueChar;
  int relaisInt;
  // Read chars for On or Off for all relays.
  valueChar = packetBuffer[valuePos];
  relaisInt = packetBuffer[relaisPos] - 49;

  boolean status;
  if (valueChar == Chr0 || valueChar == Chr1)
  {
    // if c == 1 - true. if c == 0 false.
    status = valueChar == Chr1;

    //Schreibt den Status des Relais SetRelayStatus(0-3, 0/1)
    SetRelayStatus(relaisInt, status);
  }
}

/// <summary>
/// WriteClientResponse void. Write html response.
/// </summary>
void WriteClientResponseRelay()
{
  // Add relay data
  for (int i = 0; i < RELAY_COUNT; i++)
  {
    if (digitalOut[i] != GetRelayStatus(i))
    {
      Udp.beginPacket(ipLoxone, portLoxone);
      // Send a reply, to the IP address and port that sent us the packet we received.
      Udp.write(Prefix);
      // Response write.
      Udp.write(CommandOutput);
      //Add Relais Nr
      Udp.write(IntToChars(i + 1));
      //GetRealyStatus(0-3)
      Udp.write(GetRelayStatus(i) ? Chr1 : Chr0);
      //Wert Speichern
      digitalOut[i] = GetRelayStatus(i);
      Udp.endPacket();
    }
  }
}

/// <summary>
/// WriteClientResponse void. Write html response.
/// </summary>
void WriteClientResponseOneWire()
{
  // Send the command to get temperatures.
  sensors.requestTemperatures();

  //OneWire Adresse als Char
  char adress[8];

  // Add OneWire data
  for (int i = 0; i < numberOfOneWireDevices; i++)
  {
    float temp;
    bool sensorAvaible = sensors.getAddress(tempDeviceAddress, i);
    if (sensorAvaible)
    {
      // Get temperature in Celsius.
      temp = sensors.getTempC(tempDeviceAddress);
    }

    //Nur schicken wenn sich der wert geaendert (nachkommastellen ueber cast abgeschnitten) hat (traffic vermeiden)
    if (oneWireDevice[i] != (int)temp)
    {
      Udp.beginPacket(ipLoxone, portLoxone);
      // Send a reply, to the IP address and port that sent us the packet we received.
      Udp.write(Prefix);
      Udp.write(CommandOneWire);

      //Add sensor Nr
      Udp.write(IntToChars(i + 1));

      //Hardware Adresse in char wandeln
      for (uint8_t i = 0; i < 8; i++)
      {
        adress[i] = tempDeviceAddress[i];
      }
      //Add sensor Hardware-Adresse
      Udp.write(adress);

      // Add temperatur in °C
      Udp.write(FloatToChars(temp, 1));
      oneWireDevice[i] = (int)temp  ;
      Udp.endPacket();
    }
  }
}

void WriteClientResponseDHT()
{
  if (liveCounter >= 300)
  {
#ifdef DEBUG
    Serial.print("DHT22, \t");
#endif

    WriteClientResponseDHTtemp();
    WriteClientResponseDHThumi();

  }
  //DHTDebug();

}

void DHTDebug()
{
#ifdef DEBUG
  // READ DATA
  uint32_t start = micros();
  int chk = DHT.read22(DHT22_PIN);
  uint32_t stop = micros();

  switch (chk)
  {
  case DHTLIB_OK:
    stat.ok++;
    Serial.print("OK,\t");
    break;
  case DHTLIB_ERROR_CHECKSUM:
    stat.crc_error++;
    Serial.print("Checksum error,\t");
    break;
  case DHTLIB_ERROR_TIMEOUT:
    stat.time_out++;
    Serial.print("Time out error,\t");
    break;
  case DHTLIB_ERROR_CONNECT:
    stat.connect++;
    Serial.print("Connect error,\t");
    break;
  case DHTLIB_ERROR_ACK_L:
    stat.ack_l++;
    Serial.print("Ack Low error,\t");
    break;
  case DHTLIB_ERROR_ACK_H:
    stat.ack_h++;
    Serial.print("Ack High error,\t");
    break;
  default:
    stat.unknown++;
    Serial.print("Unknown error,\t");
    break;
  }  
#endif

}


/// <summary>
/// Liest die DHT temp aus und gibt das Signal ueber UDP aus. 
/// </summary>
void WriteClientResponseDHTtemp()
{
  Udp.beginPacket(ipLoxone, portLoxone);
  // Send a reply, to the IP address and port that sent us the packet we received.
  Udp.write(Prefix);
  Udp.write(CommandDHTtemp);

  // Add temperatur in °C
  Udp.write(FloatToChars(DHT.temperature, 1));
  Udp.endPacket();

#ifdef DEBUG
  Serial.println("Temperature in C: ");
  Serial.println(FloatToChars(DHT.temperature, 1));
#endif
}

/// <summary>
/// Liest die DHT temp aus und gibt das Signal ueber UDP aus. 
/// </summary>
void WriteClientResponseDHThumi()
{
  Udp.beginPacket(ipLoxone, portLoxone);
  // Send a reply, to the IP address and port that sent us the packet we received.
  Udp.write(Prefix);
  Udp.write(CommandDHThumi);

  // Add Humidity in %
  Udp.write(FloatToChars(DHT.humidity, 1));
  Udp.endPacket();

#ifdef DEBUG
  Serial.println("Luftfeuchte in %: ");
  Serial.println(FloatToChars(DHT.humidity, 1));   
  Serial.println("---");
#endif
}

/// <summary>
/// Get all available One Wire devices.
/// </summary>
void GethOneWireDevices()
{
  // Grab a count of devices on the wire.
  numberOfOneWireDevices = sensors.getDeviceCount();
}










