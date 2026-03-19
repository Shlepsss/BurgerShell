#include <dos.h>
#include <i86.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs\fs.h"
#include "libs\graphics.h"
#include "libs\keyboard.h"
#include "libs\mouse.h"
#include "libs\sound.h"
#include "libs\time.h"
#include "libs\utils.h"

union REGS regs;

#define SCR_W 320
#define SCR_H 200
#define CELL  8

#define TOPBAR_H 16
#define BOTBAR_H 16

#define MAX_FILES 64
#define EDIT_BUF  4096

// Важно: текст рисуется BIOS-ом в режиме 13h, поэтому координаты квантуются по 8px (столбцы/строки).
// Любые смещения меньше 8px могут "не влиять", а при переходе порога 8px будет скачок на 1 символ.
#define TEXT_DX 0
#define TEXT_DY 0

typedef enum {
    APP_NONE = 0,
    APP_FILES = 1,
    APP_EDITOR = 2,
    APP_CALC = 3,
    APP_ABOUT = 4,
    APP_SNAKE = 5
} AppKind;

typedef struct {
    int running;
    int mouse_present;
    int last_second;
    int prev_buttons;

    AppKind app;
    int needs_full_redraw;

    char status[41];

    char file_list[MAX_FILES][13];
    int file_count;
    int file_selected;
    int file_scroll;

    char editor_filename[13];
    char editor_buf[EDIT_BUF];
    int editor_len;
    int editor_dirty;
    int editor_scroll_line;

    char calc_input[64];
    int calc_len;
    char calc_result[32];

    // Snake game state
    int snake_active;
    int snake_dir;           // 0=U 1=R 2=D 3=L
    int snake_len;
    int snake_score;
    unsigned long snake_last_tick;
    int snake_food_x;
    int snake_food_y;
    int snake_body_x[200];
    int snake_body_y[200];
} ShellState;

static unsigned char far* g_vram = (unsigned char far*)0xA0000000L;
static unsigned char far* g_bg = 0;

static int point_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static void ui_string(int x, int y, const char* str, char color) {
    // см. комментарий про квантизацию: выравниваем к 8px сетке, чтобы поведение было предсказуемым
    int gx = (x + TEXT_DX) & ~7;
    int gy = (y + TEXT_DY) & ~7;
    draw_string(gx, gy, (char*)str, color);
}

static void build_background(void) {
    int x, y;

    if (g_bg) return;
    g_bg = (unsigned char far*)_fmalloc(64000UL);
    if (!g_bg) return;

    for (y = 0; y < SCR_H; y++) {
        for (x = 0; x < SCR_W; x++) {
            unsigned char c = BLACK;
            if (y >= TOPBAR_H && y < SCR_H - BOTBAR_H) {
                c = (unsigned char)(((x + y) / 8) & 15);
            }
            g_bg[(unsigned long)y * SCR_W + x] = c;
        }
    }
}

static void draw_time_top_right(void) {
    unsigned char h, m, s;
    char time_str[9];

    h = get_hours();
    m = get_minutes();
    s = get_seconds();

    time_str[0] = '0' + (h / 10);
    time_str[1] = '0' + (h % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (m / 10);
    time_str[4] = '0' + (m % 10);
    time_str[5] = ':';
    time_str[6] = '0' + (s / 10);
    time_str[7] = '0' + (s % 10);
    time_str[8] = '\0';

    draw_filled_rect(SCR_W - 8 * 8 - 6, 2, 8 * 8 + 8, 12, BLACK);
    ui_string(SCR_W - 8 * 8, 4, time_str, WHITE);
}

static void draw_top_bar(void) {
    draw_filled_rect(0, 0, SCR_W, TOPBAR_H, DARK_GRAY);
    draw_rect(0, 0, SCR_W, TOPBAR_H, WHITE);

    ui_string(8, 4, "Files", WHITE);
    ui_string(8 + 8 * 6, 4, "Calc", WHITE);
    ui_string(8 + 8 * 11, 4, "About", WHITE);
    ui_string(8 + 8 * 17, 4, "Game", WHITE);
    ui_string(8 + 8 * 22, 4, "Pwr", YELLOW);
    ui_string(8 + 8 * 26, 4, "Exit", LIGHT_RED);

    draw_time_top_right();
}

static void draw_bottom_bar(ShellState* st) {
    draw_filled_rect(0, SCR_H - BOTBAR_H, SCR_W, BOTBAR_H, DARK_GRAY);
    draw_rect(0, SCR_H - BOTBAR_H, SCR_W, BOTBAR_H, WHITE);

    draw_filled_rect(2, SCR_H - BOTBAR_H + 2, SCR_W - 4, BOTBAR_H - 4, BLACK);
    ui_string(4, SCR_H - BOTBAR_H + 4, st->status, LIGHT_CYAN);
}

static void set_status(ShellState* st, const char* msg) {
    int i;
    if (!msg) msg = "";
    for (i = 0; i < 40; i++) st->status[i] = msg[i] ? msg[i] : '\0';
    st->status[40] = '\0';
}

static void draw_window(int x, int y, int w, int h, const char* title) {
    draw_filled_rect(x, y, w, h, GRAY);
    draw_rect(x, y, w, h, WHITE);

    draw_filled_rect(x, y, w, 12, BLUE);
    draw_rect(x, y, w, 12, WHITE);

    if (title) ui_string(x + 6, y + 2, title, WHITE);

    // Close button [X]
    draw_filled_rect(x + w - 14, y + 2, 12, 8, RED);
    draw_rect(x + w - 14, y + 2, 12, 8, WHITE);
    ui_string(x + w - 12, y + 2, "X", WHITE);
}

static int window_close_hit(int mx, int my, int x, int y, int w, int h) {
    (void)h;
    return point_in_rect(mx, my, x + w - 14, y + 2, 12, 8);
}

static void app_files_refresh(ShellState* st) {
    st->file_count = fs_list_txt_files(st->file_list, MAX_FILES);
    st->file_selected = 0;
    st->file_scroll = 0;
}

static void editor_open_file(ShellState* st, const char* name) {
    int got;

    strncpy(st->editor_filename, name ? name : "NEW.TXT", 12);
    st->editor_filename[12] = '\0';

    st->editor_len = 0;
    st->editor_buf[0] = '\0';
    st->editor_dirty = 0;
    st->editor_scroll_line = 0;

    if (!name) return;

    got = fs_load_text(name, st->editor_buf, EDIT_BUF);
    if (got < 0) {
        st->editor_len = 0;
        st->editor_buf[0] = '\0';
        st->editor_dirty = 0;
        sound_error();
        set_status(st, "Open failed");
        return;
    }
    st->editor_len = got;
    set_status(st, "Opened");
}

static int editor_line_count(ShellState* st) {
    int i, lines = 1;
    for (i = 0; i < st->editor_len; i++) {
        if (st->editor_buf[i] == '\n') lines++;
    }
    return lines;
}

static void editor_append_char(ShellState* st, char c) {
    if (st->editor_len >= EDIT_BUF - 2) return;
    st->editor_buf[st->editor_len++] = c;
    st->editor_buf[st->editor_len] = '\0';
    st->editor_dirty = 1;
}

static void editor_backspace(ShellState* st) {
    if (st->editor_len <= 0) return;
    st->editor_len--;
    st->editor_buf[st->editor_len] = '\0';
    st->editor_dirty = 1;
}

static void editor_save(ShellState* st) {
    if (fs_save_text(st->editor_filename, st->editor_buf, st->editor_len) == 0) {
        st->editor_dirty = 0;
        sound_click();
        set_status(st, "Saved (Ctrl+S)");
    } else {
        sound_error();
        set_status(st, "Save failed");
    }
}

static void draw_editor(ShellState* st) {
    int wx = 16, wy = TOPBAR_H + 8, ww = SCR_W - 32, wh = SCR_H - TOPBAR_H - BOTBAR_H - 16;
    int i, row, col;
    int start_line = st->editor_scroll_line;
    int cur_line = 0;

    draw_window(wx, wy, ww, wh, "Editor");

    draw_filled_rect(wx + 2, wy + 14, ww - 4, 10, BLACK);
    ui_string(wx + 6, wy + 14, st->editor_filename, YELLOW);
    ui_string(wx + 6 + 8 * 14, wy + 14, st->editor_dirty ? "*" : " ", LIGHT_RED);
    ui_string(wx + 6 + 8 * 12, wy + 14, "Ctrl+S save, ESC close", LIGHT_CYAN);

    draw_filled_rect(wx + 2, wy + 26, ww - 4, wh - 28, BLACK);

    row = 0;
    col = 0;
    for (i = 0; i < st->editor_len; i++) {
        char c = st->editor_buf[i];
        if (c == '\r') continue;

        if (cur_line < start_line) {
            if (c == '\n') cur_line++;
            continue;
        }

        if (c == '\n') {
            row++;
            col = 0;
            cur_line++;
        } else {
            if (row >= (wh - 34) / CELL) break;
            if (col < (ww - 12) / CELL) {
                char s[2];
                s[0] = c;
                s[1] = '\0';
                ui_string(wx + 6 + col * CELL, wy + 28 + row * CELL, s, WHITE);
            }
            col++;
        }
    }

    {
        char info[40];
        sprintf(info, "Lines:%d  Scroll:%d", editor_line_count(st), st->editor_scroll_line);
        draw_filled_rect(wx + ww - 8 * 18 - 6, wy + wh - 10, 8 * 18 + 4, 8, GRAY);
        ui_string(wx + ww - 8 * 18 - 4, wy + wh - 10, info, BLUE);
    }
}

static void draw_files(ShellState* st) {
    int wx = 24, wy = TOPBAR_H + 16, ww = SCR_W - 48, wh = SCR_H - TOPBAR_H - BOTBAR_H - 32;
    int list_x = wx + 6, list_y = wy + 26;
    int list_rows = (wh - 36) / CELL;
    int i;

    draw_window(wx, wy, ww, wh, "Files (*.TXT)");
    ui_string(wx + 6, wy + 14, "Click file or Enter. ESC close.", LIGHT_CYAN);

    draw_filled_rect(wx + 2, wy + 26, ww - 4, wh - 28, BLACK);

    if (st->file_count <= 0) {
        ui_string(list_x, list_y, "No .TXT files in current dir", LIGHT_RED);
        return;
    }

    for (i = 0; i < list_rows; i++) {
        int idx = st->file_scroll + i;
        int y = list_y + i * CELL;
        if (idx >= st->file_count) break;

        if (idx == st->file_selected) {
            draw_filled_rect(wx + 4, y, ww - 8, CELL, BLUE);
            ui_string(list_x, y, st->file_list[idx], WHITE);
        } else {
            ui_string(list_x, y, st->file_list[idx], GRAY);
        }
    }

    if (st->file_count > list_rows) {
        char s[24];
        sprintf(s, "%d/%d", st->file_selected + 1, st->file_count);
        draw_filled_rect(wx + ww - 8 * 6 - 6, wy + 14, 8 * 6 + 4, 8, GRAY);
        ui_string(wx + ww - 8 * 6 - 4, wy + 14, s, BLUE);
        ui_string(wx + ww - 8 * 1 - 10, list_y, "^", WHITE);
        ui_string(wx + ww - 8 * 1 - 10, list_y + (list_rows - 1) * CELL, "v", WHITE);
    }
}

static int files_row_from_mouse(ShellState* st, int mx, int my) {
    int wx = 24, wy = TOPBAR_H + 16, ww = SCR_W - 48, wh = SCR_H - TOPBAR_H - BOTBAR_H - 32;
    int list_x = wx + 6, list_y = wy + 26;
    int list_rows = (wh - 36) / CELL;
    int row;

    if (!point_in_rect(mx, my, wx, wy, ww, wh)) return -1;
    if (!point_in_rect(mx, my, wx + 2, list_y, ww - 4, wh - 28)) return -1;

    row = (my - list_y) / CELL;
    if (row < 0 || row >= list_rows) return -1;
    return row;
}

static void calc_reset(ShellState* st) {
    st->calc_len = 0;
    st->calc_input[0] = '\0';
    st->calc_result[0] = '\0';
}

static void calc_compute(ShellState* st) {
    char* p = st->calc_input;
    char* end1;
    long a, b;
    char op = 0;
    long r = 0;

    while (*p == ' ') p++;
    a = strtol(p, &end1, 10);
    p = end1;
    while (*p == ' ') p++;
    op = *p;
    if (op == 0) {
        strcpy(st->calc_result, "ERR");
        return;
    }
    p++;
    while (*p == ' ') p++;
    b = strtol(p, &end1, 10);

    if (op == '+') r = a + b;
    else if (op == '-') r = a - b;
    else if (op == '*') r = a * b;
    else if (op == '/') {
        if (b == 0) {
            strcpy(st->calc_result, "DIV0");
            return;
        }
        r = a / b;
    } else {
        strcpy(st->calc_result, "ERR");
        return;
    }

    sprintf(st->calc_result, "%ld", r);
}

static void draw_calc(ShellState* st) {
    int wx = 48, wy = TOPBAR_H + 32, ww = SCR_W - 96, wh = 96;
    draw_window(wx, wy, ww, wh, "Calculator");

    ui_string(wx + 6, wy + 14, "Type problem then Enter.", LIGHT_CYAN);

    draw_filled_rect(wx + 6, wy + 30, ww - 12, 12, BLACK);
    ui_string(wx + 10, wy + 30, st->calc_input, WHITE);

    ui_string(wx + 6, wy + 54, "=", YELLOW);
    draw_filled_rect(wx + 18, wy + 52, ww - 24, 12, BLACK);
    ui_string(wx + 22, wy + 52, st->calc_result, LIGHT_GREEN);
}

static void draw_about(ShellState* st) {
    int wx = 40, wy = TOPBAR_H + 24, ww = SCR_W - 80, wh = 112;
    draw_window(wx, wy, ww, wh, "About");
    ui_string(wx + 6, wy + 16, "Burger Shell", WHITE);
    ui_string(wx + 6, wy + 28, st->mouse_present ? "Mouse found"  : "Mouse not found", st->mouse_present ? LIGHT_GREEN : LIGHT_RED);
    ui_string(wx + 6, wy + 44, "Running as DOS program", LIGHT_CYAN);
    ui_string(wx + 6, wy + 60, "ESC to close", YELLOW);
}

static unsigned long bios_ticks(void) {
    union REGS r;
    r.h.ah = 0x00;
    int86(0x1A, &r, &r);
    return ((unsigned long)r.x.cx << 16) | (unsigned long)r.x.dx;
}

static int snake_cell_occupied(ShellState* st, int x, int y) {
    int i;
    for (i = 0; i < st->snake_len; i++) {
        if (st->snake_body_x[i] == x && st->snake_body_y[i] == y) return 1;
    }
    return 0;
}

static void snake_place_food(ShellState* st, int gw, int gh) {
    int tries = 500;
    while (tries-- > 0) {
        int fx = rand() % gw;
        int fy = rand() % gh;
        if (!snake_cell_occupied(st, fx, fy)) {
            st->snake_food_x = fx;
            st->snake_food_y = fy;
            return;
        }
    }
    st->snake_food_x = -1;
    st->snake_food_y = -1;
}

static void snake_start(ShellState* st) {
    int i;
    int gw = 28, gh = 16;
    st->snake_active = 1;
    st->snake_dir = 1; // right
    st->snake_len = 5;
    st->snake_score = 0;
    st->snake_last_tick = bios_ticks();

    for (i = 0; i < st->snake_len; i++) {
        st->snake_body_x[i] = (gw / 2) - (st->snake_len - 1 - i);
        st->snake_body_y[i] = gh / 2;
    }

    srand((unsigned)(st->snake_last_tick ^ (unsigned long)get_seconds() ^ ((unsigned long)get_minutes() << 8)));
    snake_place_food(st, gw, gh);
}

static void snake_step(ShellState* st) {
    int gw = 28, gh = 16;
    int hx = st->snake_body_x[st->snake_len - 1];
    int hy = st->snake_body_y[st->snake_len - 1];
    int nx = hx, ny = hy;
    int ate = 0;
    int i;

    if (!st->snake_active) return;

    if (st->snake_dir == 0) ny--;
    else if (st->snake_dir == 1) nx++;
    else if (st->snake_dir == 2) ny++;
    else nx--;

    // wall collision
    if (nx < 0 || nx >= gw || ny < 0 || ny >= gh) {
        st->snake_active = 0;
        sound_error();
        set_status(st, "Snake: Game over (ESC)");
        return;
    }
    // self collision (allow moving into tail only if tail moves away, so check excluding tail)
    for (i = 1; i < st->snake_len; i++) {
        if (st->snake_body_x[i] == nx && st->snake_body_y[i] == ny) {
            st->snake_active = 0;
            sound_error();
            set_status(st, "Snake: Game over (ESC)");
            return;
        }
    }

    if (nx == st->snake_food_x && ny == st->snake_food_y) {
        ate = 1;
        st->snake_score += 10;
        sound_click();
    }

    if (ate) {
        if (st->snake_len < 200) {
            st->snake_body_x[st->snake_len] = nx;
            st->snake_body_y[st->snake_len] = ny;
            st->snake_len++;
        }
        snake_place_food(st, gw, gh);
    } else {
        // shift left (drop tail)
        for (i = 0; i < st->snake_len - 1; i++) {
            st->snake_body_x[i] = st->snake_body_x[i + 1];
            st->snake_body_y[i] = st->snake_body_y[i + 1];
        }
        st->snake_body_x[st->snake_len - 1] = nx;
        st->snake_body_y[st->snake_len - 1] = ny;
    }
}

static void draw_snake(ShellState* st) {
    int wx = 24, wy = TOPBAR_H + 12, ww = SCR_W - 48, wh = SCR_H - TOPBAR_H - BOTBAR_H - 24;
    int board_x = wx + 8;
    int board_y = wy + 28;
    int cell = 6;
    int gw = 28, gh = 16;
    int i;
    char info[40];

    draw_window(wx, wy, ww, wh, "Snake");
    ui_string(wx + 6, wy + 14, "WASD move, ESC close", LIGHT_CYAN);

    sprintf(info, "Score:%d", st->snake_score);
    ui_string(wx + ww - 8 * 12, wy + 14, info, YELLOW);

    draw_filled_rect(wx + 2, wy + 26, ww - 4, wh - 28, BLACK);

    // Board border
    draw_rect(board_x - 2, board_y - 2, gw * cell + 4, gh * cell + 4, GRAY);

    // Food
    if (st->snake_food_x >= 0) {
        draw_filled_rect(board_x + st->snake_food_x * cell, board_y + st->snake_food_y * cell, cell, cell, LIGHT_RED);
    }

    // Snake
    for (i = 0; i < st->snake_len; i++) {
        char c = (i == st->snake_len - 1) ? LIGHT_GREEN : GREEN;
        draw_filled_rect(board_x + st->snake_body_x[i] * cell, board_y + st->snake_body_y[i] * cell, cell, cell, c);
    }

    if (!st->snake_active) {
        ui_string(wx + 6, wy + wh - 16, "Game over", LIGHT_RED);
    }
}

static void redraw_all(ShellState* st) {
    // Чтобы не было "хвостов" курсора, прячем аппаратный/драйверный курсор
    // на время перерисовки и показываем снова.
    if (st->mouse_present) mouse_hide();

    if (g_bg) {
        _fmemcpy(g_vram, g_bg, 64000U);
    } else {
        clear(BLACK);
    }
    draw_top_bar();
    draw_bottom_bar(st);

    if (st->app == APP_FILES) draw_files(st);
    else if (st->app == APP_EDITOR) draw_editor(st);
    else if (st->app == APP_CALC) draw_calc(st);
    else if (st->app == APP_ABOUT) draw_about(st);
    else if (st->app == APP_SNAKE) draw_snake(st);

    if (st->mouse_present) mouse_show();
    st->needs_full_redraw = 0;
}

static void topbar_click(ShellState* st, int mx, int my) {
    if (!point_in_rect(mx, my, 0, 0, SCR_W, TOPBAR_H)) return;

    // Rough hitboxes based on text positions
    if (point_in_rect(mx, my, 8, 0, 8 * 5, TOPBAR_H)) {
        sound_click();
        st->app = APP_FILES;
        app_files_refresh(st);
        set_status(st, "Files: select .TXT");
        st->needs_full_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, 8 + 8 * 6, 0, 8 * 4, TOPBAR_H)) {
        sound_click();
        st->app = APP_CALC;
        calc_reset(st);
        set_status(st, "Calc: Enter to compute");
        st->needs_full_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, 8 + 8 * 11, 0, 8 * 5, TOPBAR_H)) {
        sound_click();
        st->app = APP_ABOUT;
        set_status(st, "About");
        st->needs_full_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, 8 + 8 * 17, 0, 8 * 4, TOPBAR_H)) {
        sound_click();
        st->app = APP_SNAKE;
        snake_start(st);
        set_status(st, "Snake: WASD, ESC close");
        st->needs_full_redraw = 1;
        return;
    }
    if (point_in_rect(mx, my, 8 + 8 * 22, 0, 8 * 3, TOPBAR_H)) {
        sound_click();
        set_status(st, "Powering off (APM)...");
        st->needs_full_redraw = 1;
        redraw_all(st);
        if (apm_poweroff() == 0) {
            st->running = 0;
        } else {
            sound_error();
            set_status(st, "APM not available (use Exit)");
            st->needs_full_redraw = 1;
        }
        return;
    }
    if (point_in_rect(mx, my, 8 + 8 * 26, 0, 8 * 4, TOPBAR_H)) {
        sound_click();
        st->running = 0;
        return;
    }
}

static void files_click(ShellState* st, int mx, int my) {
    int wx = 24, wy = TOPBAR_H + 16, ww = SCR_W - 48, wh = SCR_H - TOPBAR_H - BOTBAR_H - 32;
    int row, idx;

    if (window_close_hit(mx, my, wx, wy, ww, wh)) {
        sound_click();
        st->app = APP_NONE;
        set_status(st, "Desktop");
        st->needs_full_redraw = 1;
        return;
    }

    row = files_row_from_mouse(st, mx, my);
    if (row < 0) return;

    idx = st->file_scroll + row;
    if (idx < 0 || idx >= st->file_count) return;

    st->file_selected = idx;
    sound_click();
    editor_open_file(st, st->file_list[st->file_selected]);
    st->app = APP_EDITOR;
    set_status(st, "Editor: Ctrl+S save, ESC close");
    st->needs_full_redraw = 1;
}

static void editor_click(ShellState* st, int mx, int my) {
    int wx = 16, wy = TOPBAR_H + 8, ww = SCR_W - 32, wh = SCR_H - TOPBAR_H - BOTBAR_H - 16;
    if (window_close_hit(mx, my, wx, wy, ww, wh)) {
        sound_click();
        st->app = APP_NONE;
        set_status(st, "Desktop");
        st->needs_full_redraw = 1;
        return;
    }
}

static void calc_click(ShellState* st, int mx, int my) {
    int wx = 48, wy = TOPBAR_H + 32, ww = SCR_W - 96, wh = 96;
    if (window_close_hit(mx, my, wx, wy, ww, wh)) {
        sound_click();
        st->app = APP_NONE;
        set_status(st, "Desktop");
        st->needs_full_redraw = 1;
        return;
    }
}

static void about_click(ShellState* st, int mx, int my) {
    int wx = 40, wy = TOPBAR_H + 24, ww = SCR_W - 80, wh = 112;
    if (window_close_hit(mx, my, wx, wy, ww, wh)) {
        sound_click();
        st->app = APP_NONE;
        set_status(st, "Desktop");
        st->needs_full_redraw = 1;
        return;
    }
}

static void handle_click(ShellState* st, int mx, int my) {
    topbar_click(st, mx, my);
    if (!st->running) return;

    if (st->app == APP_FILES) files_click(st, mx, my);
    else if (st->app == APP_EDITOR) editor_click(st, mx, my);
    else if (st->app == APP_CALC) calc_click(st, mx, my);
    else if (st->app == APP_ABOUT) about_click(st, mx, my);
}

static void handle_key(ShellState* st, char key) {
    if (key == 0) return;

    // Global ESC closes current app
    if (key == 27) {
        if (st->app != APP_NONE) {
            st->app = APP_NONE;
            set_status(st, "Desktop");
            st->needs_full_redraw = 1;
            return;
        }
        set_status(st, "Use Exit button to quit");
        st->needs_full_redraw = 1;
        return;
    }

    if(!st->mouse_present){
        if(key == 87){set_mouse_position(get_mouse_x(),get_mouse_y() - 3);}
        if(key == 65){set_mouse_position(get_mouse_x() - 3,get_mouse_y());}
        if(key == 83){set_mouse_position(get_mouse_x(),get_mouse_y() + 3);}
        if(key == 68){set_mouse_position(get_mouse_x() + 3,get_mouse_y());}
        if(key == 64){set_mouse_buttons(1,0,0);}
        if(key == 35){set_mouse_buttons(0,1,0);}
        if(key == 36){set_mouse_buttons(0,0,1);}
    }
    

    if (key == 17 && st->app == APP_NONE) {
        sound_click();
        st->running = 0;
        return;
    }

    if (st->app == APP_FILES) {
        if (key == 'n' || key == 'N') {
            editor_open_file(st, 0);
            st->app = APP_EDITOR;
            set_status(st, "Editor: Ctrl+S save, ESC close");
            st->needs_full_redraw = 1;
            return;
        }
        if (key == 'j' || key == 'J') {
            if (st->file_selected + 1 < st->file_count) st->file_selected++;
            st->needs_full_redraw = 1;
        } else if (key == 'k' || key == 'K') {
            if (st->file_selected > 0) st->file_selected--;
            st->needs_full_redraw = 1;
        }

        {
            int wx = 24, wy = TOPBAR_H + 16, ww = SCR_W - 48, wh = SCR_H - TOPBAR_H - BOTBAR_H - 32;
            int list_rows = (wh - 36) / CELL;
            if (st->file_selected < st->file_scroll) st->file_scroll = st->file_selected;
            if (st->file_selected >= st->file_scroll + list_rows) st->file_scroll = st->file_selected - (list_rows - 1);
            if (st->file_scroll < 0) st->file_scroll = 0;
            if (st->file_scroll > st->file_count - list_rows) st->file_scroll = st->file_count - list_rows;
            if (st->file_scroll < 0) st->file_scroll = 0;
        }

        if (key == 13 && st->file_count > 0) {
            editor_open_file(st, st->file_list[st->file_selected]);
            st->app = APP_EDITOR;
            set_status(st, "Editor: Ctrl+S save, ESC close");
            st->needs_full_redraw = 1;
        }
        return;
    }

    if (st->app == APP_EDITOR) {
        if (key == '[') {
            if (st->editor_scroll_line > 0) st->editor_scroll_line--;
            st->needs_full_redraw = 1;
            return;
        }
        if (key == ']') {
            int max_scroll;
            int wx = 16, wy = TOPBAR_H + 8, ww = SCR_W - 32, wh = SCR_H - TOPBAR_H - BOTBAR_H - 16;
            int rows = (wh - 34) / CELL;
            max_scroll = editor_line_count(st) - rows;
            if (max_scroll < 0) max_scroll = 0;
            if (st->editor_scroll_line < max_scroll) st->editor_scroll_line++;
            st->needs_full_redraw = 1;
            return;
        }
        if (key == 19) { // Ctrl+S
            editor_save(st);
            st->needs_full_redraw = 1;
            return;
        }
        if (key == 8) {
            editor_backspace(st);
            st->needs_full_redraw = 1;
            return;
        }
        if (key == 13) {
            editor_append_char(st, '\r');
            editor_append_char(st, '\n');
            st->needs_full_redraw = 1;
            return;
        }
        if (key >= 32 && key <= 126) {
            editor_append_char(st, key);
            st->needs_full_redraw = 1;
            return;
        }
        return;
    }

    if (st->app == APP_CALC) {
        if (key == 8) {
            if (st->calc_len > 0) {
                st->calc_len--;
                st->calc_input[st->calc_len] = '\0';
                st->needs_full_redraw = 1;
            }
            return;
        }
        if (key == 13) {
            calc_compute(st);
            st->needs_full_redraw = 1;
            return;
        }
        if (key >= 32 && key <= 126) {
            if (st->calc_len < (int)sizeof(st->calc_input) - 1) {
                st->calc_input[st->calc_len++] = key;
                st->calc_input[st->calc_len] = '\0';
                st->needs_full_redraw = 1;
            }
            return;
        }
        return;
    }

    if (st->app == APP_SNAKE) {
        // WASD controls
        if (key == 'w' || key == 'W') { if (st->snake_dir != 2) st->snake_dir = 0; }
        else if (key == 'd' || key == 'D') { if (st->snake_dir != 3) st->snake_dir = 1; }
        else if (key == 's' || key == 'S') { if (st->snake_dir != 0) st->snake_dir = 2; }
        else if (key == 'a' || key == 'A') { if (st->snake_dir != 1) st->snake_dir = 3; }
        st->needs_full_redraw = 1;
        return;
    }
}

void main(void) {
    static ShellState st;
    int mx, my, mb;
    char key;
    unsigned long now_ticks;

    memset(&st, 0, sizeof(st));
    st.running = 1;
    st.last_second = -1;
    st.app = APP_NONE;
    st.needs_full_redraw = 1;
    set_status(&st, "Desktop");

    st.mouse_present = init_mouse();
    set_mouse_borders(0, 319, 0, 199);

    set_video_mode(0x13);
    build_background();
    if (st.mouse_present) mouse_show();

    while (st.running) {
        mouse_get_state(&mx, &my, &mb);

        if ((mb & 1) && !(st.prev_buttons & 1)) {
            handle_click(&st, mx, my);
        }
        st.prev_buttons = mb;

        key = get_key();
        if (key) handle_key(&st, key);

        if (st.app == APP_SNAKE) {
            now_ticks = bios_ticks();
            if ((unsigned long)(now_ticks - st.snake_last_tick) >= 3UL) { // ~6 updates/sec
                st.snake_last_tick = now_ticks;
                snake_step(&st);
                st.needs_full_redraw = 1;
            }
        }

        if (get_seconds() != st.last_second) {
            st.last_second = get_seconds();
            draw_time_top_right();
        }

        if (st.needs_full_redraw) redraw_all(&st);

        delay(20);
    }

    if (st.mouse_present) mouse_hide();
    set_video_mode(0x03);
    clear_text();

    if (g_bg) {
        _ffree(g_bg);
        g_bg = 0;
    }

    printf("Burger Shell was eaten.\r\n");
    printf("Burger advice you go to C: and start shell.exe\r\n");
}
