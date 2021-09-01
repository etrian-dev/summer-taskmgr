/**
 * \file sighandler.c
 * \brief File containing implementation of signal handlers declared in main.h
 */

#include <signal.h>
#include <unistd.h>

#include <ncurses.h>
#include <pthread.h>

#include "windows.h"

#include "mem_info.h"
#include "cpu_info.h"
#include "process_info.h"

#include "main.h"

/**
 * \brief Function triggered by SIGALRM: refreshes the main windows's contents
 *
 * This handler is called whenever a SIGALRM is received by the process. It simply performs
 * the calls to the functions that update and display the contents of the memory, cpu
 * and process list windows
 */
void alarm_handler( WINDOW *memwin, Mem_data_t *mem_stats,
                    WINDOW *cpuwin, CPU_data_t *cpu_stats,
                    WINDOW *procwin, TaskList *tasks,
                    int flag_rawdata) {
    mem_window_update(memwin, mem_stats, flag_rawdata);
    cpu_window_update(cpuwin, cpu_stats);
    proc_window_update(procwin, tasks);
}

void *signal_thread(void *param) {
    struct taskmgr_data_t *data = (struct taskmgr_data_t *)param;

    // prepare the signal mask to capture SIGALRM and SIGINT
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGINT);
    int delivered_sig;

    int noterm = 1;
    while(noterm) {
        if(sigwait(&mask, &delivered_sig) != 0) {
            return (void*)1;
        }
        if(delivered_sig == SIGALRM) {
            alarm_handler(  data->memwin, data->mem_stats,
                            data->cpuwin, data->cpu_stats,
                            data->procwin, data->tasks,
                            data->rawdata);
        }
        if(delivered_sig == SIGINT) {
            _exit(0);
        }
    }
    return (void*)0;
}
