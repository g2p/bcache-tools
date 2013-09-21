
PREFIX=/usr
CFLAGS+=-O2 -Wall -g

all: make-bcache probe-bcache bcache-super-show

install: make-bcache probe-bcache bcache-super-show
	install -m0755 make-bcache bcache-super-show	$(DESTDIR)${PREFIX}/sbin/
	install -m0755 probe-bcache	$(DESTDIR)/sbin/
	install -m0644 61-bcache.rules	$(DESTDIR)/lib/udev/rules.d/
	install -m0755 bcache-register	$(DESTDIR)/lib/udev/
	-install -m0755 initramfs/hook	$(DESTDIR)/usr/share/initramfs-tools/hooks/bcache
	install -m0644 -- *.8 $(DESTDIR)${PREFIX}/share/man/man8
#	install -m0755 bcache-test $(DESTDIR)${PREFIX}/sbin/

clean:
	$(RM) -f make-bcache probe-bcache bcache-super-show bcache-test *.o

bcache-test: LDLIBS += `pkg-config --libs openssl`
make-bcache: LDLIBS += `pkg-config --libs uuid blkid`
make-bcache: CFLAGS += `pkg-config --cflags uuid blkid`
make-bcache: bcache.o
probe-bcache: LDLIBS += `pkg-config --libs uuid`
bcache-super-show: LDLIBS += `pkg-config --libs uuid`
bcache-super-show: bcache.o
