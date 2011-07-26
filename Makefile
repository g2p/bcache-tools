
PREFIX=/usr
CFLAGS=-O2 -Wall -g

all: make-bcache probe-bcache

install: make-bcache probe-bcache
	install -m0755 make-bcache ${PREFIX}/sbin/
	install -m0755 probe-bcache /sbin/
	install -m0644 61-bcache.rules /lib/udev/rules.d/
	install -m0755 initramfs /usr/share/initramfs-tools/hooks/bcache
	install -m0644 make-bcache.8 ${PREFIX}/share/man/man8
#	install -m0755 bcache-test ${PREFIX}/sbin/

clean:
	rm -f make-bcache probe-bcache bcache-test *.o

bcache-test: LDLIBS += -lm -lssl -lcrypto
make-bcache: LDLIBS += -luuid
make-bcache: bcache.o
probe-bcache: LDLIBS += -luuid
