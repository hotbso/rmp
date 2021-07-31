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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "XPLMPlugin.h"
#include "XPLMPlanes.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMDataAccess.h"
#include "XPLMUtilities.h"
#include "XPLMProcessing.h"
#include "XPLMMenus.h"
#include "XPWidgets.h"
#include "XPStandardWidgets.h"

#include "rmpif.h"

// sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833
// sim/cockpit2/radios/actuators/com1_frequency_hz_833

#define UNUSED(x) (void)(x)

#define VERSION "0.9a"

static char port[20] = "COM8";

static float flight_loop_cb(float unused1, float unused2, int unused3, void *unused4);

static char xpdir[512];
static const char *psep;
static char pref_path[512];

static void
save_pref()
{
    FILE *f = fopen(pref_path, "wb");
    if (NULL == f)
        return;

    fputs(port, f); putc('\n', f);
    fclose(f);
}


static void
load_pref()
{
    FILE *f  = fopen(pref_path, "rb");
    if (NULL == f)
        return;

    fgets(port, sizeof(port), f);
    int len = strlen(port);
    if ('\n' == port[len - 1]) port[len - 1] = '\0';
    fclose(f);
}

static void process_msg(char *msg, int len) {
    if (msg[0] == 'D') {
        log_msg(msg);
    } else if (msg[0] == 'S' && msg[len-1] == '_') {
        long f;
        if (1 == sscanf(msg, "S%6ld_", &f)) {
            log_msg("%ld", f);
        } else {
            log_msg("invalid input ->%s<-", msg);
        }
    }
}

#if 0
int main()
{
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)setvbuf(stderr, NULL, _IONBF, 0);
    
    if (port_open("COM8") < 0) {
        port_log_error("open");
        exit(1);
    }
    
    char line[100];
    int ptr = 0;

    while (1) {
        char buffer[100];
        int n;
        if((n = port_read(buffer, sizeof(buffer))) < 0) {
            port_log_error("read");
        }

        if (n == 0) {
            Sleep(200);
            continue;
        }

        for (int i = 0; i < n; i++) {
            char c = buffer[i];
            if (c == '\r') continue;
            if (c == '\n') {
                line[ptr] = '\0';
                log_msg("->%s<-\n", line);
                process_msg(line, ptr);
                ptr = 0;
                continue;
            }
            line[ptr++] = c;
            if (ptr > sizeof(line)) ptr = 0;
        }
    }
}
#endif

//* ------------------------------------------------------ API -------------------------------------------- */
PLUGIN_API int
XPluginStart(char *out_name, char *out_sig, char *out_desc)
{
    log_msg("startup " VERSION);

    /* Always use Unix-native paths on the Mac! */
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);

    strcpy(out_name, "rmpif " VERSION);
    strcpy(out_sig, "rmpif-hotbso");
    strcpy(out_desc, "A plugin that connects to an Arduino based rmp");

    psep = XPLMGetDirectorySeparator();
    XPLMGetSystemPath(xpdir);

    /* load preferences */
    XPLMGetPrefsPath(pref_path);
    XPLMExtractFileAndPath(pref_path);
    strcat(pref_path, psep);
    strcat(pref_path, "rmpif.prf");
    load_pref();
    return 1;
}


PLUGIN_API void
XPluginStop(void)
{
    save_pref();
}


PLUGIN_API void
XPluginDisable(void)
{
}


PLUGIN_API int
XPluginEnable(void)
{
    return 1;
}


PLUGIN_API void
XPluginReceiveMessage(XPLMPluginID in_from, long in_msg, void *in_param)
{
    UNUSED(in_from);
    switch (in_msg) {
        case XPLM_MSG_PLANE_LOADED:
#if 0
            XPLMMenuID menu = XPLMFindPluginsMenu();
            int sub_menu = XPLMAppendMenuItem(menu, "Simbrief Connector", NULL, 1);
            tlsb_menu = XPLMCreateMenu("Simbrief Connector", menu, sub_menu, menu_cb, NULL);
            XPLMAppendMenuItem(tlsb_menu, "Configure", &conf_widget, 0);
            XPLMAppendMenuItem(tlsb_menu, "Fetch OFP", &getofp_widget, 0);

            toggle_cmdr = XPLMCreateCommand("tlsb/toggle", "Toggle simbrief connector widget");
            XPLMRegisterCommandHandler(toggle_cmdr, toggle_cmd_cb, 0, NULL);

            fetch_cmdr = XPLMCreateCommand("tlsb/fetch", "Fetch ofp data and show in widget");
            XPLMRegisterCommandHandler(fetch_cmdr, fetch_cmd_cb, 0, NULL);

            fetch_xfer_cmdr = XPLMCreateCommand("tlsb/fetch_xfer", "Fetch ofp data and xfer load data");
            XPLMRegisterCommandHandler(fetch_xfer_cmdr, fetch_xfer_cmd_cb, 0, NULL);

            flight_loop_id = XPLMCreateFlightLoop(&create_flight_loop);
#endif
            break;
    }
}
