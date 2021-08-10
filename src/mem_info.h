#ifndef MEM_INFO_INCLUDED
#define MEM_INFO_INCLUDED

#include <glib.h>

#define MEM_STATFILE "/proc/meminfo"
#define BUF_BASESZ 256

typedef struct mem_data_t {
    gulong total_mem;
    gulong free_mem;
    gulong avail_mem;
    gulong buffer_cached;
    gulong swp_tot;
    gulong swp_free;
} Mem_data_t;

gboolean get_mem_info(Mem_data_t *mem_usage);

#endif
