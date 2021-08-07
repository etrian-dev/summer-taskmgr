/*
 * main.c
 *
 * Copyright 2021 nicola vetrini <nicola@ubuntu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */


#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <ncurses.h>

#include "cpu_info.h"
#include "mem_info.h"
#include "process_info.h"

int main(int argc, char **argv)
{
	// init curses
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	int rows, cols;
	getmaxyx(stdscr, rows, cols);

	gulong total_mem, free_mem, avail_mem, swp_tot, swp_free;
	// refresh rate (approx)
	struct timespec delay;
	delay.tv_sec = 0;
	delay.tv_nsec = 500000000; // 5 * 10^8 ns = 0.5s

	char progbar[102];
	progbar[0] = '[';
	progbar[101] = ']';
	memset(progbar + 1, '.', 100 * sizeof(char));
	char scale[102];
	memset(scale + 1, '_', 100 * sizeof(char));
	scale[0] = '|';
	scale[25] = '|';
	scale[50] = '|';
	scale[75] = '|';
	scale[101] = '|';

	int xoff = 1;
	int yoff = 1;

	GArray *ps = NULL;
	gulong num_ps;
	gulong num_threads;

	while(1) {
		get_mem_info(&total_mem, &free_mem, &avail_mem, &swp_tot, &swp_free);
		get_processes_info(&ps, &num_ps, &num_threads);

		// sets the percentage of memory not available for new processes (not necessarily in use)
		// as the
		gfloat percent = 100.0 - (avail_mem * 100.0)/(float)total_mem;
		gulong quot = floor(percent);
		memset(progbar + 1, '#', quot * sizeof(char));

		mvaddstr(yoff++, xoff, "MEMORY STATISTICS");
		mvprintw(yoff++, xoff, "%s (%.3f%% in use)", progbar, percent);
		mvaddstr(yoff++, xoff, scale);
		mvprintw(
			yoff++, xoff,
			"Total: %lu MiB\tAvailable: %lu MiB\tFree: %lu",
			total_mem / 1024, avail_mem / 1024, free_mem / 1024);

		// does the same thing, but for swap (resets the bar first)
		memset(progbar + 1, '.', quot * sizeof(char));
		percent = 100 - (swp_free * 100)/(float)swp_tot;
		memset(progbar + 1, '#', percent * sizeof(char));

		mvprintw(yoff++, xoff, "%s (%.3f%% in use)", progbar, percent);
		mvaddstr(yoff++, xoff, scale);
		mvprintw(yoff++, xoff, "Swap Total: %lu MiB\tSwap Free: %lu", swp_tot / 1024, swp_free / 1024);

		// print the list of processes (as long as they fit in the window
		yoff++;
		mvaddstr(yoff++, xoff, "PROCESS STATISTICS");
		mvprintw(yoff++, xoff, "# processes: %lu", num_ps);
		mvprintw(yoff++, xoff,
			"%-10s %-10s %-10s",
			"PID", "STATE", "CMD");
		for(int i = 0; i + yoff < rows - 1; i++) {
			Task *t = &(g_array_index(ps, Task, num_ps - i - 1));
			mvprintw(i + yoff, xoff,
				"%-10ld %-10c %-s",
				t->pid, t->state, t->command);
		}

		// reset the array
		g_array_free(ps, TRUE);
		ps = NULL;
		num_ps = 0;
		num_threads = 0;

		refresh();

		memset(progbar + 1, '.', percent * sizeof(char));
		yoff = 1;

		nanosleep(&delay, NULL);
	}
	endwin();
	return 0;
}

