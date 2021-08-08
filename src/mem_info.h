#ifndef MEM_INFO_INCLUDED
#define MEM_INFO_INCLUDED

#include <glib.h>

#define MEM_STATFILE "/proc/meminfo"
#define BUF_BASESZ 256

gboolean get_mem_info(
    gulong *total,
    gulong *free,
    gulong *available,
    gulong *buffer_cached,
    gulong *swp_tot,
    gulong *swp_avail
    );

#endif
