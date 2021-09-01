/**
 * \file windows.h
 * \brief Functions related to taskmanager windows update
 */
#ifndef WINDOW_H_INCLUDED
#define WINDOW_H_INCLUDED

#include "mem_info.h"
#include "cpu_info.h"
#include "process_info.h"

// lenght of the scale and progress bars drawn inside the windows
#define BARLEN 102
// lenght of a fixed buffer that should be long enough for output lines
#define LINE_MAXLEN 512

// an header that collects all functions dealing with windows
void mem_window_update(WINDOW *win, Mem_data_t *mem_usage, int scaling);
void cpu_window_update(WINDOW *win, CPU_data_t *cpu_usage);
void proc_window_update(WINDOW *win, TaskList *tasks);
// initializes the bar to empty (like this: "[        ]") and the scale to mark quarters
void init_bars(char *bar, char *scale);

#endif
