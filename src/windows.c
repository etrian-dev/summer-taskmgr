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
    box(win, ACS_VLINE, ACS_HLINE);
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

    // get the current memory usage values
    get_mem_info(mem_usage);

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

    mvwprintw(win, yoff++, xoff, "%s (%.3f%% in use)", swp_bar, percent);
    mvwaddstr(win, yoff++, xoff, scale);
    mvwprintw(win, yoff++, xoff, values);

    wrefresh(win);
}
/// function that deals with meters included in the cpu window
void cpu_window_update(WINDOW *win, CPU_data_t *cpu_usage, int *core_count) {
    werase(win);
    box(win, ACS_VLINE, ACS_HLINE);
    // update the cpu usage
    get_cpu_info(core_count, cpu_usage);
    // erase the window's contents
    char ln[LINE_MAXLEN];
    char ln2[LINE_MAXLEN];
    snprintf(ln, LINE_MAXLEN, "CPU %s\tcores %d", "<placeholder>", *core_count);

    snprintf(ln2, LINE_MAXLEN,
             "CPU%% usr: %.3f\tnice: %.3f\tsys: %.3f\tidle: %.3f",
              cpu_usage->perc_usr, cpu_usage->perc_usr_nice, cpu_usage->perc_sys, cpu_usage->perc_idle);

    mvwaddstr(win, 1, 1, ln);
    mvwaddstr(win, 2, 1, ln2);
    // refresh the window to display the contents
    wrefresh(win);
}

// default sorting function for processes in the process array
int cmp_commands(const void *a, const void *b) {
    Task *ta = (Task*)a;
    Task *tb = (Task*)b;
    if(!(ta->command || tb->command)) {
        return 0;
    }
    if(!ta->command && tb->command) {
        return 1;
    }
    if(ta->command && !tb->command) {
        return -1;
    }
    return strcasecmp(ta->command, tb->command);
}

/// function that deals with meters included in the process list window
void proc_window_update(WINDOW *win) {
    werase(win);
    box(win, ACS_VLINE, ACS_HLINE);

    int lines, cols;
    getmaxyx(win, lines, cols);
    int yoff = 1, xoff = 1;

    // the array of structures representig processes
    GArray *ps = g_array_new(FALSE, FALSE, sizeof(Task));
    g_array_set_clear_func(ps, clear_task);
    // other local vars storing the values to be printed
    long num_ps = 0;
    long num_threads = 0;

    char ln[LINE_MAXLEN];
    char ln2[LINE_MAXLEN];

    // update the process array
    get_processes_info(ps, &num_ps, &num_threads);
    // sort the process array lexicographically by command name
    g_array_sort(ps, cmp_commands);

    snprintf(ln, LINE_MAXLEN, "processes: %lu\tthreads: %lu", num_ps, num_threads);
    snprintf(ln2, LINE_MAXLEN,
        "%-10s %-10s %-5s %-5s %-10s %-10s %-10s",
        "PID", "PPID", "STATE", "NICE", "THREADS", "VSZ (kB)", "CMD");

    mvwaddstr(win, yoff++, xoff, ln);
    mvwaddstr(win, yoff++, xoff, ln2);

    for(int i = yoff; i < lines - 1; i++) {
        char procline[LINE_MAXLEN];
        Task *t = &(g_array_index(ps, Task, i));
        snprintf(procline, LINE_MAXLEN,
                 "%-10d %-10d %-5c %-5ld %-10ld %-10ld %s",
                 t->pid, t->ppid, t->state, t->nice, t->num_threads, t->virt_size_bytes / 1024, t->command);

        mvwaddstr(win, i, xoff, procline);
    }

    num_ps = 0;
    num_threads = 0;

    // reset the array
    g_array_free(ps, TRUE);
    ps = g_array_new(FALSE, FALSE, sizeof(Task));
    g_array_set_clear_func(ps, clear_task);

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
    scale[BARLEN / 3] = '|';
    scale[BARLEN - 1] = '|';
}
