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

#define VERSION "1.0-dev"

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <RotaryEncoder.h>
#include <ButtonDebounce.h>

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);
RotaryEncoder kHz_encoder(12, 11, RotaryEncoder::LatchMode::FOUR3);
ButtonDebounce xfer_btn(10, 100);
RotaryEncoder mHz_encoder(9, 8, RotaryEncoder::LatchMode::FOUR3);
RotaryEncoder trim_encoder(7, 6, RotaryEncoder::LatchMode::FOUR3);

static byte arrow_l[] = {
    B00000,
    B00010,
    B00100,
    B01111,
    B00100,
    B00010,
    B00000,
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
    B00000,
};

int active_mHz = 118;
int active_kHz = 0;
int stdby_mHz = 122;
int stdby_kHz = 800;
char trim_dir = '_';
long heartbeat_ts = -20000000;

char inbuff[50];
int inbuff_nxt = 0;

int mHz_pos, kHz_pos, trim_pos;
float kHz_step_time;
static const float EXP_SMOOTH = 0.3;
int active;

void
update_display() {
    char buff[17];
    if (! active) {
        active = 1;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Active   Standby");
    }

    lcd.setCursor(0, 1);
    snprintf(buff, sizeof(buff), "%3d.%03d\001\002%3d.%03d", active_mHz, active_kHz, stdby_mHz, stdby_kHz);
    lcd.print(buff);
}

void
send_message(char type) {
    char buff[30];
    if ('S' == type) {
        snprintf(buff, sizeof buff, "S%06ld_", stdby_mHz * 1000L + stdby_kHz);
     } else if ('X' == type) {
        snprintf(buff, sizeof buff, "X%06ld%06ld_", active_mHz * 1000L + active_kHz, stdby_mHz * 1000L + stdby_kHz);
    } else if ('T' == type) {
        snprintf(buff, sizeof buff, "T%c_", trim_dir);
    } else
        return;
        
    Serial.println(buff);
}

void
dial_step(int mode, int dir, int steps = 1) {
    if (0 == mode) {
        stdby_mHz += dir;
        if (stdby_mHz < 118)
            stdby_mHz = 118;
        else if (stdby_mHz > 135)
            stdby_mHz = 135;
    } else {
        do  {
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

            steps--;
        } while (steps > 0);
    }

    send_message('S');
    update_display();
}

void
xfer(const int state) {
    if (state != 1)
        return;

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

void setup() {
    Serial.begin(115200);
    Serial.println("D Startup RMP " VERSION);
    xfer_btn.setCallback(xfer);
    lcd.init();
    lcd.backlight();

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

#if 0
    if (now - heartbeat_ts > 20 * 1000) {
        delay(500);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Waiting for sim");
        active = 0;
        return;
    }
#endif

    kHz_encoder.tick();
    mHz_encoder.tick();
    int pos = kHz_encoder.getPosition();
    if (pos != kHz_pos) {
        int step_time = min(500, kHz_encoder.getMillisBetweenRotations());

        // exponential smoother for step time
        kHz_step_time =  EXP_SMOOTH * step_time + (1.0 - EXP_SMOOTH) * kHz_step_time;
        //Serial.print(step_time); Serial.print(" "); Serial.println(kHz_step_time);
        int steps = kHz_step_time < 150.0 ? 10 : 1;
        dial_step(1, pos > kHz_pos ? 1 : -1, steps);
        kHz_pos = pos;
    }

    pos = mHz_encoder.getPosition();
    if (pos != mHz_pos) {
        dial_step(0, pos > mHz_pos ? 1 : -1);
        mHz_pos = pos;
    }

    trim_encoder.tick();
    pos = trim_encoder.getPosition();
    if (pos != trim_pos) {
        trim_dir = pos < trim_pos ? 'D' : 'U';
        send_message('T');
        trim_pos = pos;
    }


    xfer_btn.update();
 }
