/*
  Bit 0: Program=On/Off
  Bit 1: Preview=On/Off
  Pin 10 - reciever data
  pins 2 - 9 - program tally (HDMI 1-4, SDI 1-4)
*/
/*    Tally bits    |   00|   00|   00|   00|   00|   00|   00|   00|
       Atem input   |  in1|  in2|  in3|  in4|  in5|  in6|  in7|  in8|
       Atem source  |HDMI1|HDMI2|HDMI3|HDMI4| SDI1| SDI2| SDI3| SDI4|
       Atem buttons | Cam3| Cam4| Cam5| Cam6| Cam1| Cam2| Cam7| Cam8|
       Arduino pins |    2|    3|    4|    5|    6|    7|    8|    9| - Program
       Arduino pins |   10|   12|14/A0|15/A1|16/A2|17/A3|18/A4|19/A5| - Preview
       Pin 13 = LED_BUILTIN - blink, when recieving
*/

#include <VirtualWire.h>
#include <Streaming.h>
const int program_leds[8] = {2, 3, 4, 5, 6, 7, 8, 9};
const int preview_leds[8] = {10, 12, 14, 15, 16, 17, 18, 19,};
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 8; i++) {
    pinMode(program_leds[i], OUTPUT);
    pinMode(preview_leds[i], OUTPUT);
    digitalWrite(program_leds[i], LOW);
    digitalWrite(preview_leds[i], LOW);
  }
  Serial.begin(9600);    // Debugging only
  Serial << F("\n- - - - - - - -\nSerial Started\n");

  // Initialise the IO and ISR
//  vw_set_rx_pin(11);
  vw_set_ptt_inverted(true); // Required for DR3100
  vw_setup(2000);   // Bits per sec

  vw_rx_start();       // Start the receiver PLL running

}

void loop() {
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  static word tally;
  uint8_t program = 0;
  uint8_t preview = 0;
  if (vw_get_message(buf, &buflen)) // Non-blocking
  {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.print("Received data = ");
    for (int l = 0; l < buflen; l++) {
      printBits(buf[l]);
      Serial.print(" ");
    }
    Serial.print("\n");
    // Check data length (4), first and fourth byte. Save 2 middle bytes to tally var.

    if (buflen == 4 && buf[0] == B10101010 && buf[3] == B01010101) {
      tally = word(buf[1], buf[2]);
      Serial.print("Tally = ");
      printBits(highByte(tally));
      Serial.print(" ");
      printBits(lowByte(tally));
      Serial.print("\n");
      //Extract program and preview from tally (bit0-program, bit1-preview)
      int b = 0;
      for (int i = 0; i < 16; i = i + 2) {
        bitWrite(program, b, bitRead(tally, i));
        bitWrite(preview, b, bitRead(tally, i + 1));
        b++;
      }
      Serial.print("Program = ");
      printBits(program);
      Serial.print("\n");
      Serial.print("Preview = ");
      printBits(preview);
      Serial.print("\n");
      //Output program to Arduino pins (program_leds), reversing byte
      for (int i = 0; i < 8; i++) {
        digitalWrite(program_leds[i], bitRead(program, 7 - i));
        digitalWrite(preview_leds[i], bitRead(preview, 7 - i));
      }
    }
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void printBits(byte myByte) {
//  Function printBits() just for printing to serial console for debugging
  for (byte mask = 0x80; mask; mask >>= 1) {
    if (mask  & myByte)
      Serial.print('1');
    else
      Serial.print('0');
  }
}
