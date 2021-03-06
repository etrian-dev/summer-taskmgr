/**
 * \file cpu_info.h
 * \brief Data structures and functions related to CPU usage
 */
#ifndef CPU_INFO_INCLUDED
#define CPU_INFO_INCLUDED

#include "main.h"

#define CPU_STATFILE "/proc/stat"
#define CPU_MODELFILE "/proc/cpuinfo" // arch-dependent content

// defines a structure to hold cpu statistics
struct core_data_t
{
    // previous (unscaled) data points
    unsigned long int prev_usr;
    unsigned long int prev_usr_nice;
    unsigned long int prev_sys;
    unsigned long int prev_idle;
    unsigned long int prev_total;
    // current core usage percentages (calculated on deltas)
    float perc_usr;
    float perc_usr_nice;
    float perc_sys;
    float perc_idle;
};

typedef struct cpu_data_t
{
    char *model;                 ///< CPU model, as read from /proc/cpuinfo
    int num_cores;               ///< The cpu's number of cores
    struct core_data_t *percore; ///< The per-core usage statistics
    struct core_data_t total;    ///< The usage statistics of the whole CPU
    // syncronization variables
    pthread_mutex_t mux_memdata;
    pthread_cond_t cond_updating;
    bool is_busy;
} CPU_data_t;

// gets statistics about the cpu usage
// not reentrant: uses static variables
bool get_cpu_info(CPU_data_t *cpudata);
bool get_cpu_model(char **model, int *cores);

#endif
