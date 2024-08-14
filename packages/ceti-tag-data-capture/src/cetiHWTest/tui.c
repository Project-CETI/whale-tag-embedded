//-----------------------------------------------------------------------------
// Project:      CETI Hardware Test Application
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "tui.h"

#include <stdint.h> // uint32_t
#include <stdio.h> // printf()
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>


static struct termios og_termios;
static int cols = 80;
static int lines = 24;

/**
 * @brief place cursor at position
 * 
 * @param x 
 * @param y 
 */
void tui_goto(int x, int y) {
    printf("\e[%d;%dH", y, x);
}

/**
 * @brief Draws 44 x 7 character ANSI version of the tag if there is enough
 * screen space  
 * 
 * @param x 
 * @param y 
 */

void tui_draw_tag_ascii(int x, int y) {
/*   /¯¯\ STARBOARD  /¯¯\
 *  ( ,'O¯¯¯¯¯¯¯O¯¯¯|¯¯¯¯¯¯¯¯```--.
 *   \| [__]      .'               \ __________
 *    |           |                 |__________
 *   /|            '|              /
 *  ( '.©_______O___|________...--·	
 *   \__/ PORT       \__/
 */
    // if (cols - x < 44) 
        // return;

    // if (lines - y < 7)
        // return;
    //  printf("\e[%d;%dH TAG\n", y, x);
    printf("\e[%d;%dH /¯¯\\            /¯¯\\                      ", y, x);
    printf("\e[%d;%dH( ,'O¯¯¯¯¯¯¯O¯¯¯|¯¯¯¯¯¯¯¯```--.            ", y + 1, x);
    printf("\e[%d;%dH \\| [__]      .'               \\ __________", y + 2, x);
    printf("\e[%d;%dH  |           |                 |__________", y + 3, x);
    printf("\e[%d;%dH /|            '|              /           ", y + 4, x);
    printf("\e[%d;%dH( '.©_______O___|________...--·            ", y + 5, x);
    printf("\e[%d;%dH \\__/            \\__/                      ", y + 6, x);

}

void tui_draw_inv_horzontal_bar(double value, double val_min, int x, int y, int w){
    printf("\e[%d;%dH", y, (uint32_t)(x + w*(1.0 - (value/val_min))));
    printf("\e[106m");
    for(int i = w*(1.0 - (value/val_min)); i < w; i++){
        printf(" ");

    }
    printf("\e[0m\e[0K");
}

void tui_draw_horzontal_bar(double value, double val_max, int x, int y, int w){
    printf("\e[%d;%dH", y, x);
    printf("\e[106m");
    for(int i = 0; i < w*value/val_max; i++){
        printf(" ");
    }
    printf("\e[0m\e[0K");
}


int tui_get_screen_width(void) { return cols; }
int tui_get_screen_height(void) { return lines; }

/**
 * @brief initialize terminal user interface. Automatically sets up tui_terminate()
 * to be called at exit
 * 
 */
void tui_initialize(void) {
    tcgetattr(STDIN_FILENO, &og_termios);
    atexit(tui_terminate);

    struct termios raw = og_termios;
    raw.c_iflag &= ~(ICRNL | IXON);
    // raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    fputs("\e[?25l", stdout); //hide cursor
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void tui_terminate(void) {
    fputs("\e[?25h", stdout); //show cursor
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_termios);
}

int tui_update_screen_size(void) {
    int screen_size_change = 0;
    #ifdef TIOCGSIZE
        struct ttysize ts;
        ioctl(STDIN_FILENO, TIOCGSIZE, &ts);
        screen_size_change = (cols != ts.ts_cols) || (lines != ts.ts_lines);
        cols = ts.ts_cols;
        lines = ts.ts_lines;
    #elif defined(TIOCGWINSZ)
        struct winsize ts;
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
        screen_size_change = (cols != ts.ws_col) || (lines != ts.ws_row);
        cols = ts.ws_col;
        lines = ts.ws_row;
    #endif /* TIOCGSIZE */
    return screen_size_change;
}