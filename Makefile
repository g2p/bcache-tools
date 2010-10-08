
#PREFIX=/usr/local
CFLAGS=-O2 -Wall -g

all: make-bcache probe-bcache

clean:
	rm -f make-bcache bcache-test *.o

install: make-bcache probe-bcache
	install -m0755 make-bcache ${PREFIX}/sbin/
	install -m0755 probe-bcache ${PREFIX}/sbin/
	install -m0644 61-bcache.rules /lib/udev/rules.d/
	install -m0755 initramfs /usr/share/initramfs-tools/hooks/bcache
#	install -m0755 bcache-test ${PREFIX}/sbin/


bcache-test: LDFLAGS += -lm -lssl -lcrypto
make-bcache: LDFLAGS += -luuid
probe-bcache: LDFLAGS += -luuid
