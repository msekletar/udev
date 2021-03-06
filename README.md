[![Build Status](https://travis-ci.org/msekletar/udev.svg?branch=master)](https://travis-ci.org/msekletar/udev)
[![Coverity Scan Status](https://scan.coverity.com/projects/msekletar-udev/badge.svg)](https://scan.coverity.com/projects/msekletar-udev)

# udev - userspace device management

Integrating udev in the system has complex dependencies and differs from distro
to distro. All major distros depend on udev these days and the system may not
work without a properly installed version. The upstream udev project does not
recommend to replace a distro's udev installation with the upstream version.

Tools and rules shipped by udev are not public API and may change at any time.
Never call any private tool in /lib/udev from any external application, it might
just go away in the next release. Access to udev information is only offered
by udevadm and libudev. Tools and rules in /lib/udev, and the entire content of
the /dev/.udev directory is private to udev and does change whenever needed.

## Requirements

* Version 2.6.27 of the Linux kernel with sysfs, procfs, signalfd, inotify, unix
  domain sockets, networking and hotplug enabled:
  * CONFIG_HOTPLUG=y
  * CONFIG_UEVENT_HELPER_PATH=""
  * CONFIG_NET=y
  * CONFIG_UNIX=y
  * CONFIG_SYSFS=y
  * CONFIG_SYSFS_DEPRECATED*=n
  * CONFIG_PROC_FS=y
  * CONFIG_TMPFS=y
  * CONFIG_INOTIFY_USER=y
  * CONFIG_SIGNALFD=y
  * CONFIG_TMPFS_POSIX_ACL=y (user ACLs for device nodes)
  * CONFIG_BLK_DEV_BSG=y (SCSI devices)

* For reliable operations, the kernel must not use the CONFIG_SYSFS_DEPRECATED
  option.

* Unix domain sockets (CONFIG_UNIX) as a loadable kernel module may work, but it
  is not supported.

* The proc filesystem must be mounted on /proc, the sysfs filesystem must be
  mounted at /sys. No other locations are supported by udev.

* The system must have the following group names resolvable at udev startup:
  disk, cdrom, floppy, tape, audio, video, lp, tty, dialout, kmem.  Especially
  in LDAP setups, it is required, that getgrnam() is able to resolve these group
  names with only the rootfs mounted, and while no network is available.

* To build all udev extras, libacl, libglib2, libusb, usbutils, pciutils, gperf
  are needed. These dependencies can be disabled with the --disable-extras
  option.

## Operation

Udev creates and removes device nodes in /dev, based on events the kernel sends
out on device discovery or removal.

* Early in the boot process, the /dev directory should get a 'tmpfs'
  filesystem mounted, which is maintained by udev. Created nodes or changed
  permissions will not survive a reboot, which is intentional.

* The content of /lib/udev/devices directory which contains the nodes,
  symlinks and directories, which are always expected to be in /dev, should
  be copied over to the tmpfs mounted /dev, to provide the required nodes
  to initialize udev and continue booting.

* The deprecated hotplug helper /sbin/hotplug should be disabled in the
  kernel configuration, it is not needed today, and may render the system
  unusable because the kernel may create too many processes in parallel
  so that the system runs out-of-memory.

* All kernel events are matched against a set of specified rules in
  /lib/udev/rules.d which make it possible to hook into the event
  processing to load required kernel modules and setup devices. For all
  devices the kernel exports a major/minor number, udev will create a
  device node with the default kernel name, or the one specified by a
  matching udev rule.

Please direct any comment/question to the linux-hotplug mailing list at:
  linux-hotplug@vger.kernel.org

