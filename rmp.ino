/*
    MIT License

    Copyright (c) 2021 Holger Teutsch

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    RMP that connects to p3d via FSUIPC

*/

#include <Arduino.h>
#include <LiquidCrystal.h>

#define VERSION "1.0-dev"

static byte arrow_l[] = {
    B00000,
    B00010,
    B00100,
    B01111,
    B00100,
    B00010,
    B00000,
};

static byte arrow_r[] = {
    B00000,
    B01000,
    B00100,
    B11110,
    B00100,
    B01000,
    B00000,
};

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

int active_mHz = 118;
int active_kHz = 0;
int stdby_mHz = 122;
int stdby_kHz = 800;
int mode = 1;

long button_ts;
long heartbeat_ts = -20000000;

char inbuff[50];
int inbuff_nxt = 0;

void
update_display() {
    char buff[17];
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Active   Standby");

    lcd.setCursor(0, 1);
    snprintf(buff, sizeof(buff), "%3d.%03d\001\002%3d.%03d", active_mHz, active_kHz, stdby_mHz, stdby_kHz);
    lcd.print(buff);
}

void
send_message(char type) {
    char buff[30];
    if ('S' == type) {
        snprintf(buff, sizeof buff, "S%06ld_", stdby_mHz * 1000L + stdby_kHz);
        Serial.println(buff);
    } else if ('X' == type) {
        snprintf(buff, sizeof buff, "X%06ld%06ld_", active_mHz * 1000L + active_kHz, stdby_mHz * 1000L + stdby_kHz);
        Serial.println(buff);
    }
}

void
dial_step(int dir) {
    if (0 == mode) {
        stdby_mHz += dir;
        if (stdby_mHz < 118)
            stdby_mHz = 135;
        else if (stdby_mHz > 135)
            stdby_mHz = 118;
    } else {
        int kHz8 = stdby_kHz % 25;
        if (0 == kHz8) {
            stdby_kHz += (dir > 0) ? 5 : -10;
        } else if (15 == kHz8) {
            stdby_kHz += (dir > 0) ? 10 : -5;
        } else {
            stdby_kHz += dir * 5;
        }

        if (stdby_kHz < 0)
            stdby_kHz = 990;
        else if (stdby_kHz > 990)
            stdby_kHz = 0;
    }

    send_message('S');
    update_display();
}

void
xfer() {
    int t = active_mHz; active_mHz = stdby_mHz; stdby_mHz = t;
    t = active_kHz; active_kHz = stdby_kHz; stdby_kHz = t;

    send_message('X');
    update_display();
}

void
process_message(const char *msg) {
    long a, s;
    int cksum;
    int res = sscanf(msg + 1, "%06ld%06ld%1x", &a, &s, &cksum);
    if (res != 3) {
        Serial.print("D error parsing >>>>"); Serial.print(msg); Serial.println("<<<<");
        return;
    }

    active_mHz = a / 1000; active_kHz = a % 1000;
    stdby_mHz = s / 1000; stdby_kHz = s % 1000;
    update_display();
    heartbeat_ts = millis();
}

int
get_message() {
    int na = Serial.available();
    if (na == 0)
        return 0;

    int nr = min(na, sizeof(inbuff) - inbuff_nxt - 1);   // always keep last 0
    int res = Serial.readBytes(inbuff + inbuff_nxt, nr);
    //Serial.print(nr); Serial.print(" "); Serial.println(inbuff_nxt);
    if (res <= 0) {
        Serial.println("short read");
    }
    inbuff_nxt += res;
    inbuff[inbuff_nxt] = '\0';
    for (int i = 0; i < inbuff_nxt; i++) {
        if (inbuff[i] == '\n') {
            if (i > 0 && inbuff[i - 1] == '\r') // always remove a possible CR
                inbuff[i - 1] = '\0';

            inbuff[i++] = '\0';

            //Serial.println(inbuff);

            process_message(inbuff);
            int remain = inbuff_nxt - i;
            memcpy(inbuff, inbuff + i, remain);
            inbuff_nxt = remain;
        }

    }
    return 0;
}

// define some values used by the panel and buttons
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// read the buttons
int read_LCD_buttons()
{
    int adc_key_in = analogRead(0);      // read the value from the sensor
    // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
    // we add approx 50 to those values and check to see if we are close
    if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
    // For V1.1 us this threshold
    if (adc_key_in < 50)   return btnRIGHT;
    if (adc_key_in < 200)  return btnUP;
    if (adc_key_in < 300)  return btnDOWN;
    if (adc_key_in < 450)  return btnLEFT;
    if (adc_key_in < 700)  return btnSELECT;

    return btnNONE;  // when all others fail, return this...
}

void setup() {
    Serial.begin(115200);
    Serial.println("D Startup RMP " VERSION);

    lcd.begin(16, 2);
    lcd.createChar(1, arrow_l);
    lcd.createChar(2, arrow_r);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("rmp " VERSION);
    delay(2000);
}


void loop() {
    long now = millis();

    get_message();

    if (now - heartbeat_ts > 20 * 1000) {
        delay(500);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Waiting for sim");
        return;
    }

    if (now - button_ts > 200) {
        int lcd_key = read_LCD_buttons();  // read the buttons
        button_ts = now;
        switch (lcd_key)   {
            case btnRIGHT:
                mode = 1;
                break;

            case btnLEFT:
                mode = 0;
                break;

            case btnUP:
                dial_step(1);
                break;

            case btnDOWN:
                dial_step(-1);
                break;

            case btnSELECT:
                xfer();
                button_ts += 200;
                break;

            default:
                break;
        }
    }
}
