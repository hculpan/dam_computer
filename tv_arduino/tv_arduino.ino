#include <TVout.h>
#include <fontALL.h>

#define VIDEO_RAM_BASE     0xF000

#define MAX_VIDEO_COL      40
#define MAX_VIDEO_ROW      25

#define FONT_WIDTH         6
#define FONT_HEIGHT        8

#define READ               3
#define WRITE              2

#define DATA_WAITING       30
#define RCV_ACK            31

#define DATA_PIN0          32
#define DATA_PIN1          33
#define DATA_PIN2          34
#define DATA_PIN3          35
#define DATA_PIN4          36
#define DATA_PIN5          37
#define DATA_PIN6          38
#define DATA_PIN7          39

bool clean_screen = false;

TVout TV;

uint16_t cursor_blink_speed = 500;

unsigned char y = 0;
unsigned char x = 0;

long last_millis = 0;
bool cursor_on = false;
bool cursor_disabled = false;

const unsigned char x_inc = 6;
const unsigned char y_inc = 8;

typedef struct {
  uint8_t col;
  uint8_t row;
  uint8_t c;
} VIDEO_WRITE;

VIDEO_WRITE video_write;
bool video_ready = false;

char message[100];

void setup (void) {
  Serial.begin(57600);
  
  pinSetup();
  
  // TV setup
  TV.begin(NTSC, MAX_VIDEO_COL * FONT_WIDTH, MAX_VIDEO_ROW * FONT_HEIGHT);
  TV.select_font(font6x8);
  TV.clear_screen();
  last_millis = TV.millis();
  
  Serial.println("done setup");
}  // end of setup

void pinSetup() {
  pinMode(DATA_WAITING, INPUT);
  pinMode(RCV_ACK, OUTPUT);
  pinMode(DATA_PIN0, INPUT);
  pinMode(DATA_PIN1, INPUT);
  pinMode(DATA_PIN2, INPUT);
  pinMode(DATA_PIN3, INPUT);
  pinMode(DATA_PIN4, INPUT);
  pinMode(DATA_PIN5, INPUT);
  pinMode(DATA_PIN6, INPUT);
  pinMode(DATA_PIN7, INPUT);
  
  digitalWrite(RCV_ACK, LOW);
}

void new_line() {
  y++;
  if (y >= MAX_VIDEO_ROW) {
    y = MAX_VIDEO_ROW - 1;
    TV.shift(y_inc, 0);
  }
  x = 0;
}

void erase_char() {
  TV.draw_rect(x * x_inc, y * y_inc, x_inc, y_inc, 0, 0);
}

void backspace() {
  if (x > 0) {
    erase_char();
    x--;
  }
}

void clearscreen() {
  TV.clear_screen();
  x = 0;
  y = 0;
}

void erase_char(uint8_t xloc, uint8_t yloc) {
  TV.draw_rect(xloc * x_inc, yloc * y_inc, x_inc, y_inc, 0, 0);
}

void output_char(uint8_t xloc, uint8_t yloc, uint8_t c) {
  if (cursor_on) {
    draw_cursor();
  }
  
  if (c > 31 && c < 128) {
    TV.print_char(xloc * x_inc, yloc * y_inc, c);
  } else {
    erase_char(xloc, yloc);
  }
}

void output_char(unsigned char c) {
  if (cursor_on) {
    draw_cursor();
  }
  
  if (c == 147) {
    clearscreen();
  } else if (c == 127) { // backspace
    backspace();
  } else if (c == '\n') {
    new_line();
  } else if (c == '\r') {
    new_line();
  } else if (c > 31 && c < 128) {
    TV.print_char(x * x_inc, y * y_inc, c);
    x++;
    if (x >= MAX_VIDEO_COL) {
      new_line();
    }
  }
}

void draw_cursor() {
  if (cursor_on && cursor_disabled) {
    TV.draw_rect(x * x_inc, y * y_inc, x_inc - 1, y_inc, 0, 0);
    cursor_on = false;
  } else if (!cursor_disabled) {
    cursor_on = !cursor_on;
    if (cursor_on) {
      TV.draw_rect(x * x_inc, y * y_inc, x_inc - 2, y_inc - 2, 1, 1);
    } else {
      TV.draw_rect(x * x_inc, y * y_inc, x_inc - 1, y_inc, 0, 0);
    }
  }
}

void handle_video_command() {
  uint8_t command = video_write.row;
  uint8_t arg = video_write.c;
  
  if (command == 0) {
    output_char(arg);
  } else if (command == 1 && arg == 0) {
    backspace();
  } else if (command == 1 && arg == 1) {
    x = 0;
    y = 0;
  } else if (command == 2) {
    clearscreen();
  } else if (command == 0x05 && arg == 0x00) { // disable cursor
    cursor_disabled = true;
    cursor_on = false;
    draw_cursor();
  } else if (command == 0x05 && arg == 0x01) {
    cursor_disabled = false;
    cursor_on = false;
    cursor_blink_speed = 0;
    draw_cursor();
  } else if (command == 0x05 && arg == 0x02) {
    cursor_disabled = false;
    cursor_on = false;
    cursor_blink_speed = 500;
    draw_cursor();
  } else if (command == 0x05 && arg == 0x03) {
    cursor_disabled = false;
    cursor_on = false;
    cursor_blink_speed = 1000;
    draw_cursor();
  } else if (command == 10 && arg < MAX_VIDEO_COL) {
    x = arg;
  } else if (command == 11 && arg < MAX_VIDEO_ROW) {
    y = arg;
  }
}

uint8_t readByte() {
  while (digitalRead(DATA_WAITING) == LOW) {
    // Need to wait until set to low
  }
  uint8_t result = 0;
  result = digitalRead(DATA_PIN7);
  result = result | (digitalRead(DATA_PIN6) << 1);
  result = result | (digitalRead(DATA_PIN5) << 2);
  result = result | (digitalRead(DATA_PIN4) << 3);
  result = result | (digitalRead(DATA_PIN3) << 4);
  result = result | (digitalRead(DATA_PIN2) << 5);
  result = result | (digitalRead(DATA_PIN1) << 6);
  result = result | (digitalRead(DATA_PIN0) << 7);
  digitalWrite(RCV_ACK, HIGH);
  while (digitalRead(DATA_WAITING) == HIGH) {
    // Need to wait until set to low
  }
  digitalWrite(RCV_ACK, LOW);
  return result;
}

void readData() {
  video_write.col = readByte();
  video_write.row = readByte();
  video_write.c = readByte();
  video_ready = true;
}

// main loop - wait for flag set in interrupt routine
void loop (void) {
  if (digitalRead(DATA_WAITING) == HIGH) {
    readData();
    sprintf(message, "col=%u, row=%u, c=%02x", video_write.col, video_write.row, video_write.c);
    Serial.println(message);
  }
  
  if (video_ready) {
    if (video_write.col == 0xFF) { // we have a command
      handle_video_command();
    } else {
      output_char(video_write.col, video_write.row, video_write.c);
    }
    video_ready = false;
  }
  
  if (!cursor_disabled && cursor_blink_speed > 0 && TV.millis() > last_millis + cursor_blink_speed) {
    last_millis = TV.millis();
    draw_cursor();
  }
}  

