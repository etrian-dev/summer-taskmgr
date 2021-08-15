#include <pthread.h>

#include <unistd.h>

#include "mem_info.h"
#include "cpu_info.h"
#include "process_info.h"

/**
 * \brief This is the function executed by the thread that updates the memory data structure
 */
void *update_mem(void *mem_ds) {
    struct timespec delay;
    delay.tv_sec = 2;
    delay.tv_nsec = 0;

    while(1) {
        Mem_data_t *mdata = (Mem_data_t*)mem_ds;
        // about to access shared data: lock
        pthread_mutex_lock(&mdata->mux_memdata);
        while(mdata->is_busy == TRUE) {
            pthread_cond_wait(&mdata->cond_updating, &mdata->mux_memdata);
        }
        mdata->is_busy = TRUE;

        get_mem_info(mdata);

        mdata->is_busy = FALSE;
        pthread_cond_signal(&mdata->cond_updating);
        // shared data is not accessed now: unlock
        pthread_mutex_unlock(&mdata->mux_memdata);

        nanosleep(&delay, NULL);
    }
    return (void*)0;
}
/**
 * \brief This is the function executed by the thread that updates the CPU data structure
 */
void *update_cpu(void *cpu_ds) {
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 500000000; // 0.5s

    while(1) {
        CPU_data_t *cpudata = (CPU_data_t*)cpu_ds;
        // about to access shared data: lock
        pthread_mutex_lock(&cpudata->mux_memdata);
        while(cpudata->is_busy == TRUE) {
            pthread_cond_wait(&cpudata->cond_updating, &cpudata->mux_memdata);
        }
        cpudata->is_busy = TRUE;

        get_cpu_info(cpudata);

        cpudata->is_busy = FALSE;
        pthread_cond_signal(&cpudata->cond_updating);
        // shared data is not accessed now: unlock
        pthread_mutex_unlock(&cpudata->mux_memdata);

        nanosleep(&delay, NULL);
    }
    return (void*)0;
}
/**
 * \brief This is the function executed by the thread that updates the process list data structure
 */
void *update_proc(void *proc_ds) {
    struct timespec delay;
    delay.tv_sec = 1;
    delay.tv_nsec = 0;

    while(1) {
        TaskList *tl = (TaskList*)proc_ds;
        // about to access shared data: lock
        pthread_mutex_lock(&tl->mux_memdata);
        while(tl->is_busy == TRUE) {
            pthread_cond_wait(&tl->cond_updating, &tl->mux_memdata);
        }
        tl->is_busy = TRUE;

        get_processes_info(tl);

        tl->is_busy = FALSE;
        pthread_cond_signal(&tl->cond_updating);
        // shared data is not accessed now: unlock
        pthread_mutex_unlock(&tl->mux_memdata);

        nanosleep(&delay, NULL);
    }
    return (void*)0;
}
