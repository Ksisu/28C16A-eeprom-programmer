
/*

ARDUINO NANO <- I2C -> MCP23017
EEPROM: 28C16A

28c16a i/o     - mcp.PORTA REVERSE!!! GPA0 <-> IO7
28c16a a[0..7] - mcp.PORTB            GPB0 <-> A0
28c16a a[8]    - D2 (PD2)
28c16a a[9]    - D3 (PD3)
28c16a a[10]   - D6 (PD6)

28c16a /WE     - D4 (PD4)
28c16a /OE     - D5 (PD5)
28c16a /CE     - D7 (PD7)

*/

#include <Wire.h>
#include <MCP23017.h>

#define MCP23017_ADDR 0x20
MCP23017 mcp = MCP23017(MCP23017_ADDR);

#define EEPROM_IO_REVERSE MCP23017Port::A

#define EEPROM_WE PORTD4
#define EEPROM_OE PORTD5
#define EEPROM_CE PORTD7

#define EEPROM_ADDRESS_L MCP23017Port::B
#define EEPROM_ADDRESS8 PORTD2
#define EEPROM_ADDRESS9 PORTD3
#define EEPROM_ADDRESS10 PORTD6

void print_hex_byte(uint8_t data) {
  Serial.print(data < 16 ? "0" : "");
  Serial.print(data, HEX);
}

uint8_t reverse(uint8_t in) {
  uint8_t out;
  out = 0;
  if (in & 0x01) out |= 0x80;
  if (in & 0x02) out |= 0x40;
  if (in & 0x04) out |= 0x20;
  if (in & 0x08) out |= 0x10;
  if (in & 0x10) out |= 0x08;
  if (in & 0x20) out |= 0x04;
  if (in & 0x40) out |= 0x02;
  if (in & 0x80) out |= 0x01;

  return(out);
}

void _eeprom_set_address(word address) {
  uint8_t address_l = (address & 0xFF);
  uint8_t address_8 = (address >> 8) & 0b00000001;
  uint8_t address_9 = (address >> 9) & 0b00000001;
  uint8_t address_10 = (address >> 10) & 0b00000001;
  mcp.writePort(EEPROM_ADDRESS_L, (address & 0xFF));
  digitalWrite(EEPROM_ADDRESS8, (address >> 8) & 0b00000001);
  digitalWrite(EEPROM_ADDRESS9, (address >> 9) & 0b00000001);
  digitalWrite(EEPROM_ADDRESS10, (address >> 10) & 0b00000001);
}

void eeprom_write(word address, uint8_t data) {
  _eeprom_set_address(address);
  mcp.portMode(EEPROM_IO_REVERSE, 0x00);
  mcp.writePort(EEPROM_IO_REVERSE, reverse(data));
  digitalWrite(EEPROM_OE, 1);
  digitalWrite(EEPROM_CE, 0);
  delay(20);
  digitalWrite(EEPROM_WE, 0);
  delay(20);
  digitalWrite(EEPROM_WE, 1);
  digitalWrite(EEPROM_CE, 1);
  delay(150);
}

void printWelcome() {
  Serial.println("=================================");
  Serial.println("| 28C16A EEPROM READER/WRITER   |");
  Serial.println("|        version: 1.0.0         |");
  Serial.println("|                               |");
  Serial.println("| Commands:                     |");
  Serial.println("| - HELP [READ|WRITE|CLEAR]     |");
  Serial.println("| - READ <ADDRESS> [<LENGTH>]   |");
  Serial.println("| - WRITE <ADDRESS> [<LENGTH>]  |");
  Serial.println("| - CLEAR                       |");
  Serial.println("|                               |");
  Serial.println("=================================");
}

void setup() {
  Wire.begin();
  Serial.begin(9600);

  digitalWrite(EEPROM_WE, 1);
  digitalWrite(EEPROM_OE, 1);
  digitalWrite(EEPROM_CE, 1);

  pinMode(EEPROM_WE, OUTPUT);
  pinMode(EEPROM_OE, OUTPUT);
  pinMode(EEPROM_CE, OUTPUT);
  
  pinMode(EEPROM_ADDRESS8, OUTPUT);
  pinMode(EEPROM_ADDRESS9, OUTPUT);
  pinMode(EEPROM_ADDRESS10, OUTPUT);
  
  mcp.init();
  mcp.portMode(EEPROM_IO_REVERSE, 0xFF); // input
  mcp.portMode(EEPROM_ADDRESS_L, 0); // output

  Serial.println();
  Serial.println();
  printWelcome();
}

void handleHelp(String msg) {
  Serial.println("==[HELP]==");
  if(msg.equals("help read")) {
    Serial.println("READ <ADDRESS> [<LENGTH>] - Reads and prints eeprom data");
    Serial.println("  - ADDRESS: hexadecimal number, format 'hhh'; range 000H - 7FFH eg. 'READ 25D'");
    Serial.println("  - LENGTH: (default 1) decimal number eg. 'READ 25D 5'");
    Serial.println("  Output format: hexadecimal numbers without space in one line. eg. '002402FF'");
  } else if(msg.equals("help write")) {
    Serial.println("WRITE <ADDRESS> [<LENGTH>] - Writes data to eeprom");
    Serial.println("  - ADDRESS: hexadecimal number, format 'hhh'; range 000H - 7FFH eg. 'WRITE 25D'");
    Serial.println("  - LENGTH: (default 1) decimal number eg. 'WRITE 25D 5'");
    Serial.println("  - LENGTH: (default 1) decimal number eg. 'WRITE 25D 5'");
    Serial.println("  Waiting for one byte in hexadecimal format in one line. Writes it, checks it, send it back, and waiting for the next byte.");
    Serial.println("  Input format: one byte in hexadecimal format in one line eg. '0F'");
  } else if(msg.equals("help clear")) {
    Serial.println("CLEAR - Clear eeprom data (writes 0xFF)");
  } else {
    Serial.println("ERROR: Unkown command");
  }
}

bool parseAttributes(String msg, word* address, long* length) {
  int spaceIndex = msg.indexOf(' ');
  if(spaceIndex == -1) {
    Serial.println("ERROR: Wrong address/length format");
    return false;
  }
  String attributes = msg.substring(spaceIndex + 1);
  spaceIndex = attributes.indexOf(' ');
  // parse address
  String addressText = spaceIndex == -1 ? attributes : attributes.substring(0, spaceIndex);
  if(addressText.length() != 3) {
    Serial.println("ERROR: Wrong address format");
    return false;
  }
  if(!isHexadecimalDigit(addressText.charAt(0)) || !isHexadecimalDigit(addressText.charAt(1)) || !isHexadecimalDigit(addressText.charAt(2))) {
    Serial.println("ERROR: Wrong address format");
    return false;
  }
  char address_c[] = "0___";
  address_c[1] = addressText.charAt(0);
  address_c[2] = addressText.charAt(1);
  address_c[3] = addressText.charAt(2);
  *address = strtol(address_c, 0, 16);

  // parse length
  if (spaceIndex == -1) {
    *length = 1;
  } else {
    long lengthTmp = attributes.substring(spaceIndex + 1).toInt();
    if(lengthTmp == 0) {
      Serial.println("ERROR: Wrong length format");
      return false;
    }
    *length = lengthTmp;
  }
  return true;
}

void handleRead(String msg) {
  word address;
  long length;
  if(!parseAttributes(msg, &address, &length)) {
    return;
  }
  word endAddress = address + length;
  if(endAddress > 2048) {
    endAddress = 2048;
  }

  mcp.portMode(EEPROM_IO_REVERSE, 0xFF);
  digitalWrite(EEPROM_CE, 0);
  digitalWrite(EEPROM_OE, 0);
  while(address < endAddress) {
    _eeprom_set_address(address);
    delay(10);
    uint8_t data = reverse(mcp.readPort(EEPROM_IO_REVERSE));
    print_hex_byte(data);
    address += 1;
  }
  Serial.println();
  digitalWrite(EEPROM_CE, 1);
  digitalWrite(EEPROM_OE, 1);
  Serial.println("READ FINISHED");
}


void handleWrite(String msg) {
  word address;
  long length;
  if(!parseAttributes(msg, &address, &length)) {
    return;
  }
  word endAddress = address + length;
  if(endAddress > 2048) {
    endAddress = 2048;
  }
  
  while(address < endAddress) {
    char receivedChars[] = "__";
    while (Serial.available() <= 0) {
      delay(10);
    }
    receivedChars[0] = Serial.read();
     while (Serial.available() <= 0) {
      delay(10);
    }
    receivedChars[1] = Serial.read();
    uint8_t data = strtol(receivedChars, 0, 16) & 0xFF;
    eeprom_write(address, data);
    print_hex_byte(data);
    address += 1;
  }
  Serial.println();
  
  Serial.println("WRITE FINISHED");
}


void handleClear() {
  word address = 0;
  word endAddress = 2048;
  uint8_t process = 0;
  while(address < endAddress) {
    eeprom_write(address, 0xFF);
    address += 1;
    uint8_t process_n = ((double) address / 20.48);
    if(process != process_n) {
      process = process_n;
      Serial.print(process);
      Serial.println("%");
    }
  }
  Serial.println("CLEAR FINISHED");
}

void handleMessage(String msg) {
  msg.toLowerCase();
  if(msg.equals("help")) {
    printWelcome();
  } else if(msg.startsWith("help ")) {
    handleHelp(msg);
  } else if(msg.startsWith("read ")) {
    handleRead(msg);
  } else if(msg.startsWith("write ")) {
    handleWrite(msg);
  } else if(msg.equals("clear")) {
    handleClear();
  } else {
    Serial.println("ERROR: Unknown command");
  }
}

String receivedMessage;
void loop() {
  while (Serial.available() > 0) {
    char receivedChar = Serial.read();
    if (receivedChar == '\n') {
      handleMessage(receivedMessage);
      receivedMessage = "";
    } else {
      receivedMessage += receivedChar;
    }
  }
}
