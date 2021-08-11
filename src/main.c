#include <unistd.h>

#include <glib.h>
#include <ncurses.h>
#include <pthread.h>

#include "mem_info.h"
#include "cpu_info.h"
#include "process_info.h"

#include "update_threads.h"

#include "windows.h"

int main(int argc, char **argv) {
    // init ncurses
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();

    // create three indipendent windows
    WINDOW *memwin = newwin(LINES/4, COLS, 0, 0);
    WINDOW *cpuwin = newwin(LINES/4, COLS, LINES/4, 0);
    WINDOW *procwin = newwin(LINES/2, COLS, LINES/2, 0);

    // local vars used to store values to be printed

    // Memory
    Mem_data_t *mem_stats = calloc(1, sizeof(Mem_data_t));
    pthread_mutex_init(&mem_stats->mux_memdata, NULL);
    pthread_cond_init(&mem_stats->cond_updating, NULL);
    // CPU
    CPU_data_t *cpu_stats = calloc(1, sizeof(CPU_data_t));
    // sets the number of cores and the model of the CPU just once
    get_cpu_model(&(cpu_stats->model), &(cpu_stats->num_cores));
    pthread_mutex_init(&cpu_stats->mux_memdata, NULL);
    pthread_cond_init(&cpu_stats->cond_updating, NULL);

    // Processes
    TaskList *tasks = calloc(1, sizeof(TaskList));
    tasks->ps = g_array_new(FALSE, FALSE, sizeof(Task));
    g_array_set_clear_func(tasks->ps, clear_task);
    pthread_mutex_init(&tasks->mux_memdata, NULL);
    pthread_cond_init(&tasks->cond_updating, NULL);

    // terminal window refresh rate (may be different from data refresh rate)
    struct timespec delay;
    delay.tv_sec = 1;
    delay.tv_nsec = 0;

    pthread_t update_th[3];
    pthread_create(&update_th[0], NULL, update_mem, mem_stats);
    pthread_create(&update_th[1], NULL, update_cpu, cpu_stats);
    pthread_create(&update_th[2], NULL, update_proc, tasks);

    while(1) {
        mem_window_update(memwin, mem_stats);
        cpu_window_update(cpuwin, cpu_stats);
        proc_window_update(procwin, tasks);

        nanosleep(&delay, NULL);
    }

    delwin(memwin);
    delwin(cpuwin);
    delwin(procwin);
    endwin();

    return 0;
}

