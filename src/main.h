/**
 * \file main.h
 * \brief Main header file that contains the declaration of shared data structures and input handling
 */
#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <ncurses.h>
#include <time.h>

// forward declarations to avoid circular dependencies with the headers where these types are defined
typedef struct mem_data_t Mem_data_t;
typedef struct cpu_data_t CPU_data_t;
typedef struct tasklist TaskList;

// json menu description file path
#define JSON_MENUFILE "menus.json"
// base buffer size
#define BUF_BASESZ 128

struct taskmgr_data_t {
    // windows displaying data fetched
    WINDOW *memwin;
    WINDOW *cpuwin;
    WINDOW *procwin;
    // data structures holding data to be displayed
    Mem_data_t *mem_stats;
    CPU_data_t *cpu_stats;
    TaskList *tasks;
    // flag to be set to display raw data reads, instead of scaled ones
    int rawdata;
};

// Utility functions: see utilities.c

// stops the given timer and saves the settings in oldval
void stop_timer(timer_t timerid, struct itimerspec *oldval);
// prints the menu with supplied keybindings, items and descriptions
int print_menu(int *keybinds, char **items, char **descriptions, const int nitems);
int isNumber(const char* s, long* n);
// reads a pattern from the window win at the location supplied with a prompt
char* read_pattern(WINDOW *win, const int row, const int col, const char *prompt);
// read and find a pattern in the tasklist
void find_pattern(TaskList *tasks);
// read the PID and try to kill a process in the tasklist
void kill_process(TaskList *tasks);

// thread handling signals
void *signal_thread(void *param);

#endif
