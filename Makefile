CC = gcc
CFLAGS = -g -Wall -pedantic
CFLAGS += $(shell pkg-config --cflags glib-2.0)
LIBS = -lm -lncurses
LIBS += $(shell pkg-config --libs glib-2.0)

.PHONY: all clean

all: src/main.c src/cpu_info.c src/mem_info.c src/process_info.c src/windows.c
	$(CC) $(CFLAGS) $^ $(LIBS)
clean:
	-rm $(wildcard src/*.o)
