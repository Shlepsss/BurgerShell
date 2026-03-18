#include <dos.h>
#include <conio.h>
#include "mouse.h"

// Глобальные переменные для хранения состояния мыши
static int mouse_max_x = 319;
static int mouse_max_y = 199;
static int leftbut = 0;
static int rightbut = 0;
static int middlebut = 0;

extern union REGS regs;

// Инициализация мыши и автоматическая настройка под наш экран
int init_mouse(void) {
    regs.x.ax = 0x0000;
    int86(0x33, &regs, &regs);
    if(regs.x.ax == 0xFFFF) {return regs.x.bx;}
    else{return 0;}
}

// Установка границ движения мыши
void set_mouse_borders(int min_x, int max_x, int min_y, int max_y) {
    mouse_max_x = max_x;
    mouse_max_y = max_y;
    
    // Горизонтальные границы
    regs.x.ax = 0x0007;
    regs.x.cx = min_x * 2;
    regs.x.dx = max_x * 2;
    int86(0x33, &regs, &regs);
    
    // Вертикальные границы
    regs.x.ax = 0x0008;
    regs.x.cx = min_y;
    regs.x.dx = max_y;
    int86(0x33, &regs, &regs);
}

// Получить состояние мыши (с преобразованием в пиксели)
void mouse_get_state(int* x, int* y, int* buttons) {
    regs.x.ax = 0x0003;
    int86(0x33, &regs, &regs);
    
    // Функция 3 возвращает координаты в пикселях, если мы правильно настроили!
    *x = regs.x.cx / 2;
    *y = regs.x.dx;
    *buttons = (leftbut ? 1 : 0) | (rightbut ? 2 : 0) | (middlebut ? 4 : 0);
    
    leftbut = (regs.x.bx & 1) ? 1 : 0;

    rightbut = (regs.x.bx & 2) ? 1 : 0;

    middlebut = (regs.x.bx & 4) ? 1 : 0;
}

// Показать курсор
void mouse_show(void) {
    regs.x.ax = 0x0001;
    int86(0x33, &regs, &regs);
}

// Спрятать курсор
void mouse_hide(void) {
    regs.x.ax = 0x0002;
    int86(0x33, &regs, &regs);
}

// Установить позицию
void set_mouse_position(int x, int y) {
    if (x < 0) x = 0;
    if (x > mouse_max_x) x = mouse_max_x;
    if (y < 0) y = 0;
    if (y > mouse_max_y) y = mouse_max_y;
    
    regs.x.ax = 0x0004;
    regs.x.cx = x * 2;
    regs.x.dx = y;
    int86(0x33, &regs, &regs);
}

// Получить позицию мыши (просто X)
int get_mouse_x(void) {
    int x, y, buttons;
    mouse_get_state(&x, &y, &buttons);
    return x;
}

// Получить позицию мыши (просто Y)
int get_mouse_y(void) {
    int x, y, buttons;
    mouse_get_state(&x, &y, &buttons);
    return y;
}

// Получить состояние кнопок
int get_mouse_buttons(void) {
    int x, y, buttons;
    mouse_get_state(&x, &y, &buttons);
    return buttons;
}

void set_mouse_buttons(int left, int right, int middle) { leftbut = left; rightbut = right; middlebut = middle; }
