#include <time.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>

#include <glib.h>
#include <pthread.h>

#include <ncurses.h>
#include <menu.h> // for menus

#include "mem_info.h"
#include "cpu_info.h"
#include "process_info.h"

#include "update_threads.h"

#include "windows.h"

WINDOW *memwin = NULL;
WINDOW *cpuwin = NULL;
WINDOW *procwin = NULL;

Mem_data_t *mem_stats = NULL;
CPU_data_t *cpu_stats = NULL;
TaskList *tasks = NULL;

/**
 * \brief Signal handler for SIGALRM: refreshes the main windows's contents
 *
 * This handler is called whenever a SIGALRM is received by the process. It simply performs
 * the calls to the functions that update and display the contents of the memory, cpu
 * and process list windows.
 * \param [in] sig The signal received (currently unused)
 */
void alarm_handler(int sig) {
    mem_window_update(memwin, mem_stats);
    cpu_window_update(cpuwin, cpu_stats);
    proc_window_update(procwin, tasks);
}
/**
 * \brief Stops the POSIX timer given and passes to the caller its settings
 *
 * The POSIX timer timerid (created by timer_create) is disarmed (stopped) by setting
 * its it_value field as zeros. The old interval and starting delay is saved and passed
 * to the caller, so that it may restart the timer with the same settings
 * \param [in] timerid The ID of the timer to be stopped
 * \param [out] oldval The timer's settings before disarming it
 */
void stop_timer(timer_t timerid, struct itimerspec *oldval) {
    // disarm (stop) the timer that emits SIGALRM and pass to the caller its specification to be able to restart it
    struct itimerspec stop_timer_spec;
    // zeroed to have the field it_value zeroed, so that the timer is stopped
    memset(&stop_timer_spec, 0, sizeof(stop_timer_spec));
    timer_settime(timerid, 0, &stop_timer_spec, oldval); // save the interval
}
/**
 * \brief Prints a menu on the screen from an array on items
 *
 * The function erases stdscr and prints nitems lines, each containing an
 * item in the array, with the given description associated. It then refreshes
 * the screen so that the menu is visible to the user
 * \param [in] items An array of strings representing an action the user can do
 * \param [in] descr An array of strings representing the corresponding action
 * \param [in] nitems The lenght of each of the input arrays (their lenghts must match)
 * \return Returns 0 if the menu was printed successfully, -1 otherwise
 */
int print_menu(char **items, char **descriptions, const int nitems) {
    int yoff = 1;
    int xoff = 1;
    erase();
    for(int i = 0; i < nitems && i < LINES; i++) {
        mvprintw(yoff++, xoff, "- %s: %s", items[i], descriptions[i]);
    }
    refresh();
    return 0;
}
/**
 *
 */
void find_pattern(void) {
    int c;
    char *pattern = calloc(BUF_BASESZ, sizeof(char));
    gsize alloc_len = BUF_BASESZ;
    const gsize xoff = strlen("Pattern: ");
    gsize currlen = 0;
    while((c = getch()) != '\n') {
        if(alloc_len <= currlen) {
            alloc_len *= 2;
            pattern = realloc(pattern, alloc_len);
        }
        if(isalnum(c)) {
            pattern[currlen] = (char)c;
            currlen++;
            mvaddch(LINES - 1, xoff + currlen, (char)c);
        }
    }
    // Hightlight matching paths
    pthread_mutex_lock(&tasks->mux_memdata);
    while(tasks->is_busy == TRUE) {
        pthread_cond_wait(&tasks->cond_updating, &tasks->mux_memdata);
    }
    tasks->is_busy = TRUE;
    int matches = 0;
    for(int p = 0; p < tasks->num_ps; p++) {
        Task *t = &g_array_index(tasks->ps, Task, p);
        if(strstr(t->command, pattern) != NULL) {
            t->highlight = TRUE;
            matches++;
        }
    }
    tasks->is_busy = FALSE;
    pthread_mutex_unlock(&tasks->mux_memdata);
    mvprintw(LINES - 1, xoff + currlen + 5,
        "Processes matching \"%s\": %d", pattern, matches);
    free(pattern);
    refresh();
}

/**
 * \brief The program's main function
 */
int main(int argc, char **argv) {
    // defines main menu's items and their descriptions
    const int mainmenu_size = 6;
    char **menuitems = malloc(mainmenu_size * sizeof(char*));
    char **menudescr = malloc(mainmenu_size * sizeof(char*));
    menuitems[0] = "q"; menudescr[0] = "Quit the program";
    menuitems[1] = "h"; menudescr[1] = "Help screen";
    menuitems[2] = "s"; menudescr[2] = "Change the process list sorting mode";
    menuitems[3] = "i"; menudescr[3] = "Freeze the state of the screen";
    menuitems[4] = "f"; menudescr[4] = "Find a pattern in the process list";
    menuitems[5] = "m"; menudescr[5] = "Hide the menu";

    // submenu used to choose the process sorting mode
    const int nmodes = 4;
    char **sorting_modes = malloc(nmodes * sizeof(char*));
    char **modes_descr = malloc(nmodes * sizeof(char*));
    sorting_modes[0] = "cmd"; modes_descr[0] = "Sorts processes using lexicographical order of their command line";
    sorting_modes[1] = "PID (incr)"; modes_descr[1] = "Sorts processes using increasing ordering on their PID";
    sorting_modes[2] = "PID (decr)"; modes_descr[2] = "Sorts processes using decreasing ordering on their PID";
    sorting_modes[4] = "PID (decr)"; modes_descr[3] = "Sorts processes using lexicograhical order of their owner's usernames";

    // creates a timer that generates SIGALRM each interval
    // this timer is used to periodically refresh the windows displaying data
    timer_t alarm;
    timer_create(CLOCK_REALTIME, NULL, &alarm);

    // define and install a signal handler for SIGALRM (the function handle_alarm defined above)
    struct sigaction handle_alarm;
    memset(&handle_alarm, 0, sizeof(handle_alarm));
    handle_alarm.sa_handler = alarm_handler;
    // flag set to restart blocking functions interrupted by the signal without throwing EINTR
    handle_alarm.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &handle_alarm, NULL);

    // Initialize the memory data structure
    mem_stats = calloc(1, sizeof(Mem_data_t));
    pthread_mutex_init(&mem_stats->mux_memdata, NULL);
    pthread_cond_init(&mem_stats->cond_updating, NULL);

    // Does the same for CPU
    cpu_stats = calloc(1, sizeof(CPU_data_t));
    // sets the number of cores and the model of the CPU just once at startup, since that's unlikely to change
    get_cpu_model(&(cpu_stats->model), &(cpu_stats->num_cores));
    pthread_mutex_init(&cpu_stats->mux_memdata, NULL);
    pthread_cond_init(&cpu_stats->cond_updating, NULL);

    // And for processes
    tasks = calloc(1, sizeof(TaskList));
    tasks->ps = g_array_new(FALSE, FALSE, sizeof(Task));
    g_array_set_clear_func(tasks->ps, clear_task);
    // default process sorting criteria: lexicographical order of command lines
    tasks->sortfun = cmp_commands;
    pthread_mutex_init(&tasks->mux_memdata, NULL);
    pthread_cond_init(&tasks->cond_updating, NULL);

    // creates the threads that handle data update (detached)
    pthread_t update_th[3];
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);
    pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    pthread_create(&update_th[0], &attrs, update_mem, mem_stats);
    pthread_create(&update_th[1], &attrs, update_cpu, cpu_stats);
    pthread_create(&update_th[2], &attrs, update_proc, tasks);

    // inititalize ncurses with some useful additions
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();

    // create three indipendent windows to handle memory, cpu and process display
    // The first tho occupy each a quarter of the screen, while the third fills the rest
    memwin = newwin(LINES/4, COLS, 0, 0);
    cpuwin = newwin(LINES/4, COLS, LINES/4, 0);
    procwin = newwin(LINES/2, COLS, LINES/2, 0);

    // declare the timer interval and starting offset
    struct timespec interval; // sends SIGALRM every second
    interval.tv_sec = 1;
    interval.tv_nsec = 0;
    struct timespec start_delay; // starts after two seconds
    start_delay.tv_sec = 2;
    start_delay.tv_nsec = 0;
    struct itimerspec spec;
    spec.it_interval = interval;
    spec.it_value = start_delay;
    // arm (start) the timer using the time quantities defined above
    timer_settime(alarm, 0, &spec, NULL);

    // Main application loop: it receives character input directly from the user, but it's
    // interrupted by SIGALRMs each interval
    gboolean run = TRUE;
    gboolean menu_shown = FALSE; // TRUE if the menu below must be printed (updates to data are not shown to the user)
    while(run == TRUE) {
        if(menu_shown == TRUE) {
            print_menu(menuitems, menudescr, mainmenu_size);
        }
        // gets user input (be aware that this works whether the menu is shown or not)
        int c = getch();
        switch(c) {
            case 'q':
                run = FALSE;
                break;
            case 'h':
                // unimplemented
                break;
            case 's': {
                // print the submenu and change the soring mode
                print_menu(sorting_modes, modes_descr, nmodes);
                char s = getch();
                switch(s) {
                    case '0':
                        switch_sortmode(tasks, cmp_commands);
                        break;
                    case '1':
                        switch_sortmode(tasks, cmp_pid_incr);
                        break;
                    case '2':
                        switch_sortmode(tasks, cmp_pid_decr);
                        break;
                    case '3':
                        switch_sortmode(tasks, cmp_usernames);
                        break;
                }
                break;
            }

            case 'i':
                // unimplemented
                break;
            case 'f': {
                // search for a pattern in the process list
                attron(A_BOLD);
                mvaddstr(LINES - 1, 1, "Pattern: ");
                attroff(A_BOLD);
                find_pattern();
                menu_shown = FALSE;
                timer_settime(alarm, 0, &spec, NULL);
                break;
            }
            case 'm': // toggles menu visibility
                menu_shown = (menu_shown == TRUE ? FALSE : TRUE);
                // stop the timer if the menu will be shown
                if(menu_shown == TRUE) {
                    stop_timer(alarm, &spec);
                }
                // otherwise restart it because the menu will be hidden
                else {
                    timer_settime(alarm, 0, &spec, NULL);
                }
        }
    }

    // free menu items and descriptions
    free(menuitems);
    free(menudescr);
    free(sorting_modes);
    free(modes_descr);
    // deletes the alarm timer
    timer_delete(alarm);
    // frees the memory, cpu and process data structures
    free(mem_stats);
    if(cpu_stats->model) free(cpu_stats->model);
    free(cpu_stats);
    if(tasks->ps) g_array_free(tasks->ps, TRUE); // frees data stored inside as well
    tasks->sortfun = NULL;
    free(tasks);
    // deletes all the WINDOWs and end ncurses mode
    delwin(memwin);
    delwin(cpuwin);
    delwin(procwin);
    endwin();

    return 0;
}

