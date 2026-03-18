#ifndef KEYBOARD_H
#define KEYBOARD_H

char wait_key(void);

// Прочитать клавишу, если есть (иначе 0)
char get_key(void);

unsigned char get_scan_code(void);

int get_modifiers(void);

#endif
