/*
 * resizefat.c -- Resize a file system
 *
 * $Id: resizefat.c 3 2011-09-14 03:17:55Z efalk $
 */

/*
 * (C) Copyright 2011 Hewlett-Packard Development Company, L.P.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <parted/parted.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "repairfat.h"

static int verbose_flag = 0;
static int repair_only = 0;

const char short_opts[] = "vhr";

const struct option long_opts[] = {
	{ "verbose", no_argument, NULL, 'v' },
	{ "repair", no_argument, NULL, 'r' },
	{ "help", no_argument, NULL, 'h' },
	{ 0, 0, 0, 0 },
};

void usage(void)
{
	printf("Usage: resizefat [-v] [-r] [-h] <device> [<size>]\n");
	printf("\t-v	Verbose progress report.\n");
	printf("\t-r	Repair file system only.\n");
	printf("\nSize is a decimal number with optional suffix [BSKMG]\n");
	printf("(bytes, sectors, kilobytes, megabytes, gigabytes)\n");
	printf("Default is M.  Size is not used with -r\n");
}

void process_args(int argc, char **argv)
{
	while (1) {
		int option_index, c;
		c = getopt_long(argc, argv, short_opts, long_opts,
			&option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'v':
			verbose_flag = 1;
			break;
		case 'r':
			repair_only = 1;
			break;
		case 'h':
			usage();
			exit(EX_OK);
		}
	}
}

static PedSector
byte_size(const char *sizeStr)
{
	const char *sfx;
	PedSector size = atoll(sizeStr);
	sfx = sizeStr + strlen(sizeStr) - 1;
	switch (*sfx) {
	case 'B': break;
	case 'S': size *= 512; break;
	case 'K': size *= 1024; break;
	default:
	case 'M': size *= 1024*1024; break;
	case 'G': size *= 1024*1024*1024; break;
	}
	return size;
}

void timer_handler(PedTimer *timer, void *context)
{
	printf("%.0f percent complete.\n", timer->frac*100);
}

static PedExceptionOption
parted_exception_ignore(PedException *ex)
{
	return PED_EXCEPTION_IGNORE;
}

int main(int argc, char **argv)
{
	int err = 0;
	char cmd[200];

	/* Device and device level constraint */
	PedDevice *dev;
	PedGeometry *dev_geometry;

	/* Target filesystem to resize */
	PedFileSystem *fs;
	PedConstraint *resize_constraint;
	PedGeometry *new_geometry;

	PedSector new_size = 0;
	PedExceptionHandler *old_handler;

	/* Optional Timer to report progress */
	PedTimer *timer = NULL;

	process_args(argc, argv);

	argc -= optind;
	argv += optind;
	if (argc < (repair_only ? 1 : 2)) {
		usage();
		exit(1);
	}

	const char *resize_dev = *argv++;

	if (!repair_only)
		new_size = byte_size(*argv++) / 512;

	/*
	 * In the field, we've discovered FAT file systems which could
	 * not be repaired by dosfsck.  The repairFat() function handles
	 * these cases.  Call it before calling dosfsck.  We'll ignore
	 * the return code; if the file system isn't repairable, we'll
	 * find out soon enough.
	 */
	if ((err = repairFat(resize_dev, verbose_flag ? 2 : 1, 0, 0)) >= 2) {
		fprintf(stderr,
			"Unable to repair filesystem %s, repairFat returns %d\n",
			resize_dev, err);
		exit(err);
	}

	snprintf(cmd, sizeof(cmd), "dosfsck -a %s", resize_dev);
	if (verbose_flag)
		printf("check filesystem: %s\n", cmd);

	/* If return code 0, success.  If 1, try again.  If > 1, fail
	 * If second try does not return 0, fail.
	 */
	if ((err = system(cmd)) != 0)
	{
		if ((WIFEXITED(err) && WEXITSTATUS(err) > 1) ||
			(err = system(cmd)) != 0)
		{
			fprintf(stderr,
				"Unable to repair filesystem %s, exit code %d\n",
				resize_dev, WEXITSTATUS(err));
			exit(5);
		}
	}

	if (repair_only)
		goto out;

	/* Get the device (such as "/dev/sda") */
	dev = ped_device_get(resize_dev);
	if (!dev) {
		printf("Unable to access device \"%s\".\n", resize_dev);
		err = 2;
		goto out;
	}

	if (!ped_device_open(dev)) {
		printf("Unable to open device \"%s\".\n", resize_dev);
		err = 2;
		goto out;
	}

	if (verbose_flag)
		printf("Device %s, size %lld sectors\n", resize_dev, dev->length);

	/* Create a geometry description spanning entire device */
	dev_geometry = ped_geometry_new(dev, 0, dev->length);

	if (new_size > dev->length) {
		printf("New size reduced to maximum available of %lld sectors\n",
			dev->length);
		new_size = dev->length;
	}

	/* Open file system, ignore exceptions caused by bad CHS geometry. */
	old_handler = ped_exception_get_handler();
	ped_exception_set_handler(parted_exception_ignore);
	fs = ped_file_system_open(dev_geometry);
	ped_exception_set_handler(old_handler);
	if (!fs) {
		printf("Unable to locate filesystem on device \"%s\".\n",
			resize_dev);
		err = 5;
		goto out;
	}

	resize_constraint = ped_file_system_get_resize_constraint(fs);
	if (!resize_constraint) {
		printf("Unable to retreive filesystem statistics from \"%s\" "
			"for resize.\n", resize_dev);
		err = 5;
		goto out;
	}

	if (new_size < resize_constraint->min_size) {
		printf("Unable to resize filesystem.  Minimum size %llu "
			"sectors, %llu requested.\n",
			resize_constraint->min_size, new_size);
		err = 6;
		goto out;
	}

	new_geometry = ped_geometry_new(dev, 0, new_size);

	if (verbose_flag) {
		timer = ped_timer_new(timer_handler, 0);
	}

	if (!ped_file_system_resize(fs, new_geometry, timer)) {
		printf("Operation failed!!\n");
		err = 4;
	}

	ped_device_close(dev);

	ped_device_free_all();

out:
	return err;
}

