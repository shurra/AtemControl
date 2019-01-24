/*
  172.17.52.110 - IP of BM PS 4k on zal
  172.17.52.111 - IP of console on zal
  Atem inputs: Black=0; Bars=1000; MP1=3010; MP2=3020; Col1=2001; Col2=2002; HDMI1-4=1-4; SDI1-4=5-8

    The circuit:
     KBD Clock (PS2 pin 1) to an interrupt pin on Arduino ( this example pin 2 )
     KBD Data (PS2 pin 5) to a data pin ( this example pin 3 )
     +5V from Arduino to PS2 pin 4
     GND from Arduino to PS2 pin 3
*/
// Including libraries:
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EEPROM.h>      // For storing IP numbers
// Include ATEM library and make an instance:
#include <ATEMbase.h>
#include <ATEMext.h>
ATEMext AtemSwitcher;

#include <Streaming.h>

#include <PS2KeyAdvanced.h>
//#include <PS2Keyboard.h>

/* Keyboard constants  Change to suit your Arduino
   define pins used for data and clock from keyboard */
#define DATAPIN 3
#define IRQPIN  2
#define PS2_LOCK_NUM 1
uint16_t c;

PS2KeyAdvanced keyboard;
//PS2Keyboard keyboard;
//Tally pins:
//int tallyPins[] = {14, 15, 16, 17, 18, 19, 20, 21};
int tallyPins[] = {43, 45, 47, 49, 39, 41, 51, 53};

// Configure the IP addresses and MAC address with the sketch "ConfigEthernetAddresses":
uint8_t ip[4];        // Will hold the Arduino IP address
uint8_t atem_ip[4];  // Will hold the ATEM IP address
uint8_t mac[6];    // Will hold the Arduino Ethernet shield/board MAC address (loaded from EEPROM memory, set with ConfigEthernetAddresses example sketch)

uint8_t atemME = 0;

// -BEGIN- For radio
#include <VirtualWire.h>
// -END- For radio
word tally_prev = 0;
word tally = 0;
unsigned long previousMillis = 0;
const long interval = 1000;

void setup() {
  // Start the Ethernet, Serial (debugging) and UDP:
  Serial.begin(115200);
  Serial << F("\n- - - - - - - -\nSerial Started\n");
  pinMode(LED_BUILTIN, OUTPUT);
  // Setting the Arduino IP address:
  ip[0] = EEPROM.read(0 + 2);
  ip[1] = EEPROM.read(1 + 2);
  ip[2] = EEPROM.read(2 + 2);
  ip[3] = EEPROM.read(3 + 2);

  // Setting the ATEM IP address:
  atem_ip[0] = EEPROM.read(0 + 2 + 4);
  atem_ip[1] = EEPROM.read(1 + 2 + 4);
  atem_ip[2] = EEPROM.read(2 + 2 + 4);
  atem_ip[3] = EEPROM.read(3 + 2 + 4);

  Serial << F("SKAARHOJ Device IP Address: ") << ip[0] << "." << ip[1] << "." << ip[2] << "." << ip[3] << "\n";
  Serial << F("ATEM Switcher IP Address: ") << atem_ip[0] << "." << atem_ip[1] << "." << atem_ip[2] << "." << atem_ip[3] << "\n";

  // Setting MAC address:
  mac[0] = EEPROM.read(10);
  mac[1] = EEPROM.read(11);
  mac[2] = EEPROM.read(12);
  mac[3] = EEPROM.read(13);
  mac[4] = EEPROM.read(14);
  mac[5] = EEPROM.read(15);
  char buffer[18];
  sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial << F("SKAARHOJ Device MAC address: ") << buffer << F(" - Checksum: ")
         << ((mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5]) & 0xFF) << "\n";
  if ((uint8_t)EEPROM.read(16) != ((mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5]) & 0xFF))  {
    Serial << F("MAC address not found in EEPROM memory!\n") <<
           F("Please load example sketch ConfigEthernetAddresses to set it.\n") <<
           F("The MAC address is found on the backside of your Ethernet Shield/Board\n (STOP)");
    while (true);
  }

  Ethernet.begin(mac, ip);

  // Configure the keyboard library
  keyboard.begin( DATAPIN, IRQPIN );
  keyboard.setNoBreak(1);         // No break codes for keys (when key released)
  keyboard.setNoRepeat(1);        // Don't repeat shift ctrl etc

  // Initialize a connection to the switcher:
  AtemSwitcher.begin(IPAddress(atem_ip[0], atem_ip[1], atem_ip[2], atem_ip[3]), 56417);
  //  AtemSwitcher.serialOutput(0x80);  // Remove or comment out this line for production code. Serial output may decrease performance!
  AtemSwitcher.connect();
  //Tally setup
  for (int i = 0; i <= 7; i++) {
    pinMode(tallyPins[i], OUTPUT);
    digitalWrite(tallyPins[i], HIGH);
  }
  for (int i = 0; i <= 7; i++) {
    digitalWrite(tallyPins[i], LOW);
    // delay(200);
  }

  // -BEGIN- For radio
  // Initialise the IO and ISR
  vw_set_tx_pin(38); //Transmitter pin
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);   // Bits per sec
  // -END- For radio
  // ######### End setup ############
}
bool AtemOnline = false;

void loop() {
  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;
      Serial << F("ATEM Connected\n");
    }
    readkey();
    tallyLights();
  } else {
    if (AtemOnline)  {
      AtemOnline = false;
      //ledDisconnected();
      Serial << F("Disconnected\n");

    }
  }
}
//############################################
void tallyLights() {
  unsigned long currentMillis = millis();
  //pins - {43, 45, 47, 49, 39, 41, 51, 53}
  //pins - {16, 17, 18, 19, 14, 15, 20, 21}
  //numbers{ 0,  1,  2,  3,  4,  5,  6,  7}
  //inputs { 5,  6,  1,  2,  3,  4,  7,  8}
  //  static word tally_prev = 0;
  //  static word tally = 0;
  uint8_t atem_tally;
  for (int i = 0; i < 8; i++) {
    atem_tally = AtemSwitcher.getTallyBySourceTallyFlags(i + 1);
    digitalWrite(tallyPins[i], atem_tally & B00000001);
    //    Serial.print("AtemSwitcher.getTallyBySourceTallyFlags = ");
    //    Serial.println(atem_tally, BIN);
    tally = tally << 2;
    tally |= atem_tally;
  }
  if (tally != tally_prev) {
    tallySend(tally);
    tally_prev = tally;
  }
  if (currentMillis - previousMillis >= interval) {
    tallySend(tally);
    previousMillis = currentMillis;
  }

}
//############################################
void readkey() {
  if ( keyboard.available() )
  {
    // read the next key
    c = keyboard.read();
    if ( c > 0 )
    {
      //      Serial.print( F( "Value " ) );
      //      Serial.print( c, HEX );
      //      Serial.print( F( " - Status Bits " ) );
      //      Serial.print( c >> 8, HEX );
      //      Serial.print( F( "  Code " ) );
      //
      //      Serial.println( c & 0xFF, HEX );
      //      Serial.println(c);
      manageButtons(c);
    }
  }
  //else{Serial.print("no key pressed"); Serial.println(c);delay(100);}
}
void manageButtons(int keycode) {
  //int keycode = readkey();
  long level;
  switch (keycode)
  {
    // #####################  Preview Input Buttons
    case 90:  // Key z or Z
    case 4186:  // Key z or Z
      AtemSwitcher.setPreviewInputVideoSource(atemME, 5);
      break;
    case 88:  // Key x or X
    case 4184:  // Key x or X
      AtemSwitcher.setPreviewInputVideoSource(atemME, 6);
      break;
    case 67:  // Key c or C
    case 4163:  // Key c or C
      AtemSwitcher.setPreviewInputVideoSource(atemME, 1);
      break;
    case 86:  // Key v or V
    case 4182:
      AtemSwitcher.setPreviewInputVideoSource(atemME, 2);
      break;
    case 66:  // Key b or B
    case 4162:
      AtemSwitcher.setPreviewInputVideoSource(atemME, 3);
      break;
    case 78:  // Key n or N
    case 4174:
      AtemSwitcher.setPreviewInputVideoSource(atemME, 4);
      break;
    case 77:  // Key m or M
    case 4173:
      AtemSwitcher.setPreviewInputVideoSource(atemME, 7);
      break;
    case 59:  // Key , or <
    case 4155:
      AtemSwitcher.setPreviewInputVideoSource(atemME, 8);
      break;
    case 16646:   // Key Left Shift
      AtemSwitcher.setPreviewInputVideoSource(atemME, 1000); //input 1000 - BARS
      break;
    case 61:  // Key . or >
    case 4157:
      AtemSwitcher.setPreviewInputVideoSource(atemME, 3010); //input 1000 - MP1
      break;
    case 62:  // Key / or ?
    case 4158:
      AtemSwitcher.setPreviewInputVideoSource(atemME, 3020); //input 1000 - MP2
      break;
    // ##################### AUX1 Input Buttons
    case 65:  // Key a or A
    case 4161:
      AtemSwitcher.setAuxSourceInput(0, 5);
      break;
    case 83:  // Key s or S
    case 4179:
      AtemSwitcher.setAuxSourceInput(0, 6);
      break;
    case 68:  // Key d or D
    case 4164:
      AtemSwitcher.setAuxSourceInput(0, 1);
      break;
    case 70:  // Key f or F
    case 4166:
      AtemSwitcher.setAuxSourceInput(0, 2);
      break;
    case 71:  // Key g or G
    case 4167:
      AtemSwitcher.setAuxSourceInput(0, 3);
      break;
    case 72:  // Key h or H
    case 4168:
      AtemSwitcher.setAuxSourceInput(0, 4);
      break;
    case 74:  // Key j or J
    case 4170:
      AtemSwitcher.setAuxSourceInput(0, 7);
      break;
    case 75:  // Key k or K
    case 4171:
      AtemSwitcher.setAuxSourceInput(0, 8);
      break;
    case 58:   // Key '
    case 4154:
      AtemSwitcher.setAuxSourceInput(0, 1000); //input 1000 - BARS
      break;
    case 76:  // Key l or L
    case 4172:
      AtemSwitcher.setAuxSourceInput(0, 10011); //input 10011 - ME 1 Prev
      break;
    case 91:  // Key ;
    case 4187:
      AtemSwitcher.setAuxSourceInput(0, 10010); //input 1000 - ME 1 Prog
      break;
    // ##################### Program Input Buttons
    case 64:   // Key `
    case 4160:
      AtemSwitcher.setProgramInputVideoSource(atemME, 1000); //input 1000 - BARS
      break;
    case 49:  // Key 1
    case 4145:
      AtemSwitcher.setProgramInputVideoSource(atemME, 5);
      break;
    case 50:  // Key 2
    case 4146:
      AtemSwitcher.setProgramInputVideoSource(atemME, 6);
      break;
    case 51:  // Key 3
    case 4147:
      AtemSwitcher.setProgramInputVideoSource(atemME, 1);
      break;
    case 52:  // Key 4
    case 4148:
      AtemSwitcher.setProgramInputVideoSource(atemME, 2);
      break;
    case 53:  // Key 5
    case 4149:
      AtemSwitcher.setProgramInputVideoSource(atemME, 3);
      break;
    case 54:  // Key 6
    case 4150:
      AtemSwitcher.setProgramInputVideoSource(atemME, 4);
      break;
    case 55:  // Key 7
    case 4151:
      AtemSwitcher.setProgramInputVideoSource(atemME, 7);
      break;
    case 56:  // Key 8
    case 4152:
      AtemSwitcher.setProgramInputVideoSource(atemME, 8);
      break;
    case 57:   // Key 9
    case 4153:
      AtemSwitcher.setProgramInputVideoSource(atemME, 3010); //input 3010 - MP1
      break;
    case 48:  // Key 0
    case 4144:
      AtemSwitcher.setProgramInputVideoSource(atemME, 3020); //input 3020 - MP2
      break;
    case 60:  // Key -
    case 4156:
      AtemSwitcher.setProgramInputVideoSource(atemME, 0); //input 0 - Black
      break;
    // ##################### Cut & Auto buttons
    case 43:  // Key Enter on numpad
    case 4139:
      AtemSwitcher.performCutME(atemME);
      break;
    case 281:  // Key 0 on numpad
    case 4377:
    case 32:
    case 4128:
      AtemSwitcher.performAutoME(atemME);
      break;
    // ##################### Transition time buttons
    case 33:  // Key 1 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 25);
      AtemSwitcher.setTransitionWipeRate(atemME, 25);
      break;
    case 34:  // Key 2 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 50);
      AtemSwitcher.setTransitionWipeRate(atemME, 50);
      break;
    case 35:  // Key 3 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 75);
      AtemSwitcher.setTransitionWipeRate(atemME, 75);
      break;
    case 36:  // Key 4 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 100);
      AtemSwitcher.setTransitionWipeRate(atemME, 100);
      break;
    case 37:  // Key 5 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 125);
      AtemSwitcher.setTransitionWipeRate(atemME, 125);
      break;
    case 38:  // Key 6 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 150);
      AtemSwitcher.setTransitionWipeRate(atemME, 150);
      break;
    case 39:  // Key 7 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 175);
      AtemSwitcher.setTransitionWipeRate(atemME, 175);
      break;
    case 40:  // Key 8 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 200);
      AtemSwitcher.setTransitionWipeRate(atemME, 200);
      break;
    case 41:  // Key 9 on numpad
      AtemSwitcher.setTransitionMixRate(atemME, 250);
      AtemSwitcher.setTransitionWipeRate(atemME, 250);
      break;
    // ##################### Volume level buttons
    case 275:  // Key PageUP - MasterVolumeLevel
      level = AtemSwitcher.getAudioMixerMasterVolume();
      AtemSwitcher.setAudioMixerMasterVolume(level + 1000 > 65381 ? 65381 : level + 1000);
      break;
    case 276:  // Key PageDOWN
      level = AtemSwitcher.getAudioMixerMasterVolume();
      AtemSwitcher.setAudioMixerMasterVolume(level - 1000 < 0 ? 0 : level - 1000);
      break;
    // ##################### Run Macros buttons
    case 353:  // Key F1
    case 4449:
      AtemSwitcher.setMacroAction(0, 0);
      break;
    case 354:  // Key F2
    case 4450:
      AtemSwitcher.setMacroAction(1, 0);
      break;
    case 355:  // Key F3
    case 4451:
      AtemSwitcher.setMacroAction(2, 0);
      break;
    case 356:  // Key F4
    case 4452:
      AtemSwitcher.setMacroAction(3, 0);
      break;

  }

}

void tallySend(word tally) {
  // Send tally by radio

  //  char buf[2];
  //  uint8_t *buf;
  uint8_t buf[4];
  //  for (uint8_t i = 0; i < 8; i++) {
  //    Serial.print(s[i], BIN);
  //    Serial.print("__");
  //
  //
  //  }
  Serial.print("\n");
  Serial.print("tally = ");
  Serial.println(tally, BIN);
  //  if (tally != tally_prev) {
  //  tally_prev = tally;
  //  Serial << F("Tally updated: ") << _BINPADL(tally, 8, "0") << F("\n");
  // Send message
  //    const char *msg = "hello";
  //    vw_send((uint8_t *)msg, strlen(msg));
  digitalWrite(LED_BUILTIN, HIGH); // Flash a light to show transmitting
  //  Serial << F("strlen(tally) = ") << strlen(tally) << F("\n");
  //  Serial.println("buf start");
  buf[0] = B10101010;
  buf[1] = highByte(tally);
  //  Serial.println("buf cont");
  buf[2] = lowByte(tally);
  buf[3] = B01010101;
  //  Serial.println("buf end");
  //  Serial << F("strlen(buf) = ") << strlen(buf) << F("\n");
  vw_send((uint8_t*)buf, 4);
  vw_wait_tx(); // Wait until the whole message is gone
  digitalWrite(LED_BUILTIN, LOW);
  //  }
  //  delay(200);
}
