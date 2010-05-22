
CFLAGS=-O2 -Wall -g

all: make-bcache bcache-test

clean:
	rm -f make-bcache bcache-test *.o

bcache-test: LDFLAGS += -lm -lssl
