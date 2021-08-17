# Summer task manager
A simple terminal-based task manager written in C, with a TUI interface written in ncurses
**Required libraries**:
- glib
- [ncurses](https://invisible-island.net/ncurses/)
- pthreads
- [libjansson](https://digip.org/jansson/)
- linux-specific headers, such as `unistd.h`
## Compiling
Just `make` and it should be fine, given the prerequisites above are satisfied.
An executable named ./a.out will be produced in the base directory of the repository
## Execution
The task manager has a main screen containing memory and cpu usage statistics
and a scrollable process list. A simple menu (hidden at startup) allows the user to
perform a few actions, such as quitting the program and sorting processes; the menu
visibility is toggled (shown/hidden) by pressing the 'm' key. The operation to find a
pattern in processes'command lines, activated by pressing 'f', can be used without entering
the menu. Be aware that, while keybindings that trigger an operation are received even
then the menu is not visible, most of them are not designed to be used that way (such as
's' to sort processes).
### Menu options
The menus are loaded at runtime from the file `menus.json` in the repository's base directory
The default file lists the following options for the main menu:
- Quit (q): Quit the program
- Help (h): Shows the help screen (unimplemented)
- Sort (s): Change the process sorting mode (this opens a new submenu)
- Freeze (i): Freeze the screen (unimplemented)
- Find (f): Find a pattern in the process list
- Menu (m): Show/Hide the menu
The submenu opened by selecting 's' contains the implemented sorting modes for processes:
- Command (0): Sorts processes in lexicographical order of their command line
- Username (1): Sorts processes in lexicographical order of their owner's username
- PID incr (2): Sorts processes in increasing order of their PID
- PID decr (3): Sorts processes in decreasing order of their PID
- thread incr (3): Sorts processes in increasing order of their thread count
- thread decr (3): Sorts processes in decreasing order of their thread count
### Usage
To access the menu type 'm'. You will be presented with the set of options described above.
Finding patterns works properly (and it's probably more useful) without entering the menu.
A search is performed by typing 'f' and then a search pattern up to a newline (sent by <Enter>).
The pattern provided is searched as a substring of any process command line currently in the task list.
Any process whose command line matches pattern is then highlighted until 'f' is pressed to exit the search mode.
The number of matching processes is printed on the same line as the search prompt.
No other types of filtering or refined searches is currently implemented.
To navigate the process list use the up/down arrow keys, page up/down to scroll one page
(puts the cursor on the entry past the last visible process) and the home/end keys to
jump to the first/last process in the list.
The window refresh rate is variable, meaning that it increases as soon as two consecutive
scrolling keys are pressed to enable smooth scrolling, but falls back to normal (1s) when
a key other than a scolling key is pressed. This tradeoff between smoothness and cpu usage
seems reasonable to me.
