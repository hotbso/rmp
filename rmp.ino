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
#include <limits.h>
#include <LiquidCrystal_I2C.h>
#include <RotaryEncoder.h>
#include <ButtonDebounce.h>
#include "rmp_wiring.h"

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(lcd_I2C, 16, 2);

RotaryEncoder kHz_encoder(kHz_encoder_pin1, kHz_encoder_pin2, kHz_latch_mode);
RotaryEncoder mHz_encoder(mHz_encoder_pin1, mHz_encoder_pin2, mHz_latch_mode);
ButtonDebounce xfer_btn(xfer_button_bin_pin, 100);

#ifdef TRIM_WHEEL
RotaryEncoder trim_encoder(trim_encoder_pin1, trim_encoder_pin2, trim_latch_mode);
#endif

// time to keep display on after activity
#define DISPLAY_ON_DIAL_SEC 600L
#define DISPLAY_ON_XFER_SEC 2400L       // if xfer is pressed it's likely that it's really used

//
// acceleration on continuous turns
//
class ProgressiveDial {
    private:
        int _max_steps;
        static constexpr int _nbuf = 25;
        long _ts[_nbuf];
        int _head = 0;
        RotaryEncoder::Direction _last_dir = RotaryEncoder::Direction::NOROTATION;

    public:
        ProgressiveDial(const int max_steps) {
            _max_steps = max_steps;
            _ts[_nbuf - 1] = -LONG_MAX;
        }

        int steps(RotaryEncoder::Direction dir) {
            // save ts of call
            long now = millis();

            // if we reverse direction reinit the array
            if (dir != _last_dir) {
                _last_dir = dir;
                _head = 0;
                _ts[_nbuf - 1] = -LONG_MAX;
            }

            _ts[_head] = now;

            // count clicks in 1s,2s,3s,4s
            int clicks_1s = 1;
            int clicks_2s = 1;
            int clicks_3s = 1;
            int clicks_4s = 1;

            int i = _head - 1;
            while (1) {
                if (i < 0) i = _nbuf - 1;
                if (i == _head) break;
                if (_ts[i] > now - 1000) clicks_1s++;
                if (_ts[i] > now - 2000) clicks_2s++;
                if (_ts[i] > now - 3000) clicks_3s++;
                if (_ts[i] > now - 4000)
                    clicks_4s++;
                else
                    break;

                i--;
            }

            if (++_head == _nbuf)
                _head = 0;

            //Serial.print("1s: "); Serial.print(clicks_1s); Serial.print(", 2s: "); Serial.print(clicks_2s);
            //Serial.print(", 3s: "); Serial.print(clicks_3s); Serial.print(", 4s: "); Serial.println(clicks_4s);

            // fine tuning
            if (clicks_1s <= 4) return 1;
            if (clicks_2s <= 8) return 2;

            // ramp up to max_steps between 9 and max
            if (clicks_4s > 8) {
                return round(4 + (clicks_4s - 8.0) / (_nbuf - 8) * (_max_steps - 4));
            }

            return 2;
        }
};

ProgressiveDial kHz_dial(10);
ProgressiveDial mHz_dial(4);

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
long heartbeat_ts;
long display_off_ts;
int active;

char inbuff[50];
int inbuff_len = 0;


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

// send message to p3d
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

// process dial steps for frequency
void
dial_step(int mode, RotaryEncoder::Direction dir, int steps = 1) {
    // Serial.print("steps: "); Serial.println(steps);
    long now = millis();

    // if display is off just switch it on on first freq change
    if (now > display_off_ts) {
        lcd.backlight();
        update_display();
        display_off_ts = now + DISPLAY_ON_DIAL_SEC * 1000;
        return;
    }

    if (0 == mode) {
        stdby_mHz += (int)dir;
        // the mHz don't wrap
        if (stdby_mHz < 118)
            stdby_mHz = 118;
        else if (stdby_mHz > 135)
            stdby_mHz = 135;
    } else {
        do  {
            int kHz8 = stdby_kHz % 25;
            // wrap the kHz
            if (0 == kHz8) {
                stdby_kHz += (dir == RotaryEncoder::Direction::CLOCKWISE) ? 5 : -10;
            } else if (15 == kHz8) {
                stdby_kHz += (dir == RotaryEncoder::Direction::CLOCKWISE) ? 10 : -5;
            } else {
                stdby_kHz += (int)dir * 5;
            }

            if (stdby_kHz < 0)
                stdby_kHz = 990;
            else if (stdby_kHz > 990)
                stdby_kHz = 0;

            steps--;
        } while (steps > 0);
    }

    display_off_ts = max(display_off_ts, now + DISPLAY_ON_DIAL_SEC * 1000);
    update_display();
    send_message('S');
}

// button press callback, send XFER message
void
xfer(const int state) {
    if (state != 1)
        return;

    int t = active_mHz; active_mHz = stdby_mHz; stdby_mHz = t;
    t = active_kHz; active_kHz = stdby_kHz; stdby_kHz = t;

    long now = millis();

    // if display is off just switch it on on first freq change
    if (now > display_off_ts)
        lcd.backlight();

    update_display();
    display_off_ts = now + DISPLAY_ON_XFER_SEC * 1000;

    send_message('X');
}

// Process a received message
void
process_message(const char *msg, int len) {
    if (('H' == msg[0]) && (14 == len)) {
        long a, s;

        int res = sscanf(msg + 1, "%06ld%06ld_", &a, &s);
        if (res != 2)
            goto error;

        active_mHz = a / 1000; active_kHz = a % 1000;
        stdby_mHz = s / 1000; stdby_kHz = s % 1000;
        update_display();
        heartbeat_ts = millis();
    } else
        goto error;

    return;

error:
    Serial.print("D error parsing >>>>"); Serial.print(msg); Serial.println("<<<<");
    return;

}

// assemble a message i.e. chars until a lf
void get_message() {
    int c;
    while ((c = Serial.read()) > 0) {
        if ('\r' == c)  // drop CR
            continue;

        if ('\n' == c) {
            inbuff[inbuff_len] = '\0';
            process_message(inbuff, inbuff_len);
            inbuff_len = 0;
            continue;
        }

        inbuff[inbuff_len++] = c;
        if (inbuff_len == sizeof(inbuff)) {
            inbuff_len = 0;
            Serial.println("D receiving garbage");
        }
    }
}


void setup() {
    Serial.begin(115200);
    Serial.println("D Startup RMP " __DATE__ " " __TIME__);
    xfer_btn.setCallback(xfer);
    lcd.init();
    lcd.backlight();

    lcd.createChar(1, arrow_l);
    lcd.createChar(2, arrow_r);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("rmp " __DATE__);
    lcd.setCursor(0, 1);
    lcd.print(__TIME__);

    delay(3000);
    display_off_ts = millis() + 30L * 1000;
}


void loop() {
    long now = millis();
    if (now > display_off_ts) {
        lcd.noBacklight();
    }

    if (now - heartbeat_ts > 20L * 1000) {
        // catch transition to inactive
        if (active) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Waiting for sim");
            display_off_ts = now + 30L * 1000;
        }

        active = 0;
    }

    get_message();

    kHz_encoder.tick();
    RotaryEncoder::Direction dir = kHz_encoder.getDirection();
    if (dir != RotaryEncoder::Direction::NOROTATION) {
        int steps = kHz_dial.steps(dir);
        dial_step(1, dir, steps);
    }

    mHz_encoder.tick();
    dir = mHz_encoder.getDirection();
    if (dir != RotaryEncoder::Direction::NOROTATION) {
        int steps = mHz_dial.steps(dir);
        dial_step(0, dir, steps);
    }

#ifdef TRIM_WHEEL
    trim_encoder.tick();
    dir = trim_encoder.getDirection();
    if (dir != RotaryEncoder::Direction::NOROTATION) {
        trim_dir = (dir == RotaryEncoder::Direction::CLOCKWISE) ? 'D' : 'U';
        send_message('T');
    }
#endif

    xfer_btn.update();
}
