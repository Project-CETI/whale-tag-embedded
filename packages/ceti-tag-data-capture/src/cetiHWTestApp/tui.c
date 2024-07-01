//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab, Cummings Electronics Labs, 
//               MIT CSAIL
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "stdio.h" // printf()

static struct termios og_termios;
static int cols = 80;
static int lines = 24;

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

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void tui_terminate(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_termios);
}

void tui_update_screen_size(void) {
    #ifdef TIOCGSIZE
        struct ttysize ts;
        ioctl(STDIN_FILENO, TIOCGSIZE, &ts);
        cols = ts.ts_cols;
        lines = ts.ts_lines;
    #elif defined(TIOCGWINSZ)
        struct winsize ts;
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
        cols = ts.ws_col;
        lines = ts.ws_row;
    #endif /* TIOCGSIZE */
}