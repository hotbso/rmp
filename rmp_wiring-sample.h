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

// define your wiring here

// LCD I2C
constexpr int lcd_I2C = 0x27;

// encoder digital pins
constexpr int kHz_encoder_pin1 = 12;
constexpr int kHz_encoder_pin2 = 11;
constexpr int mHz_encoder_pin1 = 9;
constexpr int mHz_encoder_pin2 = 8;
constexpr int xfer_button_bin_pin = 10;

#define TRIM_WHEEL 1

#ifdef TRIM_WHEEL
constexpr int trim_encoder_pin1 = 6;
constexpr int trim_encoder_pin2 = 7;
#endif