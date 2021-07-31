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
*/

#include <windows.h>
#include <stdio.h>
#include "rmpif.h"

static HANDLE hComm;

int port_open(const char *port)
{
    int len = strlen(port) + 4;
    char *port_full = alloca(len + 1);
    strcpy(port_full, "\\\\.\\");
    strcat(port_full, port);
    WCHAR *wc_port = alloca((len + 1) * sizeof(WCHAR));
    mbstowcs_s(NULL, wc_port, len + 1, port_full, _TRUNCATE);

    hComm = CreateFileW(wc_port,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        0,
                        OPEN_EXISTING,
                        0,
                        0);

    if (hComm == INVALID_HANDLE_VALUE) {
        log_msg("Can't open: %d\r\n", GetLastError());
        return -1;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState (hComm, &dcb)) {
        log_msg("Can't get commstate: %d\r\n", GetLastError());
        return -1;
    }

    dcb.BaudRate  = 115200;
    if (!SetCommState (hComm, &dcb)) {
        log_msg("Can't set commstate: %d\r\n", GetLastError());
        return -1;
    }

    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(hComm, &timeouts)) {
        log_msg("Invalid value: %d\r\n", GetLastError());
        return -1;
    }
    
    return 0;
}

int port_read(char *buffer, size_t len)
{
    DWORD dwBytesRead;
    if(!ReadFile(hComm, buffer, len, &dwBytesRead, NULL))
        return -1;

    return dwBytesRead;
}

int port_write(char *buffer, size_t len)
{
    DWORD dwBytesWritten;
    if(!WriteFile(hComm, buffer, len, &dwBytesWritten, NULL))
        return -1;

    return dwBytesWritten;
}


void port_log_error(const char *str)
{
    log_msg("port error (%s): %d\n", str, GetLastError());
}
