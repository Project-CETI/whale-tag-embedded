//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef TUI_H
#define TUI_H

// ANSI macros
#define BOLD(str)      "\e[1m" str "\e[0m"
#define UNDERLINE(str) "\e[4m" str "\e[0m"
#define RED(str)       "\e[31m" str "\e[0m"
#define GREEN(str)     "\e[32m" str "\e[0m"
#define YELLOW(str)    "\e[33m" str "\e[0m"
#define CYAN(str)      "\e[96m" str "\e[0m"
#define CLEAR_SCREEN   "\e[1;1H\e[2J"

#define PASS           "PASS"
#define FAIL           "!!! FAIL !!!"

// functions
void tui_goto(int x, int y);
void tui_draw_tag_ascii(int x, int y);
void tui_draw_horzontal_bar(double value, double val_max, int x, int y, int w);
void tui_draw_inv_horzontal_bar(double value, double val_min, int x, int y, int w);
int  tui_get_screen_width(void);
int  tui_get_screen_height(void);
void tui_initialize(void);
void tui_terminate(void);
int tui_update_screen_size(void);

#endif //TUI_H
