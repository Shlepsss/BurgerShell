#ifndef MOUSE_H
#define MOUSE_H

int init_mouse(void);                          // инициализация + настройка

void set_mouse_borders(int min_x, int max_x, int min_y, int max_y);

void mouse_get_state(int* x, int* y, int* buttons);  // в пикселях

void mouse_show(void);

void mouse_hide(void);

void set_mouse_position(int x, int y);

int get_mouse_x(void);

int get_mouse_y(void);

int get_mouse_buttons(void);

void set_mouse_buttons(int left, int right, int middle);

#endif
