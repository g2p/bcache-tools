# bcache
These are the userspace tools required for bcache.

`bcache` is integrated in the Linux kernel (developed in the kernel source tree)
to use SSDs to cache other block
devices. For more information, see http://bcache.evilpiepirate.org.
Documentation for the run time interface is included in the kernel tree, in
[Documentation/bcache.txt](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/bcache.txt). In order to use it you need
to set the `CONFIG_BCACHE` setting in the `.config` file of the linux source
build to `m` (to get built a loadable module) or `y` (to built `bcache` into
the kernel).

# usespace tools

## make-bcache
Formats a block device for use with `bcache`. A device can be formatted for use
as a cache or as a backing device (requires yet to be implemented kernel
support). The most important option is for specifying the bucket size.
Allocation is done in terms of buckets, and cache hits are counted per bucket;
thus a smaller bucket size will give better cache utilization, but poorer write
performance. The bucket size is intended to be equal to the size of your SSD's
erase blocks, which seems to be 128k-512k for most SSDs; feel free to
experiment.

## bcache-super-show
Prints the bcache superblock of a cache device or a backing device.


# udev rules
The first half of the `udev` rules do auto-assembly and add uuid symlinks
to cache and backing devices. If `util-linux`'s `libblkid` is
sufficiently recent (2.24) the rules will take advantage of
the fact that bcache has already been detected.  Otherwise
they call a small probe-bcache program that imitates blkid.

The second half of the rules add symlinks to cached devices,
which are the devices created by the bcache kernel module.

Note that `udev` is developed in the `systemd` source tree.

# initramfs support
Currently `initramfs-tools`, `mkinitcpio` and `dracut` are supported.
