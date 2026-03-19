#ifndef UTILS_H
#define UTILS_H

#include <i86.h>

#define delay(ms) delay(ms)

// Цвета VGA (для удобства)
#define BLACK   0
#define BLUE    1
#define GREEN   2
#define CYAN    3
#define RED     4
#define MAGENTA 5
#define BROWN   6
#define GRAY    7
#define DARK_GRAY   8
#define LIGHT_BLUE  9
#define LIGHT_GREEN 10
#define LIGHT_CYAN  11
#define LIGHT_RED   12
#define LIGHT_MAGENTA 13
#define YELLOW   14
#define WHITE    15

// Скан-коды клавиш
#define KEY_ESC     1
#define KEY_F1      59
#define KEY_F2      60
#define KEY_F3      61
#define KEY_F4      62
#define KEY_F5      63
#define KEY_F6      64
#define KEY_F7      65
#define KEY_F8      66
#define KEY_F9      67
#define KEY_F10     68
#define KEY_UP      72
#define KEY_LEFT    75
#define KEY_RIGHT   77
#define KEY_DOWN    80

void beep(void);

#endif
