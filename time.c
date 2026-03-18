#include <dos.h>
#include <conio.h>
#include "time.h"

extern union REGS regs;

// Преобразовать BCD в обычное число
unsigned char bcd_to_byte(unsigned char bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Получить часы как обычное число (0-23)
unsigned char get_hours(void) {
    regs.h.ah = 0x02;
    int86(0x1A, &regs, &regs);
    return bcd_to_byte(regs.h.ch);
}

// Получить минуты
unsigned char get_minutes(void) {
    regs.h.ah = 0x02;
    int86(0x1A, &regs, &regs);
    return bcd_to_byte(regs.h.cl);
}

// Получить секунды
unsigned char get_seconds(void) {
    regs.h.ah = 0x02;
    int86(0x1A, &regs, &regs);
    return bcd_to_byte(regs.h.dh);
}

// Получить дату (нормальную)
void get_date_normal(int* day, int* month, int* year) {
    regs.h.ah = 0x04;
    int86(0x1A, &regs, &regs);
    *day = bcd_to_byte(regs.h.dl);
    *month = bcd_to_byte(regs.h.dh);
    *year = bcd_to_byte(regs.h.cl);
    // Век в CH, но нам пока не нужно
}
