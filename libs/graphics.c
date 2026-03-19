#include <dos.h>
#include <conio.h>
#include "graphics.h"
#include <stdlib.h>
#include <string.h>

extern union REGS regs;

void set_video_mode(int mode) {
    regs.h.ah = 0x00;     // функция установки режима
    regs.h.al = mode;      // 0x13 = 320x200x256
    int86(0x10, &regs, &regs);
}

int get_video_mode(void) {
    regs.h.ah = 0x0F;
    int86(0x10, &regs, &regs);
    return regs.h.al;
}

int get_page(void) {
    regs.h.ah = 0x0F;
    int86(0x10, &regs, &regs);
    return regs.h.bh;
}

// Сохранить область экрана в буфер (простой вариант)
void save_area(int x, int y, int w, int h, char far* buffer) {
    char far* video = (char far*)0xA0000000L;
    int i, j;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            buffer[i*w + j] = video[(y+i)*320 + (x+j)];
        }
    }
}

// Восстановить область из буфера
void restore_area(int x, int y, int w, int h, char far* buffer) {
    char far* video = (char far*)0xA0000000L;
    int i, j;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            video[(y+i)*320 + (x+j)] = buffer[i*w + j];
        }
    }
}

void draw_pixel(int x, int y, char color) {
    char far *video = (char far *)0xA0000000L;  // сегмент:смещение видеопамяти
    if (x < 0 || x >= 320 || y < 0 || y >= 200) return;
    video[y * 320 + x] = color;
}

void draw_filled_rect(int x, int y, int w, int h, char color){
    int i, o;

    if (w <= 0 || h <= 0) return;

    for (i = y; i < y+h; ++i)
    {
        for (o = x; o < x+w; ++o)
        {
            draw_pixel(o,i,color);
        }
    }
}

void draw_rect(int x, int y, int w, int h, char color){
    int i, o;

    if (w <= 0 || h <= 0) return;

    for (i = y; i < y+h; ++i)
    {
        for (o = x; o < x+w; ++o)
        {
            if(i==y || i==y+h-1){
                draw_pixel(o,i,color);
            }else{
                if(o==x || o==x+w-1) {draw_pixel(o,i,color);}
            }
        }
    }
}

// Вывод символа через BIOS (уже есть в draw_text, но сделаем отдельно)
void draw_char(int x, int y, char c, char color) {
    // Устанавливаем позицию курсора (примерно)
    regs.h.ah = 0x02;
    regs.h.bh = 0;
    regs.h.dh = y / 8;
    regs.h.dl = x / 8;
    int86(0x10, &regs, &regs);
    
    // Выводим символ
    regs.h.ah = 0x0E;
    regs.h.al = c;
    regs.h.bl = color;
    regs.h.bh = 0;
    int86(0x10, &regs, &regs);
}

// Строка (обертка над draw_text)
void draw_string(int x, int y, char* str, char color) {
    union REGPACK r;
    int len;

    if (!str) return;
    len = (int)strlen(str);
    if (len <= 0) return;

    // INT 10h / AH=13h: Write String (намного быстрее, чем позиционировать/печатать по символу)
    // DH/DL = row/col, ES:BP = строка, CX = длина, BL = цвет.
    r.h.ah = 0x13;
    r.h.al = 0x00;   // не обновлять курсор, атрибут в BL
    r.h.bh = 0x00;   // page
    r.h.bl = color;
    r.w.cx = len;
    r.h.dh = (unsigned char)(y / 8);
    r.h.dl = (unsigned char)(x / 8);
    r.w.es = FP_SEG(str);
    r.w.bp = FP_OFF(str);

    intr(0x10, &r);
}

// Рисование линии (алгоритм Брезенхема - упрощенный)
void draw_line(int x1, int y1, int x2, int y2, char color) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps, k;
    float x_inc, y_inc;
    float x = x1, y = y1;
    
    if (abs(dx) > abs(dy)) steps = abs(dx);
    else steps = abs(dy);
    
    x_inc = (float)dx / (float)steps;
    y_inc = (float)dy / (float)steps;
    
    for (k = 0; k <= steps; k++) {
        draw_pixel((int)(x + 0.5), (int)(y + 0.5), color);
        x += x_inc;
        y += y_inc;
    }
}

// Рисование окружности (алгоритм Брезенхема)
void draw_circle(int xc, int yc, int r, char color) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    
    while (x <= y) {
        draw_pixel(xc + x, yc + y, color);
        draw_pixel(xc - x, yc + y, color);
        draw_pixel(xc + x, yc - y, color);
        draw_pixel(xc - x, yc - y, color);
        draw_pixel(xc + y, yc + x, color);
        draw_pixel(xc - y, yc + x, color);
        draw_pixel(xc + y, yc - x, color);
        draw_pixel(xc - y, yc - x, color);
        
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

// Залитый круг
void draw_filled_circle(int xc, int yc, int r, char color) {
    int x, y;
    for (y = -r; y <= r; y++) {
        for (x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                draw_pixel(xc + x, yc + y, color);
            }
        }
    }
}

char get_color(int x, int y) {
    regs.h.ah = 0x0D;
    regs.x.cx = x;
    regs.x.dx = y;
    int86(0x10, &regs, &regs);
    return regs.h.al;
}

void clear(int color) {
    if(get_video_mode() != 0x13) return;
    // Заполняем весь экран (320*200 = 64000 пикселей)
    draw_filled_rect(0,0,320,200,color);
}

void clear_text(void){
    if(get_video_mode()==0x13) return;
    regs.h.ah = 0x06;      // прокрутка вверх
    regs.h.al = 0;         // 0 = очистить все
    regs.h.bh = 0x07;      // атрибут (серый на черном)
    regs.h.ch = 0; regs.h.cl = 0; // верхний левый угол
    regs.h.dh = 24; regs.h.dl = 79; // нижний правый угол
    int86(0x10, &regs, &regs);
}
