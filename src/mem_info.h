/**
 * \file mem_info.h
 * \brief Data structures and functions related to memory usage
 */
#ifndef MEM_INFO_INCLUDED
#define MEM_INFO_INCLUDED

#include <pthread.h>
#include <glib.h>

#include "main.h"

#define MEM_STATFILE "/proc/meminfo"

typedef struct mem_data_t {
    gulong total_mem;
    gulong free_mem;
    gulong avail_mem;
    gulong buffer_cached;
    gulong swp_tot;
    gulong swp_free;
    // syncronization variables
    pthread_mutex_t mux_memdata;
    pthread_cond_t cond_updating;
    gboolean is_busy;
} Mem_data_t;

gboolean get_mem_info(Mem_data_t *mem_usage);

#endif
