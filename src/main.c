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
 * \brief Finds a pattern in the processes's command lines
 *
 * When the search mode is activated by pressing 'f' the user is prompted to
 * write a pattern to be searched in the command lines of processes in the task
 * list. If the pattern entered is a substring of a command line then that
 * process is marked as highlighted until 'f' is pressed again to exit search mode
 */
void find_pattern(void) {
    char *pattern = calloc(BUF_BASESZ, sizeof(char));
    gsize alloc_len = BUF_BASESZ;
    gsize currlen = 0;
    const gsize xoff = strlen("Pattern: "); // for echoing characters typed after the prompt message
    int c;
    while((c = getch()) != '\n') {
        if(alloc_len <= currlen) {
            alloc_len *= 2;
            pattern = realloc(pattern, alloc_len);
        }
        // ascii characters can appear in the pattern
        if(isalnum(c)) {
            pattern[currlen] = (char)c;
            currlen++;
            mvaddch(LINES - 1, xoff + currlen, (char)c);
        }
        // handle backspaces to allow editing the pattern
        if(c == KEY_BACKSPACE && currlen > 0) {
            mvdelch(LINES - 1, xoff + currlen);
            pattern[currlen] = '\0';
            currlen--;
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
        "Processes matching \"%s\": %d (press 'f' to quit)", pattern, matches);
    free(pattern);
    wrefresh(stdscr);
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
    const int nmodes = 6;
    char **modes_items = malloc(nmodes * sizeof(char*));
    char **modes_descr = malloc(nmodes * sizeof(char*));
    modes_items[0] = "[0] cmd"; modes_descr[0] = "Sorts processes using lexicographical order of their command line";
    modes_items[1] = "[1] PID (incr)"; modes_descr[1] = "Sorts processes using increasing ordering on their PID";
    modes_items[2] = "[2] PID (decr)"; modes_descr[2] = "Sorts processes using decreasing ordering on their PID";
    modes_items[3] = "[3] Username"; modes_descr[3] = "Sorts processes using lexicograhical order of their owner's usernames";
    modes_items[4] = "[4] thread count (incr)"; modes_descr[4] = "Sorts processes using decreasing ordering on their thread count";
    modes_items[5] = "[5] thread count (decr)"; modes_descr[5] = "Sorts processes using decreasing ordering on their thread count";
    // initialize with modes defined in process_info.h
    int (**sorting_modes)(const void*, const void*) = malloc(nmodes * sizeof(*sorting_modes));
    sorting_modes[0] = cmp_commands;
    sorting_modes[1] = cmp_pid_incr;
    sorting_modes[2] = cmp_pid_decr;
    sorting_modes[3] = cmp_usernames;
    sorting_modes[4] = cmp_nthreads_inc;
    sorting_modes[5] = cmp_nthreads_decr;

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
    tasks->cursor_start = 0; // the cursor starts at the first process
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
    if(has_colors() == TRUE) {
        start_color(); // start using colors
    }
    cbreak();
    keypad(stdscr, TRUE);
    noecho();
    // define a green on black color pair
    short green_on_black = 1;
    init_pair(green_on_black, COLOR_GREEN, COLOR_BLACK);

    // create three indipendent windows to handle memory, cpu and process display
    // The first tho occupy each a quarter of the screen, while the third fills the rest
    memwin = newwin(LINES/4, COLS, 0, 0);
    cpuwin = newwin(LINES/4, COLS, LINES/4, 0);
    procwin = newwin(LINES/2, COLS, LINES/2 - 1, 0);

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
    gboolean searching = FALSE; // flag set iff a searching is in progress
    int key = 0, last_key = 0; // remembers the last key pressed
    // This is needed to have a shorter timer interval only when the user is scolling
    while(run == TRUE) {
        if(menu_shown == TRUE) {
            print_menu(menuitems, menudescr, mainmenu_size);
        }
        // gets user input (be aware that this works whether the menu is shown or not)
        key = getch();
        switch(key) {
            case 'q':
                run = FALSE;
                break;
            case 'h':
                // unimplemented
                break;
            case 's': {
                // print the submenu and change the soring mode
                print_menu(modes_items, modes_descr, nmodes);
                char s = getch();
                if(s >= '0' && s <= '0' + nmodes - 1) {
                    switch_sortmode(tasks, sorting_modes[s - '0']);
                }
                break;
            }

            case 'i':
                // unimplemented
                break;
            case 'f': {
                if(searching == TRUE) {
                    // 'f' was pressed to quit the search view: reset all highlighted processes to normal
                    for(int i = 0; i < tasks->num_ps; i++) {
                        Task *t = &g_array_index(tasks->ps, Task, i);
                        if(t->highlight == TRUE) {
                            t->highlight = FALSE;
                        }
                    }
                    searching = FALSE;
                    // clears the search line (LINES - 1)
                    wmove(stdscr, LINES - 1, 0);
                    wclrtoeol(stdscr);
                }
                else {
                    // search for a pattern in the process list
                    attron(COLOR_PAIR(green_on_black));
                    mvaddstr(LINES - 1, 1, "Pattern: ");
                    find_pattern();
                    attroff(COLOR_PAIR(green_on_black));
                    // hide the menu
                    menu_shown = FALSE;
                    // set the searching flag
                    searching = TRUE;
                    // restart the timer
                    timer_settime(alarm, 0, &spec, NULL);
                    // update the window immediately (otherwise it will refresh at the next timer tick)
                    proc_window_update(procwin, tasks);
                }
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
            // handling of the process cursor movement (to simulate a scrollable window)
            case KEY_DOWN:
                // if a key is pressed
                if(tasks->cursor_start < tasks->num_ps - 1)
                    tasks->cursor_start++;
                wrefresh(procwin);
                break;
            case KEY_UP:
                if(tasks->cursor_start > 0)
                    tasks->cursor_start--;
                wrefresh(procwin);
                break;
            case KEY_END:
                tasks->cursor_start = tasks->num_ps - 1;
                wrefresh(procwin);
                break;
            case KEY_HOME:
                tasks->cursor_start = 0;
                wrefresh(procwin);
                break;
            case KEY_NPAGE: {
                int prows, pcols;
                getmaxyx(procwin, prows, pcols);
                tasks->cursor_start += prows - 4;
                if(tasks->cursor_start > tasks->num_ps) {
                    tasks->cursor_start = tasks->num_ps - 1;
                }
                wrefresh(procwin);
                break;
            }
            case KEY_PPAGE: {
                int prows, pcols;
                getmaxyx(procwin, prows, pcols);
                tasks->cursor_start -= prows - 4;
                if(tasks->cursor_start  < 0) {
                    tasks->cursor_start = 0;
                }
                wrefresh(procwin);
                break;
            }
        }

        // if both the last pressed key and the current are scrolling keys (one of those listed above)
        // shorten the timer tick and restart it
        if( (key == KEY_DOWN || key == KEY_UP
            || key == KEY_NPAGE || key == KEY_PPAGE
            || key == KEY_END || key == KEY_HOME)
            && (last_key == KEY_DOWN || last_key == KEY_UP
            || last_key == KEY_NPAGE || last_key == KEY_PPAGE
            || last_key == KEY_END || last_key == KEY_HOME)) {
            struct itimerspec short_tick;
            short_tick.it_interval.tv_sec = 0;
            short_tick.it_interval.tv_nsec = 250000000;
            short_tick.it_value.tv_sec = 0;
            short_tick.it_value.tv_nsec = 100000;
            timer_settime(alarm, 0, &short_tick, &spec);
        }
        // if the current key is not a scrolling key and the last key was
        // then reset the timer to the normal tick
        if( (key != KEY_DOWN && key == KEY_UP
            && key != KEY_NPAGE && key != KEY_PPAGE
            && key != KEY_END && key != KEY_HOME)
            && (last_key == KEY_DOWN || last_key == KEY_UP
            || last_key == KEY_NPAGE || last_key == KEY_PPAGE
            || last_key == KEY_END || last_key == KEY_HOME)) {
            timer_settime(alarm, 0, &spec, NULL);
        }

        // update the last key pressed
        last_key = key;
    }

    // free menu items and descriptions
    free(menuitems);
    free(menudescr);
    free(modes_items);
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

