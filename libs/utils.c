#include <dos.h>
#include <conio.h>
#include "utils.h"

extern union REGS regs;

// Пищалка (через BIOS)
void beep(void) {
    regs.h.ah = 0x0E;
    regs.h.al = 7;  // ASCII BELL
    regs.h.bl = 15;
    regs.h.bh = 0;
    int86(0x10, &regs, &regs);
}
