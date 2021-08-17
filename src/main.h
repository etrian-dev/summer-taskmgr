/**
 * \file main.h
 * \brief Main header file that contains the declaration of shared data structures and input handling
 */
#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include "mem_info.h"
#include "cpu_info.h"
#include "process_info.h"

#include <ncurses.h>

// json menu description file path
#define JSON_MENUFILE "menus.json"

struct taskmgr_data_t {
    // windows displaying data fetched
    WINDOW *memwin;
    WINDOW *cpuwin;
    WINDOW *procwin;
    // data structures holding data to be displayed
    Mem_data_t *mem_stats;
    CPU_data_t *cpu_stats;
    TaskList *tasks;
};

// Utility functions: see utilities.c

// stops the given timer and saves the settings in oldval
void stop_timer(timer_t timerid, struct itimerspec *oldval);
// prints the menu with supplied keybindings, items and descriptions
int print_menu(int *keybinds, char **items, char **descriptions, const int nitems);

// thread handling signals
void *signal_thread(void *param);

#endif
