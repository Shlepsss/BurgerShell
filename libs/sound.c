#include "sound.h"

#include <dos.h>
#include <conio.h>

static void speaker_tone(unsigned int hz, unsigned int ms) {
    unsigned int divisor;
    unsigned char tmp;

    if (hz == 0 || ms == 0) return;

    divisor = (unsigned int)(1193180UL / (unsigned long)hz);

    outp(0x43, 0xB6);
    outp(0x42, (unsigned char)(divisor & 0xFF));
    outp(0x42, (unsigned char)((divisor >> 8) & 0xFF));

    tmp = (unsigned char)inp(0x61);
    outp(0x61, (unsigned char)(tmp | 3));

    delay(ms);

    tmp = (unsigned char)inp(0x61);
    outp(0x61, (unsigned char)(tmp & (unsigned char)~3));
}

void sound_click(void) {
    speaker_tone(1200, 12);
}

void sound_error(void) {
    speaker_tone(220, 60);
}

int apm_poweroff(void) {
    union REGS inr, outr;

    // APM installation check
    inr.x.ax = 0x5300;
    inr.x.bx = 0x0000;
    int86(0x15, &inr, &outr);
    if (outr.x.cflag) return -1;

    // Connect real mode interface (ignore errors on some BIOSes)
    inr.x.ax = 0x5301;
    inr.x.bx = 0x0000;
    int86(0x15, &inr, &outr);

    // Enable power management for "all devices" (0001 is system device in many BIOSes)
    inr.x.ax = 0x5308;
    inr.x.bx = 0x0001;
    inr.x.cx = 0x0001;
    int86(0x15, &inr, &outr);
    if (outr.x.cflag) return -1;

    // Set power state: off
    inr.x.ax = 0x5307;
    inr.x.bx = 0x0001;
    inr.x.cx = 0x0003;
    int86(0x15, &inr, &outr);
    if (outr.x.cflag) return -1;

    return 0;
}

