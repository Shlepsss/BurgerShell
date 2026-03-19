#ifndef GRAPHICS_H
#define GRAPHICS_H

void set_video_mode(int mode);

int get_video_mode(void);

int get_page(void);

void save_area(int x, int y, int w, int h, char far* buffer);

void restore_area(int x, int y, int w, int h, char far* buffer);

void draw_pixel(int x, int y, char color);

void draw_filled_rect(int x, int y, int w, int h, char color);

void draw_rect(int x, int y, int w, int h, char color);

void draw_char(int x, int y, char c, char color);

void draw_string(int x, int y, char* str, char color);

void draw_line(int x1, int y1, int x2, int y2, char color);

void draw_circle(int xc, int yc, int r, char color);

void draw_filled_circle(int xc, int yc, int r, char color);

char get_color(int x, int y);

void clear(int color);

void clear_text(void);

#endif
