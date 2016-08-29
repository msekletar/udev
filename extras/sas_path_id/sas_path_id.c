/* Copyright (C) 2014  Maurizio Lombardi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <libudev.h>
#include <string.h>
#include <stdlib.h>

static int format_lun_number(struct udev_device *dev, char **lun_str);

int main(int argc, char **argv)
{
	struct udev *udev;
	struct udev_device *parent;
	struct udev_device *dev;
	struct udev_device *targetdev;
	struct udev_device *target_parent;
	struct udev_device *port;
	struct udev_device *expander;
	struct udev_device *target_sasdev = NULL;
	struct udev_device *expander_sasdev = NULL;
	struct udev_device *port_sasdev = NULL;
	char *devname;
	const char *phy_count;
	const char *phy_id;
	const char *exp_sas_addr = NULL;
	char *lun = NULL;
	char *old_symlink = NULL;
	int r = 1;

	if (argc < 2) {
		fprintf(stderr, "No node specified\n");
		goto exit;
	} else if (argc > 3) {
		fprintf(stderr, "Too many arguments\n");
		goto exit;
	}

	udev = udev_new();
	if (!udev) {
		fprintf(stderr, "Can't create udev\n");
		goto exit;
	}

	if (argc == 3) {
		char *sas_str;

		old_symlink = argv[2];

		/* Get the last component of the old by-path symlink" */
		sas_str = strstr(old_symlink, "sas-");
		if (sas_str) {
			/* truncate the last component of the old by-path link */
			*sas_str = 0;
		} else
			old_symlink = NULL;
	}

	/* Get the last component of the device's path, i.e. /dev/sda1 ---> sda1 */

	devname = strrchr(argv[1], '/');
	if (!devname) {
		fprintf(stderr, "Unable to open %s\n", argv[1]);
		goto exit;
	}

	devname++;

	dev = udev_device_new_from_subsystem_sysname(udev, "block", devname);
	if (!dev) {
		fprintf(stderr, "Unable to open %s\n", argv[1]);
		goto exit;
	}

	parent = udev_device_get_parent(dev);
	if (!parent)
		goto exit;

	targetdev = udev_device_get_parent_with_subsystem_devtype(dev, "scsi", "scsi_target");
	if (!targetdev)
		goto exit;

	target_parent = udev_device_get_parent(targetdev);
	if (!target_parent)
		goto exit;

	/* Get SAS device */
	target_sasdev = udev_device_new_from_subsystem_sysname(udev, "sas_device",
							udev_device_get_sysname(target_parent));
	if (!target_sasdev) {
		fprintf(stderr, "Cannot get sas_device\n");
		goto exit;
	}

	port = udev_device_get_parent(target_parent);
	if (!port)
		goto exit;

	/* Get port device */
	port_sasdev = udev_device_new_from_subsystem_sysname(udev,
						"sas_port", udev_device_get_sysname(port));
	if (!port_sasdev) {
		fprintf(stderr, "Cannot get port device\n");
		goto exit;
	}

	phy_count = udev_device_get_sysattr_value(port_sasdev, "num_phys");
	if (!phy_count) {
		fprintf(stderr, "Cannot read num_phys\n");
		goto exit;
	}

	/* Check if we are a simple disk */
	if (strncmp(phy_count, "1", 2)) {
		/* This is not a simple disk, keep old symlink format */
		r = 0;
		goto exit;
	}

	phy_id = udev_device_get_sysattr_value(target_sasdev,
					"phy_identifier");

	/* The port's parent is either HBA or expander */
	expander = udev_device_get_parent(port);

	expander_sasdev = udev_device_new_from_subsystem_sysname(udev,
					"sas_device", udev_device_get_sysname(expander));

	if (expander_sasdev) {
		/* Get expander SAS address */
		exp_sas_addr = udev_device_get_sysattr_value(expander_sasdev,
							"sas_address");
		if (!exp_sas_addr) {
			fprintf(stderr, "Cannot read the expander SAS address\n");
			goto exit;
		}
	}

	/* Get the LUN number */
	if (format_lun_number(udev_device_get_parent(dev), &lun) == 1) {
		fprintf(stderr, "Memory allocation failed\n");
		goto exit;
	}

	printf("ID_SAS_PATH=");

	if (old_symlink)
		printf("%s", old_symlink);
	if (exp_sas_addr)
		printf("sas-exp%s-phy%s-%s\n", exp_sas_addr, phy_id, lun);
	else
		printf("sas-phy%s-%s\n", phy_id, lun);

	r = 0;

exit:
	udev_device_unref(target_sasdev);
	udev_device_unref(expander_sasdev);
	udev_device_unref(port_sasdev);

	free(lun);

	return r;
}

static int format_lun_number(struct udev_device *dev, char **lun_str)
{
	unsigned long lun = strtoul(udev_device_get_sysnum(dev), NULL, 10);

	/* address method 0, peripheral device addressing with bus id of zero */
	if (lun < 256)
		return asprintf(lun_str, "lun-%lu", lun);
	/* handle all other lun addressing methods by using a variant of the original lun format */
	return asprintf(lun_str, "lun-0x%04lx%04lx00000000", lun & 0xffff, (lun >> 16) & 0xffff);
}

