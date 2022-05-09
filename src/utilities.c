/**
 * \file utilities.c
 * \brief Utility functions used by main.c
 */

#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <ncurses.h>

#include "process_info.h"
#include "main.h"

/**
 * \brief Stops the given POSIX timer timerid and passes to the caller its past settings
 *
 * The POSIX timer timerid (created by timer_create) is disarmed (stopped) by setting
 * its it_value field as zeros. The old interval and starting delay is saved and passed
 * to the caller, so that it may restart the timer with the same settings
 * \param [in] timerid The ID of the timer to be stopped
 * \param [out] oldval The timer's settings before disarming it
 */
void stop_timer(timer_t timerid, struct itimerspec *oldval)
{
    // disarm (stop) the timer that emits SIGALRM and pass to the caller its specification to be able to restart it
    struct itimerspec stop_timer_spec;
    // zeroed to have the field it_value zeroed, so that the timer is stopped
    memset(&stop_timer_spec, 0, sizeof(stop_timer_spec));
    timer_settime(timerid, 0, &stop_timer_spec, oldval); // save the interval
}
/**
 * \brief Prints a menu on the screen from an array of keybindings, items and descriptions
 *
 * The function takes arrays of items, descriptions and the relative keybind associated
 * with it using entries of the form:
 * 	[keybind] item: description
 * \param [in] keybinds An array of bindings associated with the corresponding item
 * \param [in] items An array of item names, representing an action the user can perform
 * \param [in] descr An array of descriptions of the corresponding item in items
 * \param [in] nitems The lenght of each of the input arrays (their lengths must be equal)
 * \return Returns 0 if the menu was printed successfully, -1 otherwise
 */
int print_menu(int *keybinds, char **items, char **descriptions, const int nitems)
{
    int yoff = 1;
    erase();
    for (int i = 0; i < nitems && i < LINES; i++)
    {
        mvprintw(yoff++, 1, "[%c] %s: %s", (char)keybinds[i], items[i], descriptions[i]);
    }
    refresh();
    return 0;
}

/**
 * \brief Utility function to convert a string to long int
 *
 * - 0: conversione ok
 * - 1: non e' un numbero
 * - 2: overflow/underflow
 * \param [in] s La stringa da convertire in un intero
 * \param [out] n Puntatore all'intero risultato della conversione
 * \return Ritorna un numero corrispondente all'esito della conversione, come riportato nella descrizione
 */
int isNumber(const char *s, long *n)
{
    if (s == NULL)
        return 1;
    if (strlen(s) == 0)
        return 1;
    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);
    if (errno == ERANGE)
        return 2; // overflow
    if (e != NULL && *e == (char)0)
    {
        *n = val;
        return 0; // successo
    }
    return 1; // non e' un numero
}

int string_dup(char **dest, const char *src)
{
    if ((*dest = strndup(src, strlen(src) + 1)) == NULL)
    {
        // errore di duplicazione della stringa, riporto il codice di errore al chiamante
        return -1;
    }
    return 0;
}

/**
 * \brief Reads a string (until '\n' is entered) with the given prompt
 *
 * The function reads a string (reads characters until newline) from the given curses WINDOW
 * at the given line and column offset. A null-terminated string may be supplied as a prompt
 *
 * \param [in] win The ncurses window that is accepting input (and where output will be echoed)
 * \param [in] row The row offset inside the window where the input line will be read and echoed
 * \param [in] col The column offset of the prompt string
 * \param [in] prompt A prompt for the user (may be NULL, otherwise it must be a null-terminated string)
 * \return Returns the pattern entered by the user
 */
char *read_pattern(WINDOW *win, const int row, const int col, const char *prompt)
{
    // the color pair green (foreground) on black (background) is defined to
    // help making the search bar stand out more
    int green_on_black = 1;
    init_pair(green_on_black, COLOR_GREEN, COLOR_BLACK);

    // pattern allocation
    char *pattern = calloc(BUF_BASESZ, sizeof(char));
    size_t alloc_len = BUF_BASESZ;
    size_t currlen = 0;
    // the xoff is variable, so it is stored here
    int xoff = col;

    wmove(win, row, col);
    wclrtoeol(win);

    attron(COLOR_PAIR(green_on_black));

    // print the prompt if supplied
    if (prompt)
    {
        mvwaddstr(win, row, xoff, prompt);
        xoff += strlen(prompt);
        wrefresh(win);
    }

    int c;
    while ((c = getch()) != '\n')
    {
        // check if the pattern needs to be realloc'd
        if (alloc_len <= currlen)
        {
            alloc_len *= 2;
            pattern = realloc(pattern, alloc_len);
        }
        // ascii letters & numbers are allowed in the pattern because they're simpler to handle
        if (isascii(c))
        {
            pattern[currlen] = (char)c;
            currlen++;
            // echo the character
            mvaddch(row, xoff + currlen, (char)c);
        }
        // handle backspaces to allow editing the pattern
        if (c == KEY_BACKSPACE && currlen > 0)
        {
            mvdelch(row, xoff + currlen);
            pattern[currlen] = '\0';
            currlen--;
        }
    }

    attroff(COLOR_PAIR(green_on_black));
    return pattern;
}

/**
 * \brief Finds a pattern in the processes's command lines
 *
 * When the search mode is activated by pressing 'f' the user is prompted to
 * write a pattern to be searched in the command lines of processes in the task
 * list. If the pattern entered is a substring of a command line then that
 * process is marked as highlighted until 'f' is pressed again to exit search mode
 */
void find_pattern(TaskList *tasks)
{
    char *pattern = read_pattern(stdscr, LINES - 1, 1, "(find) ");
    // Hightlight matching paths
    pthread_mutex_lock(&tasks->mux_memdata);
    while (tasks->is_busy == true)
    {
        pthread_cond_wait(&tasks->cond_updating, &tasks->mux_memdata);
    }
    tasks->is_busy = true;
    int matches = 0;
    for (int p = 0; p < tasks->num_ps; p++)
    {
        Task *t = &g_array_index(tasks->ps, Task, p);
        if (t->command && strstr(t->command, pattern) != NULL)
        {
            t->highlight = true;
            matches++;
        }
    }
    tasks->is_busy = false;
    pthread_mutex_unlock(&tasks->mux_memdata);
    mvprintw(LINES - 1, 1, "Processes matching \"%s\": %d (press 'f' to quit)", pattern, matches);
    free(pattern);
    wrefresh(stdscr);
}

/**
 * \brief Sends SIGKILL to the process whose PID is entered
 *
 * This function asks the user for a PID and sends SIGKILL to that process. If the
 * process can be killed it will do so, otherwise a short text will be displayed
 */
void kill_process(TaskList *tasks)
{
    // read the PID to kill
    char *pattern = read_pattern(stdscr, LINES - 1, 1, "(kill) ");
    long int pid;
    if (isNumber(pattern, &pid) != 0)
    {
        // The PID is either not a number or out of range for long
        addstr(": not a valid PID");
    }
    else
    {
        // The PID is searched in the task list
        Task *process = NULL;
        Task t;
        t.pid = pid;
        unsigned int idx;
        if (g_array_binary_search(tasks->ps, &t, cmp_pid_incr, &idx) == true)
        {
            process = &g_array_index(tasks->ps, Task, idx);
        }
        errno = 0;
        if (kill((pid_t)pid, SIGKILL) == -1)
        {
            switch (errno)
            {
            case EINVAL:
                // invalid signal name
                addstr(": Invalid signal name");
                break;
            case EPERM:
                // no sufficient permission to kill this process
                mvprintw(LINES - 1, 1, "Cannot kill PID %s (%s): No permission",
                         pattern,
                         (process ? process->command : "no cmdline"));
                break;
            case ESRCH:
                // The PID hasn't been found
                // if the process is a zombie it can't be killed, since it's already terminated
                mvprintw(LINES - 1, 1, "Cannot find PID %s (maybe it's a zombie)", pattern);
                break;
            }
        }
        else
        {
            mvprintw(LINES - 1, 1, "Killed process with PID %s (%s)",
                     pattern,
                     (process ? process->command : "no cmdline"));
        }
    }
    free(pattern);
    wrefresh(stdscr);
}
