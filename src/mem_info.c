#include <stdio.h>

#include <glib.h>

#include "mem_info.h"

// gets statistics about the memory usage
// see man 5 proc at section "/proc/meminfo"
gboolean get_mem_info(Mem_data_t *mem_usage) {

    //temp local variables
    gulong mtot = 0;
    gulong mfree = 0;
    gulong mavail = 0;
    gulong mbuffer = 0;
    gulong mcached = 0;
    gulong mswp_tot = 0;
    gulong mswp_free = 0;

    FILE *stat_file = NULL;
    char *buf = malloc(BUF_BASESZ);
    gulong bufsz = BUF_BASESZ;
    if((stat_file = fopen(MEM_STATFILE, "r")) && buf) {
        while(fgets(buf, bufsz, stat_file)) {
            sscanf(buf, "MemTotal:%*[^0-9]%lu", &mtot);
            sscanf(buf, "MemFree:%*[^0-9]%lu", &mfree);
            sscanf(buf, "MemAvailable:%*[^0-9]%lu", &mavail);
            sscanf(buf, "Buffers:%*[^0-9]%lu", &mbuffer);
            sscanf(buf, "Cached:%*[^0-9]%lu", &mcached);
            sscanf(buf, "SwapTotal:%*[^0-9]%lu", &mswp_tot);
            sscanf(buf, "SwapFree:%*[^0-9]%lu", &mswp_free);
        }
        if(mavail == 0) {
            mavail = mfree + mbuffer + mcached;
        }

        mem_usage->total_mem = mtot;
        mem_usage->free_mem = mfree;
        mem_usage->avail_mem = mavail;
        mem_usage->buffer_cached = mbuffer + mcached;
        mem_usage->swp_tot = mswp_tot;
        mem_usage->swp_free = mswp_free;
    }
    return (stat_file && buf ? TRUE : FALSE);
}
