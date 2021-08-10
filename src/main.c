#include <unistd.h>

#include <glib.h>
#include <ncurses.h>

#include "mem_info.h"
#include "cpu_info.h"

#include "windows.h"

int main(int argc, char **argv) {
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();

    // create three different windows and box them
    WINDOW *memwin = newwin(LINES/4, COLS, 0, 0);
    WINDOW *cpuwin = newwin(LINES/4, COLS, LINES/4, 0);
    WINDOW *procwin = newwin(LINES/2, COLS, LINES/2, 0);

    // local vars used to store values to be printed
    CPU_data_t cpu_stats;
    memset(&cpu_stats, 0, sizeof(CPU_data_t));
    int core_count;

    Mem_data_t mem_stats;
    memset(&mem_stats, 0, sizeof(Mem_data_t));

    struct timespec delay;
    delay.tv_sec = 1;
    delay.tv_nsec = 0;

    while(1) {
        mem_window_update(memwin, &mem_stats);
        cpu_window_update(cpuwin, &cpu_stats, &core_count);
        proc_window_update(procwin);

        nanosleep(&delay, NULL);
    }

    delwin(memwin);
    endwin();

    return 0;
}

