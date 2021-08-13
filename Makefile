CC = gcc
CFLAGS = -g -Wall -pedantic
CFLAGS += $(shell pkg-config --cflags glib-2.0)
LIBS = -lm -lmenu -lncurses -lpthread -lrt
LIBS += $(shell pkg-config --libs glib-2.0)

.PHONY: all clean

all: src/main.c src/cpu_info.c src/mem_info.c src/process_info.c src/process_sorting.c src/windows.c src/update_threads.c
	$(CC) $(CFLAGS) $^ $(LIBS)
clean:
	-rm $(wildcard src/*.o)
