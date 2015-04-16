#include <String.h>
#include <PS2Keyboard.h>
#include <SPI.h>
#include <SD.h>

#include "M6502.h"

#define RDSR   5
#define WRSR   1
#define READ   3
#define WRITE  2

#define SEND_DATA          30
#define RCV_ACK            31

#define DATA_PIN0          32
#define DATA_PIN1          33
#define DATA_PIN2          34
#define DATA_PIN3          35
#define DATA_PIN4          36
#define DATA_PIN5          37
#define DATA_PIN6          38
#define DATA_PIN7          39

#define SCREEN_COLUMNS     40
#define SCREEN_ROWS        25

#define SCREEN_START       0xF000
#define SCREEN_END         0xF000 + (SCREEN_COLUMNS * SCREEN_ROWS)

#define SCREEN_CMD_REG     SCREEN_END
#define SCREEN_CMD_ARG     SCREEN_CMD_REG + 1

extern void Wr6502(register word Addr, register byte Value);
extern uint8_t Rd6502(register word Addr);
extern uint8_t Op6502(register word Addr);
extern uint8_t Loop6502(register M6502 *R);
extern uint8_t Patch6502(register byte Op,register M6502 *R);

typedef struct {
  uint8_t col;
  uint8_t row;
  uint8_t ch;
} VIDEO_OUT;

VIDEO_OUT video_out;

typedef struct {
  uint8_t indicator;
  uint8_t command;
  uint8_t arg;
} VIDEO_COMMAND;

VIDEO_COMMAND video_command;

char message[100];

// Local (i.e., non-SPI) memory
uint8_t zero_stack_pages[512];
uint8_t keyboard_available = 0;
uint8_t last_key = 0;
uint16_t curr_ram_location = SCREEN_START;

PS2Keyboard keyboard;
M6502 m6502;

#define TX_DELAY       0

#define KBD_DATA_PIN   13
#define KBD_INT_PIN    12

#define SRAM_SELECT    53
#define SD_SELECT      49

#define RESET_PIN      26

void setup() {
  video_command.indicator = 0xFF;
  
  pinMode(RESET_PIN, OUTPUT);
  resetVideoController();
  
  Serial.begin(57600);
  Serial.println("starting setup");
  setupPins();
  delay(1000);
  
  pinMode(SRAM_SELECT, OUTPUT);
  digitalWrite(SRAM_SELECT, HIGH);
  pinMode(SD_SELECT, OUTPUT);
  SD.begin(5);

  Serial.println("init keyboard");
  keyboard.begin(KBD_DATA_PIN, KBD_INT_PIN);
  
  Serial.println("init SPI");
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  
  performRamTest();
  
  initializeRam();

  Reset6502(&m6502);  
  Run6502(&m6502);
}

void resetVideoController() {
  digitalWrite(RESET_PIN, LOW);
  delay(20);
  digitalWrite(RESET_PIN, HIGH);
}

void setupPins() {
  pinMode(SEND_DATA, OUTPUT);
  pinMode(RCV_ACK, INPUT);
  pinMode(DATA_PIN0, OUTPUT);
  pinMode(DATA_PIN1, OUTPUT);
  pinMode(DATA_PIN2, OUTPUT);
  pinMode(DATA_PIN3, OUTPUT);
  pinMode(DATA_PIN4, OUTPUT);
  pinMode(DATA_PIN5, OUTPUT);
  pinMode(DATA_PIN6, OUTPUT);
  pinMode(DATA_PIN7, OUTPUT);
  
  Serial.println("setting SEND_DATA to low");
  digitalWrite(SEND_DATA, LOW);
}

void initializeRam() {
  Serial.println("init ram");
  Wr6502(0xFFFC, 0x00);
  Wr6502(0xFFFD, 0xD0);
  Wr6502(0xFFFE, 0x00);
  Wr6502(0xFFFF, 0xD0);
  
  if (!SD.exists("/dam/rom.bin")) {
    // Disable cursor
    Wr6502(0xD000, 0xA9);    // LDA #
    Wr6502(0xD001, 0x00);
    Wr6502(0xD002, 0x8D);    // STA abs
    Wr6502(0xD003, 0xE9);
    Wr6502(0xD004, 0xF3);
  
    Wr6502(0xD005, 0xA9);    // LDA #
    Wr6502(0xD006, 0x05);
    Wr6502(0xD007, 0x8D);    // STA abs
    Wr6502(0xD008, 0xE8);
    Wr6502(0xD009, 0xF3);
  
    // Now output 'ERR' in upper-left corner
    Wr6502(0xD00A, 0xA9);    // LDA #
    Wr6502(0xD00B, 'E');
    Wr6502(0xD00C, 0x8D);    // STA abs
    Wr6502(0xD00D, 0x00);
    Wr6502(0xD00E, 0xF0);
    Wr6502(0xD00F, 0xA9);    // LDA #
    Wr6502(0xD010, 'R');
    Wr6502(0xD011, 0x8D);    // STA abs
    Wr6502(0xD012, 0x01);
    Wr6502(0xD013, 0xF0);
    Wr6502(0xD014, 0xA9);    // LDA #
    Wr6502(0xD015, 'R');
    Wr6502(0xD016, 0x8D);    // STA abs
    Wr6502(0xD017, 0x02);
    Wr6502(0xD018, 0xF0);
    Wr6502(0xD019, 0x4C);    // JMP abs
    Wr6502(0xD01A, 0x19);
    Wr6502(0xD01B, 0xD0);
  } else {
    File rom_bin = SD.open("/dam/rom.bin", FILE_READ);
    uint16_t curraddr = 0xD000;
    while (rom_bin.available()) {
      uint8_t value = rom_bin.read();
      Wr6502(curraddr++, value);
    }
    rom_bin.close();
  }
  
  // Little test rom
  
  Serial.println("done init ram");
}

void performRamTest() {
  uint16_t address;
  for (int i = 0; i < 65; i++) {
    if (i == 0) {
      address = 0;
    } else {
      address = (i * 1024) - 1;
    }
    Wr6502(address, 255);

    if (Rd6502(address) != 255) {
      sprintf(message, "error at %04x", address);
      Serial.println(message);
    }
  }
}

void Wr6502(register word Addr, register byte Value) {
  sprintf(message, "Wr6502: Addr=%04x, Value=%02x", Addr, Value);
  Serial.println(message);
  if (Addr < 512) {
    zero_stack_pages[Addr] = Value;
  } else if (Addr >= SCREEN_START && Addr < SCREEN_END) {   // Write to screen
    SpiSramWrite8(Addr, Value);
    send_data(Addr, Value);
  } else if (Addr == SCREEN_CMD_REG) {
    SpiSramWrite8(Addr, Value);
    send_video_command(Value);
  } else {
    SpiSramWrite8(Addr, Value);
  }
}

uint8_t Rd6502(register word Addr) {
  uint8_t Value;
  if (Addr < 512) {
    Value = zero_stack_pages [Addr];
  } else if (Addr == 0xF001) {
    Value = keyboard_available;
  } else if (Addr == 0xF002) {
    Value = last_key;
  } else {
    Value = SpiSramRead8(Addr);
  }
  
  sprintf(message, "Rd6502: Addr=%04x, Value=%02x", Addr, Value);
  Serial.println(message);

  return Value;
}

uint8_t Loop6502(register M6502 *R) {
  if (keyboard.available()) {
    keyboard_available = 0xFF;
    last_key = keyboard.read();
  } else {
    keyboard_available = 0x00;
  }
  return INT_NONE;
}

uint8_t Patch6502(register byte Op,register M6502 *R) {
  return 0;
}

uint8_t SpiSramRead8(uint16_t address) {
  uint8_t read_byte;
 
  digitalWrite(SRAM_SELECT, LOW);
  SPI.transfer(READ);
  SPI.transfer((uint8_t)(address >> 8) & 0xff);
  SPI.transfer((uint8_t)address);
  read_byte = SPI.transfer(0x00);
  digitalWrite(SRAM_SELECT, HIGH);
  return read_byte;
}
  
void SpiSramWrite8(uint16_t address, uint8_t data_byte) {
  digitalWrite(SRAM_SELECT, LOW);
  SPI.transfer(WRITE);
  SPI.transfer((uint8_t)(address >> 8) & 0xff);
  SPI.transfer((uint8_t)address);
  SPI.transfer(data_byte);
  digitalWrite(SRAM_SELECT, HIGH);
}

void writeByte(uint8_t data) {
  while (digitalRead(RCV_ACK) == HIGH ) {
    // just wait until video ready to take data - add a timeout here
  }
  
  if (data & 128) {digitalWrite(DATA_PIN0, HIGH);} else {digitalWrite(DATA_PIN0, LOW);}
  if (data & 64)  {digitalWrite(DATA_PIN1, HIGH);} else {digitalWrite(DATA_PIN1, LOW);}
  if (data & 32)  {digitalWrite(DATA_PIN2, HIGH);} else {digitalWrite(DATA_PIN2, LOW);}
  if (data & 16)  {digitalWrite(DATA_PIN3, HIGH);} else {digitalWrite(DATA_PIN3, LOW);}
  if (data & 8)   {digitalWrite(DATA_PIN4, HIGH);} else {digitalWrite(DATA_PIN4, LOW);}
  if (data & 4)   {digitalWrite(DATA_PIN5, HIGH);} else {digitalWrite(DATA_PIN5, LOW);}
  if (data & 2)   {digitalWrite(DATA_PIN6, HIGH);} else {digitalWrite(DATA_PIN6, LOW);}
  if (data & 1)   {digitalWrite(DATA_PIN7, HIGH);} else {digitalWrite(DATA_PIN7, LOW);}
  
  digitalWrite(SEND_DATA, HIGH);
  while (digitalRead(RCV_ACK) == LOW) {
    // just wait for acknowledgement - add a timeout here
  }
  digitalWrite(SEND_DATA, LOW);
}

void writeBytesToVideo(char *buff, uint8_t count) {
  for (int i = 0; i < count; i++) {
    writeByte(buff[i]);
  }
}

void send_video_command(uint8_t command) {
  uint8_t arg = SpiSramRead8(SCREEN_CMD_ARG);
  sprintf(message, "sending command %u with arg %u", command, arg);
  Serial.println(message);
  
  video_command.command = command;
  video_command.arg = arg;
  writeBytesToVideo((char *)&video_command, sizeof(video_command));
}

void send_data(uint16_t Addr, char data) {
  uint8_t row = (Addr - SCREEN_START) / SCREEN_COLUMNS;
  uint8_t col = (Addr - SCREEN_START) - (row * SCREEN_COLUMNS);
  sprintf(message, "sending data: %u x %u : %02x", col, row, data);
  Serial.println(message);
  video_out.col = col;
  video_out.row = row;
  video_out.ch = data;
  writeBytesToVideo((char *)&video_out, sizeof(video_out));
}

void send_data(char data) {
  send_data(curr_ram_location, data);
  curr_ram_location++;
  if (curr_ram_location >= SCREEN_END) {
    curr_ram_location = SCREEN_START;
  }
}

void send_data(char *data) {
  int len = strlen(data);
  for (int i = 0; i < len; i++) {
    send_data(data[i]);
  }
}

void loop() {
}
