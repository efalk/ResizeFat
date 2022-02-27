/**
 * @file
 * @brief repairfat.c -- Repair a file system
 *
 * This function is a front end to dosfsck, catching certain errors
 * that dosfsck misses.  You should call dosfsck after calling this
 * function.
 *
 * $Id: repairfat.c 3 2011-09-14 03:17:55Z efalk $
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


#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <err.h>
#include <sys/types.h>

#include "repairfat.h"

#define	HEADER_SIZE	512	/* file system header size */

#define	MAX(a,b)	((a)>(b)?(a):(b))

static inline uint16_t
getUInt16(const uint8_t *buffer)
{
	return buffer[1]*256 | buffer[0];
}

static inline uint32_t
getUInt32(const uint8_t *buffer)
{
	return buffer[3]*256*256*256 | buffer[2]*256*256 |
		buffer[1]*256 | buffer[0];
}

static inline void
putUint16(uint8_t *buffer, uint16_t value)
{
	buffer[0] = value & 0xff;
	buffer[1] = (value>>8) & 0xff;
}

static inline void
putUint32(uint8_t *buffer, uint32_t value)
{
	buffer[0] = value & 0xff;
	buffer[1] = (value>>8) & 0xff;
	buffer[2] = (value>>16) & 0xff;
	buffer[3] = (value>>24) & 0xff;
}

/**
 * Repair a FAT file system that's too large for the partition that
 * contains it.  This happens when the partition is reduced in size
 * without first shrinking the file system.
 *
 * This is a brute-force fix that makes no attempt to repair other
 * damage to the file system.  Caller should execute dosfsck immediately
 * after calling this function.
 *
 * @param devname    Name of the device containing the file system
 * @param verbosity  0=quiet, 1=normal, 2=verbose
 * @param notreally  Don't modify file system, just report what needs to be done
 * @param force      Fix file system even if it doesn't need it.
 * @return 0 = file system didn't need to be fixed
 *         1 = file system fixed successfully
 *         2 = failure; unable to open device
 *         3 = failure; unable to determine device size
 *         4 = failure; I/O error
 *	       5 = failure; not a FAT filesystem or not repairable.
 */
int
repairFat(const char *devname, int verbosity, int notreally, int force)
{
	uint8_t buffer[HEADER_SIZE];
	FILE *ifile = NULL;
	off_t devsize;
	int err = 0;
	int bytesPerSector;
	int totalSectors;
	int bootCopy;

	if ((ifile = fopen(devname, notreally ? "r" : "r+")) == NULL)
	    return 2;

	if (fseeko(ifile, (off_t)0, SEEK_END) != 0 ||
	    (devsize = ftello(ifile)) == (off_t)-1 ||
	    fseeko(ifile, (off_t)0, SEEK_SET) != 0)
	{
	    err = 3;
	    goto out;
	}

	if (fread(buffer, sizeof(buffer), 1, ifile) != 1) {
	    err = 4;
	    goto out;
	}

	bytesPerSector = getUInt16(buffer+0xb);
	totalSectors = getUInt16(buffer+0x13);
	if( totalSectors == 0 )
	    totalSectors = getUInt32(buffer+0x20);
	bootCopy = getUInt16(buffer+0x32);

	if (buffer[3] == 0 && bytesPerSector == 0 && totalSectors == 0) {
	    err = 5;
	    goto out;
	}

	if (verbosity > 1) {
	    printf("     Device size: %llu (%llu bytes)\n",
	      devsize/MAX(bytesPerSector,1), devsize);
	    printf("File system size: %u\n", totalSectors);
	}

	if (!force && (off_t)totalSectors * bytesPerSector <= devsize) {
	    if (verbosity > 1)
		printf("No repair needed\n");
	    return 0;
	}

	totalSectors = devsize / MAX(bytesPerSector, 1);
	if (notreally) {
	    printf("Need to set file system size to %u\n", totalSectors);
	} else {
	    if (verbosity > 1)
		printf("Fixing file system by brute force\n");
	    putUint16(buffer+0x13, totalSectors > 65535 ? 0 : totalSectors);
	    putUint32(buffer+0x20, totalSectors);
	    rewind(ifile);
	    if (verbosity > 1)
		printf("Write file system header\n");
	    if (fseeko(ifile, (off_t)0, SEEK_SET) != 0 ||
		fwrite(buffer, sizeof(buffer), 1, ifile) != 1)
	    {
		err = 4;
		goto out;
	    }

	    if (bootCopy != 0) {
		off_t offset = (off_t)bootCopy * bytesPerSector;
		if (verbosity > 1)
		    printf("Write backup file system header to sector %d\n",
			bootCopy);
		if (fseeko(ifile, (off_t)offset, SEEK_SET) != 0 ||
		    fwrite(buffer, sizeof(buffer), 1, ifile) != 1)
		{
		    err = 4;
		    goto out;
		}
	    }
	    if (verbosity > 0)
		printf("File system size corrected\n");
	}

out:
	if (ifile != NULL)
	    fclose(ifile);

	return err;
}
