
PREFIX=/usr/local
CFLAGS=-O2 -Wall -g

all: make-bcache

clean:
	rm -f make-bcache bcache-test *.o

install: make-bcache
	install -m0755 make-bcache ${PREFIX}/sbin/
	install -m0755 bcache-test ${PREFIX}/sbin/


bcache-test: LDFLAGS += -lm -lssl -lcrypto
