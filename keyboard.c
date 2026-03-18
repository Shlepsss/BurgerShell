#include <dos.h>
#include <conio.h>
#include "keyboard.h"

extern union REGS regs;

char wait_key(void) {
    regs.h.ah = 0x00;
    int86(0x16, &regs, &regs);  // ждем клавишу
    return regs.h.al;
}

// Прочитать клавишу, если есть (иначе 0)
char get_key(void) {
    union REGPACK r;
    
    r.h.ah = 0x01;           // функция проверки
    intr(0x16, &r);
    
    // В REGPACK есть поле flags!
    if ((r.w.flags & INTR_ZF) == 0) {  // ZF=0 - есть клавиша
        r.h.ah = 0x00;       // читаем клавишу
        intr(0x16, &r);
        return r.h.al;
    }
    return 0;
}

unsigned char get_scan_code(void) {
    regs.h.ah = 0x00;
    int86(0x16, &regs, &regs);
    return regs.h.ah;  // AH содержит скан-код
}

// Проверить модификаторы (Shift, Ctrl, Alt)
int get_modifiers(void) {
    regs.h.ah = 0x02;
    int86(0x16, &regs, &regs);
    return regs.h.al;  // Биты: 0=прав.Shift, 1=лев.Shift, 2=Ctrl, 3=Alt и т.д.
}
