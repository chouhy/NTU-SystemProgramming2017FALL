.PHONY: all clean a b c d e f g h

# compiler name
CC=gcc

# place options like -O2, -g here
CFLAGS=-O2 -g -std=c99 
LIBFLAGS=-lcrypto

all: miner

miner: miner.c
	$(CC) -o miner $(CFLAGS) miner.c $(LIBFLAGS)

clean:
	rm -f miner

a:
	rm -rf /tmp/ada_*
	./miner ada /tmp/ada_in /tmp/ada_out
b:
	rm -rf /tmp/bbb_*
	./miner bbq /tmp/bbb_in /tmp/bbb_out
c:
	rm -rf /tmp/ccc_*
	./miner ANAL /tmp/ccc_in /tmp/ccc_out
d:
	rm -rf /tmp/cb_*
	./miner asas /tmp/cb_in /tmp/cb_out
e:
	rm -rf /tmp/ca_*
	./miner AssNAL /tmp/ca_in /tmp/ca_out
f:
	rm -rf /tmp/cc_*
	./miner AdsdNAL /tmp/cc_in /tmp/cc_out
g:
	rm -rf /tmp/acc_*
	./miner ADSL /tmp/acc_in /tmp/acc_out
h:
	rm -rf /tmp/vcc_*
	./miner AasNAL /tmp/vcc_in /tmp/vcc_out