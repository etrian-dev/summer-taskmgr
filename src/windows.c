#include <stdio.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <ncurses.h>

#include "cpu_info.h"
#include "mem_info.h"
#include "process_info.h"
#include "windows.h"

// function that deals with meters included in the memory window
void mem_window_update(WINDOW *win, Mem_data_t *mem_usage) {
    werase(win);
    // a progress bar filled with a number of '#' correspondent to the percentage of
    // memory available in the system. A scale is drawn as well to mark the quarters of the bar
    char ram_bar[BARLEN];
    char swp_bar[BARLEN];
    char scale[BARLEN];
    init_bars(ram_bar, scale);
    init_bars(swp_bar, scale);
    char values[LINE_MAXLEN];

    int lines, cols;
    getmaxyx(win, lines, cols);
    int yoff = 1, xoff = 1;

    // about to access shared data: lock
    pthread_mutex_lock(&mem_usage->mux_memdata);
    while(mem_usage->is_busy == TRUE) {
        pthread_cond_wait(&mem_usage->cond_updating, &mem_usage->mux_memdata);
    }
    mem_usage->is_busy = TRUE;

    // set the memory percentage
    gfloat percent = 100.0 - (mem_usage->avail_mem * 100.0)/(float)mem_usage->total_mem;
    gulong quot = (unsigned long int)round(percent);
    memset(ram_bar + 1, '#', quot * sizeof(char));

    snprintf(values, LINE_MAXLEN,
             "Total: %.2f MiB\tAvailable: %.2f MiB\tFree: %.2f MiB\tBuff/Cached: %.2f MiB",
              (float)mem_usage->total_mem / 1024.0, (float)mem_usage->avail_mem / 1024.0,
              (float)mem_usage->free_mem / 1024.0, (float)mem_usage->buffer_cached / 1024.0);

    // insert the bar and values on the window
    mvwprintw(win, yoff++, xoff, "%s (%.3f%% in use)", ram_bar, percent);
    mvwaddstr(win, yoff++, xoff, scale);
    mvwprintw(win, yoff++, xoff, values);

    // does the same thing, but for swap (resets the bar first)
    percent = 100 - (mem_usage->swp_free * 100)/(float)mem_usage->swp_tot;
    memset(swp_bar + 1, '#', percent * sizeof(char));
    snprintf(values, LINE_MAXLEN,
             "Swap Total: %.2f MiB\tSwap Free: %.2f MiB",
             (float)mem_usage->swp_tot / 1024.0,
             (float)mem_usage->swp_free / 1024.0);

    mem_usage->is_busy = FALSE;
    pthread_cond_signal(&mem_usage->cond_updating);
    // shared data is not accessed now: unlock
    pthread_mutex_unlock(&mem_usage->mux_memdata);

    mvwprintw(win, yoff++, xoff, "%s (%.3f%% in use)", swp_bar, percent);
    mvwaddstr(win, yoff++, xoff, scale);
    mvwprintw(win, yoff++, xoff, values);

    wrefresh(win);
}
/// function that deals with meters included in the cpu window
void cpu_window_update(WINDOW *win, CPU_data_t *cpu_usage) {
    werase(win);
    // erase the window's contents
    char ln[LINE_MAXLEN];
    char ln2[LINE_MAXLEN];

    // about to access shared data: lock
    pthread_mutex_lock(&cpu_usage->mux_memdata);
    while(cpu_usage->is_busy == TRUE) {
        pthread_cond_wait(&cpu_usage->cond_updating, &cpu_usage->mux_memdata);
    }
    cpu_usage->is_busy = TRUE;

    snprintf(ln, LINE_MAXLEN, "Model: \'%s\'\tCores: %d", cpu_usage->model, cpu_usage->num_cores);

    snprintf(ln2, LINE_MAXLEN,
             "CPU%% usr: %.3f\tnice: %.3f\tsys: %.3f\tidle: %.3f",
              cpu_usage->perc_usr, cpu_usage->perc_usr_nice, cpu_usage->perc_sys, cpu_usage->perc_idle);

    cpu_usage->is_busy = FALSE;
    pthread_cond_signal(&cpu_usage->cond_updating);
    // shared data is not accessed now: unlock
    pthread_mutex_unlock(&cpu_usage->mux_memdata);

    mvwaddstr(win, 1, 1, ln);
    mvwaddstr(win, 2, 1, ln2);
    // refresh the window to display the contents
    wrefresh(win);
}

/// function that deals with meters included in the process list window
void proc_window_update(WINDOW *win, TaskList *tasks) {
    werase(win);

    int lines, cols;
    getmaxyx(win, lines, cols);
    int yoff = 1, xoff = 1;

    char ln[LINE_MAXLEN];
    char ln2[LINE_MAXLEN];

    // about to access shared data: lock
    pthread_mutex_lock(&tasks->mux_memdata);
    while(tasks->is_busy == TRUE) {
        pthread_cond_wait(&tasks->cond_updating, &tasks->mux_memdata);
    }
    tasks->is_busy = TRUE;

    // sort processes based on the function indicated at runtime
    g_array_sort(tasks->ps, tasks->sortfun);

    snprintf(ln, LINE_MAXLEN, "processes: %ld\tthreads: %ld", tasks->num_ps, tasks->num_threads);
    snprintf(ln2, LINE_MAXLEN,
        "%-10s %-10s %-20s %-5s %-5s %-10s %-10s %-10s",
        "PID", "PPID", "USER", "STATE", "NICE", "THREADS", "VSZ (GiB)", "CMD");

    mvwaddstr(win, yoff++, xoff, ln);
    mvwaddstr(win, yoff++, xoff, ln2);

    for(int i = yoff; i < lines - 1; i++) {
        char procline[LINE_MAXLEN];
        Task *t = &(g_array_index(tasks->ps, Task, i));
        if(t->visible == TRUE) {
            snprintf(procline, LINE_MAXLEN,
                     "%-10d %-10d %-20s %-5c %-5ld %-10ld %-10ld %-30s",
                     t->pid, t->ppid, t->username, t->state, t->nice, t->num_threads, t->virt_size_bytes / 1048576, t->command);
            if(t->highlight == TRUE) {
                wattr_on(win, A_STANDOUT, NULL);
            }
            mvwaddstr(win, i, xoff, procline);
            if(t->highlight == TRUE) {
                wattr_off(win, A_STANDOUT, NULL);
            }
        }
    }

    tasks->is_busy = FALSE;
    pthread_cond_signal(&tasks->cond_updating);
    // shared data is not accessed now: unlock
    pthread_mutex_unlock(&tasks->mux_memdata);

    wrefresh(win);
}

// initializes the bar to empty (like this: "[        ]") and the scale to mark quarters
void init_bars(char *bar, char *scale) {
    // initialize the bar
    bar[0] = '[';
    bar[BARLEN - 1] = ']';
    memset(bar + 1, ' ', (BARLEN - 2) * sizeof(char));
    // initialize the scale
    memset(scale + 1, '_', (BARLEN - 2) * sizeof(char));
    scale[0] = '|';
    scale[BARLEN / 4] = '|';
    scale[BARLEN / 2] = '|';
    scale[(3 * BARLEN) / 4] = '|';
    scale[BARLEN - 1] = '|';
}
