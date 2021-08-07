CC = gcc
CFLAGS = -g -Wall -pedantic
LIBS = -lncurses -lm

.PHONY: all clean

all: src/main.c src/cpu_info.c src/mem_info.c src/process_info.c
	$(CC) $(CFLAGS) `pkg-config --cflags glib-2.0` $^ $(LIBS) `pkg-config --libs glib-2.0`
clean:
	-rm $(wildcard src/*.o)
