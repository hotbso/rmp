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
#include "XPLMDataAccess.h"
#include "XPLMUtilities.h"
#include "XPLMProcessing.h"

#include "rmpif.h"

#define UNUSED(x) (void)(x)

#define VERSION "0.9a"

#define HEARTBEAT 5

static int dr_mapped;
static int error_disabled;

static char in_msg[100];
static int in_msg_ptr;
static XPLMDataRef com1_dr, com1_standby_dr;
static XPLMCommandRef trim_up_cmdr, trim_down_cmdr;
static char port[20];
static time_t heartbeat_ts;
static int last_com1_f, last_com1_sf;

static void process_msg(int len) {
    if (in_msg[0] == 'D') {
        log_msg(in_msg);
    } else if (in_msg[0] == 'S' && in_msg[len-1] == '_') {
        long f;
        if (1 == sscanf(in_msg, "S%6ld_", &f)) {
            log_msg("%ld", f);
            XPLMSetDatai(com1_standby_dr, f);
        } else {
            log_msg("invalid input ->%s<-", in_msg);
        }
    } else if (in_msg[0] == 'X' && in_msg[len-1] == '_') {
        long f, fs;
        if (2 == sscanf(in_msg, "X%6ld%6ld_", &f, &fs)) {
            log_msg("%ld %ld", f, fs);
            XPLMSetDatai(com1_dr, f);
            XPLMSetDatai(com1_standby_dr, fs);
        } else {
            log_msg("invalid input ->%s<-", in_msg);
        }
    } else if (0 == strcmp(in_msg, "TD_")) {
        /* discard */
    } else if (0 == strcmp(in_msg, "TU_")) {
        /* discard */
    } else {
        log_msg("invalid msg ->%s<- discarded", in_msg);
    }
}

static void
send_heartbeat(int com1_f, int com1_sf)
{
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "H%06d%06da\n", com1_f, com1_sf);
    last_com1_f = com1_f; last_com1_sf = com1_sf;
    //log_msg("send ->%s<-", buffer);

    int len = strlen(buffer);
    int n;
    if ((n = port_write(buffer, len)) < 0) {
        port_log_error("write");
        error_disabled = 1;
        return;
    }

    if (n != len) {
        log_msg("short write");
        error_disabled = 1;
        return;
    }
}

static float
flight_loop_cb(float elapsed_last_call,
               float elapsed_last_loop, int counter, void *in_refcon)
{
    float loop_delay = 0.2f;
    if (error_disabled)
        return 60.0;

    int com1_f = XPLMGetDatai(com1_dr);
    int com1_sf = XPLMGetDatai(com1_standby_dr);

    time_t now = time(NULL);
    if ((now > heartbeat_ts)
        || (com1_f != last_com1_f) || (com1_sf != last_com1_sf)) {
        send_heartbeat(com1_f, com1_sf);
        if (error_disabled)
            return 60.0;

        heartbeat_ts = now + 5;
    }

    char buffer[100];
    int n;
    if((n = port_read(buffer, sizeof(buffer))) < 0) {
        port_log_error("read");
        error_disabled = 1;
        loop_delay = 60.0;
        goto done;
    }

    if (n == 0)
        goto done;

    for (int i = 0; i < n; i++) {
        char c = buffer[i];
        if (c == '\r') continue;
        if (c == '\n') {
            in_msg[in_msg_ptr] = '\0';
            log_msg("->%s<-\n", in_msg);
            process_msg(in_msg_ptr);
            in_msg_ptr = 0;
            continue;
        }
        in_msg[in_msg_ptr++] = c;
        if (in_msg_ptr > sizeof(in_msg)) in_msg_ptr = 0;
    }

   done:
    return loop_delay;
}

static void
map_datarefs()
{
    if (dr_mapped)
        return;

    dr_mapped = 1;

    if (NULL == (com1_standby_dr = XPLMFindDataRef("sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833"))) goto err;
    if (NULL == (com1_dr = XPLMFindDataRef("sim/cockpit2/radios/actuators/com1_frequency_hz_833"))) goto err;
    if (NULL == (trim_down_cmdr = XPLMFindCommand("sim/flight_controls/pitch_trim_down"))) goto err;
    if (NULL == (trim_up_cmdr = XPLMFindCommand("sim/flight_controls/pitch_trim_up"))) goto err;
    return;

err:
    error_disabled = 1;
    log_msg("Can't map all datarefs, disabled");
}

//* ------------------------------------------------------ API -------------------------------------------- */
PLUGIN_API int
XPluginStart(char *out_name, char *out_sig, char *out_desc)
{
    log_msg("startup " VERSION);

    /* Always use Unix-native paths on the Mac! */
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    strcpy(out_name, "rmpif " VERSION);
    strcpy(out_sig, "rmpif-hotbso");
    strcpy(out_desc, "A plugin that connects to an Arduino based rmp");

    XPLMRegisterFlightLoopCallback(flight_loop_cb, 1.0f, NULL);
    return 1;
}


PLUGIN_API void
XPluginStop(void)
{
}


PLUGIN_API void
XPluginDisable(void)
{
}


PLUGIN_API int
XPluginEnable(void)
{
    char xpdir[512];
    const char *psep;
    char cfg_path[512];

    map_datarefs();
    if (! dr_mapped)
        return 0;

    psep = XPLMGetDirectorySeparator();
    XPLMGetSystemPath(xpdir);

    snprintf(cfg_path, sizeof cfg_path, "%sResources%splugins%srmpif%srmpif.cfg",
             xpdir, psep, psep, psep);
    FILE *f = fopen(cfg_path, "r");
    if (NULL == f) {
        log_msg("no rmpif.cfg in plugin dir");
        return 0;
    }

    fgets(port, sizeof(port), f);
    fclose(f);

    int n = strlen(port);
    if (port[n-1] == '\n') port[n-1] = '\0';
    log_msg("rmpif port is ->%s<-", port);

    if (port_open(port) >= 0) {
        log_msg("port opened");
    } else {
        port_log_error("open fail");
        error_disabled = 1; /* better be paranoid */
        return 0;
    }

    /* starts the flight loop */
    return 1;
}


PLUGIN_API void
XPluginReceiveMessage(XPLMPluginID in_from, long in_msg, void *in_param)
{
}
