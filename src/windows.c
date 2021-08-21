#include <stdio.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <ncurses.h>

#include "cpu_info.h"
#include "mem_info.h"
#include "process_info.h"
#include "windows.h"

/**
 * \brief Scales down by factors of 1K the given quantity and returns the amount of
 * times quantity has been scaled
 *
 * The functions takes a positive quantity and performs an integer division by 1K at each iteration.
 * It supports an initial amount of scaling being already performed on quantity, so that
 * it can be added up in the total scaling amount applied
 * \param [in,out] quantity Pointer to a positive integer to be scaled
 * \param [in] scale_in The initail amount of scaling already applied on quantity
 * \return The number of divisions by 1K performed on quantity
 */
int scale_down_1K(size_t *quantity, int scale_in) {
    int niter = (scale_in > 0 ? scale_in : 0);
    while(niter < 3 && *quantity > 1024) {
        *quantity >>= 10;
        niter++;
    }
    return niter;
}

// function that deals with meters included in the memory window
void mem_window_update(WINDOW *win, Mem_data_t *mem_usage, int scaling) {
    int lines, cols;
    getmaxyx(win, lines, cols);
    int yoff = 1, xoff = 1;

    // a progress bar filled with a number of '#' that matches the percentage of
    // memory available in the system. A scale is drawn as well to mark the quarters of the bar
    char ram_bar[BARLEN];
    char swp_bar[BARLEN];
    char scale[BARLEN];
    init_bars(ram_bar, scale);
    init_bars(swp_bar, scale);
    char ram_values[LINE_MAXLEN];
    char swp_values[LINE_MAXLEN];
    // local vars to store the memory quantities to be displayed
    gulong total, avail, free, buff_cache, swptot, swpfree;

    // about to access shared data: lock
    pthread_mutex_lock(&mem_usage->mux_memdata);
    while(mem_usage->is_busy == TRUE) {
        pthread_cond_wait(&mem_usage->cond_updating, &mem_usage->mux_memdata);
    }
    mem_usage->is_busy = TRUE;

    total = mem_usage->total_mem;
    avail = mem_usage->avail_mem;
    free = mem_usage->free_mem;
    buff_cache = mem_usage->buffer_cached;
    swptot = mem_usage->swp_tot;
    swpfree = mem_usage->swp_free;

    mem_usage->is_busy = FALSE;
    pthread_cond_signal(&mem_usage->cond_updating);
    // shared data is not accessed now: unlock
    pthread_mutex_unlock(&mem_usage->mux_memdata);

    // the percentage needs to be calculated before the eventual scaling, so that operating
    // on raw values yields precise results
    gfloat ram_percent = 100.0 - (avail * 100.0)/(float)total;
    gfloat swp_percent = 100 - (swpfree * 100)/(float)swptot;

    char *units[4] = {"B", "KiB", "MiB", "GiB"};
    // scale memory quantities down (they are already expressed in KB, so the initial scale is 1)
    int tot_scale = 1;
    int avail_scale = 1;
    int free_scale = 1;
    int buff_scale = 1;
    int swptot_scale = 1;
    int swpfree_scale = 1;
    // if scaling is set, activate it now
    if(scaling == 1) {
        tot_scale = scale_down_1K(&total, 1);
        avail_scale = scale_down_1K(&avail, 1);
        free_scale = scale_down_1K(&free, 1);
        buff_scale = scale_down_1K(&buff_cache, 1);
        swptot_scale = scale_down_1K(&swptot, 1);
        swpfree_scale = scale_down_1K(&swpfree, 1);
    }

    // set the memory percentage bar and prints the raw values on a string
    memset(ram_bar + 1, '#', (int)round(ram_percent) * sizeof(char));
    snprintf(ram_values, LINE_MAXLEN,
             "Total: %lu %s\tAvailable: %lu %s\tFree: %lu %s\tBuff/Cached: %lu %s",
              total, units[tot_scale], avail, units[avail_scale],
              free, units[free_scale], buff_cache, units[buff_scale]);
    // does the same thing, but for swap
    memset(swp_bar + 1, '#', (int)round(swp_percent) * sizeof(char));
    snprintf(swp_values, LINE_MAXLEN,
             "Swap Total: %lu %s\tSwap Free: %lu %s",
             swptot, units[swptot_scale], swpfree, units[swpfree_scale]);

    // erase the previous window contents and add the new values, then refresh
    werase(win);
    mvwprintw(win, yoff++, xoff, "%s (%.3f%% in use)", ram_bar, ram_percent);
    mvwaddstr(win, yoff++, xoff, scale);
    mvwprintw(win, yoff++, xoff, ram_values);
    mvwprintw(win, yoff++, xoff, "%s (%.3f%% in use)", swp_bar, swp_percent);
    mvwaddstr(win, yoff++, xoff, scale);
    mvwprintw(win, yoff++, xoff, swp_values);

    wrefresh(win);
}
/// function that deals with meters included in the cpu window
void cpu_window_update(WINDOW *win, CPU_data_t *cpu_usage) {
    werase(win);
    // erase the window's contents
    char make_model[LINE_MAXLEN];
    char totals[LINE_MAXLEN];

    int core = 0;
    // alloc and initialize one bar per core
    char **core_bars = malloc(cpu_usage->num_cores * sizeof(char*));
    char scale[BARLEN]; // one scale for all the bars
	for(core = 0; core < cpu_usage->num_cores; core++) {
		core_bars[core] = malloc(BARLEN * sizeof(char));
		init_bars(core_bars[core], scale);
		memset(core_bars[core] + 1, '#', ((long)round(100.0 - cpu_usage->percore[core].perc_idle)) * sizeof(char));
	}

    // about to access shared data: lock
    pthread_mutex_lock(&cpu_usage->mux_memdata);
    while(cpu_usage->is_busy == TRUE) {
        pthread_cond_wait(&cpu_usage->cond_updating, &cpu_usage->mux_memdata);
    }
    cpu_usage->is_busy = TRUE;

    snprintf(make_model, LINE_MAXLEN, "Model: \'%s\'\tCores: %d", cpu_usage->model, cpu_usage->num_cores);

    snprintf(totals, LINE_MAXLEN,
             "CPU%% usr: %.3f\tnice: %.3f\tsys: %.3f\tidle: %.3f",
              cpu_usage->total.perc_usr, cpu_usage->total.perc_usr_nice,
              cpu_usage->total.perc_sys, cpu_usage->total.perc_idle);

    cpu_usage->is_busy = FALSE;
    pthread_cond_signal(&cpu_usage->cond_updating);
    // shared data is not accessed now: unlock
    pthread_mutex_unlock(&cpu_usage->mux_memdata);

    mvwaddstr(win, 1, 1, make_model);
    for(core = 0; core < cpu_usage->num_cores; core++) {
		mvwprintw(win, core + 2, 1, "core%d %s (%.3f%%)",
			core, core_bars[core], 100.0 - cpu_usage->percore[core].perc_idle);
	}
    mvwaddstr(win, cpu_usage->num_cores + 3, 1, totals);
    // refresh the window to display the contents
    wrefresh(win);
}

/// function that deals with meters included in the process list window
void proc_window_update(WINDOW *win, TaskList *tasks) {
    werase(win);

    short table_header_color = 4;
    short cursor_highlight_color = 5;
    init_pair(table_header_color, COLOR_CYAN, COLOR_WHITE);
    init_pair(cursor_highlight_color, COLOR_WHITE, COLOR_BLUE);

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

    wattr_on(win, A_BOLD, NULL);
    mvwaddstr(win, yoff++, xoff, ln);
    wattr_off(win, A_BOLD, NULL);

    wattr_on(win, A_STANDOUT, NULL);
    mvwaddstr(win, yoff++, xoff, ln2);
    wattr_off(win, A_STANDOUT, NULL);

    int i = 0;
    while((i < lines - yoff - 1) || (tasks->cursor_start + i == tasks->num_ps)) {
        char procline[LINE_MAXLEN];
        Task *t = &(g_array_index(tasks->ps, Task, tasks->cursor_start + i));
        if(t->visible == TRUE) {
            snprintf(procline, LINE_MAXLEN,
                     "%-10d %-10d %-20s %-5c %-5ld %-10ld %-10ld %-30s",
                     t->pid, t->ppid, t->username, t->state, t->nice, t->num_threads, t->virt_size_bytes / 1048576, t->command);
            attr_t proc_attrs = 0x0;
            if(t->highlight == TRUE) {
                proc_attrs |= A_STANDOUT;
            }
            // place the cursor on the first process being displayed (overrides the matching highlight)
            if(i == 0) {
                proc_attrs = COLOR_PAIR(cursor_highlight_color);
            }
            wattr_on(win, proc_attrs, NULL);
            mvwaddnstr(win, i + yoff, xoff, procline, cols - 2); // adds the string truncated at the window's width - 2
            wattr_off(win, proc_attrs, NULL);
        }
        i++;
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
