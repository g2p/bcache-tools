
CFLAGS+=-O2 -Wall -g

PREFIX=/usr
SBINDIR=/sbin
LIBDIR=/lib
MANDIR=${PREFIX}/share/man

all: make-bcache probe-bcache bcache-super-show

install: make-bcache probe-bcache bcache-super-show
	install -p -m0755 make-bcache bcache-super-show	$(DESTDIR)$(PREFIX)$(SBINDIR)
	install -p -m0755 probe-bcache			$(DESTDIR)$(SBINDIR)
	install -p -m0644 69-bcache.rules		$(DESTDIR)$(LIBDIR)/udev/rules.d
	install -p -m0755 bcache-register		$(DESTDIR)$(LIBDIR)/udev
	-install -T -p -m0755 initramfs/hook		$(DESTDIR)/usr/share/initramfs-tools/hooks/bcache
	install -p -m0644 -- *.8			$(DESTDIR)${MANDIR}/man8
	-install -d -m0755				$(DESTDIR)$(LIBDIR)/dracut/modules.d/90bcache
	-install -p -m0755 dracut/module-setup.sh	$(DESTDIR)$(LIBDIR)/dracut/modules.d/90bcache/
#	install -m0755 bcache-test $(DESTDIR)${PREFIX}$(SBINDIR)/

clean:
	$(RM) -f make-bcache probe-bcache bcache-super-show bcache-test *.o

bcache-test: LDLIBS += `pkg-config --libs openssl`
make-bcache: LDLIBS += `pkg-config --libs uuid blkid`
make-bcache: CFLAGS += `pkg-config --cflags uuid blkid`
make-bcache: bcache.o
probe-bcache: LDLIBS += `pkg-config --libs uuid blkid`
probe-bcache: CFLAGS += `pkg-config --cflags uuid blkid`
bcache-super-show: LDLIBS += `pkg-config --libs uuid`
bcache-super-show: CFLAGS += -std=gnu99
bcache-super-show: bcache.o
