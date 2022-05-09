/**
 * \file mem_info.h
 * \brief Data structures and functions related to memory usage
 */
#ifndef MEM_INFO_INCLUDED
#define MEM_INFO_INCLUDED

#include <pthread.h>

#include "main.h"

#define MEM_STATFILE "/proc/meminfo"

typedef struct mem_data_t
{
    unsigned long total_mem;
    unsigned long free_mem;
    unsigned long avail_mem;
    unsigned long buffer_cached;
    unsigned long swp_tot;
    unsigned long swp_free;
    // syncronization variables
    pthread_mutex_t mux_memdata;
    pthread_cond_t cond_updating;
    bool is_busy;
} Mem_data_t;

bool get_mem_info(Mem_data_t *mem_usage);

#endif
