.PHONY: all clean run

# compiler name
CC=gcc

# place options like -O2, -g here
CFLAGS=-O2 -g
LIBFLAGS=-lcrypto

all: boss

boss: boss.c boss.h
	$(CC) -o boss $(CFLAGS) boss.h boss.c $(LIBFLAGS)

clean:
	rm -f boss
run:
	./boss ../config.txt
