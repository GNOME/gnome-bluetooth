/*********************************************************************
 *
 * Filename:      obex_io.c
 * Version:       0.3
 * Description:   Some useful disk-IO functions
 * Status:        Experimental.
 * Author:        Pontus Fuchs <pontus.fuchs@tactel.se>
 * Created at:    Mon Aug 07 19:32:14 2000
 * Modified at:   Mon Aug 07 19:32:14 2000
 * Modified by:   Pontus Fuchs <pontus.fuchs@tactel.se>
 * Modified by:   Edd Dumbill <edd@usefulinc.com>
 *
 *     Copyright (c) 1999 Dag Brattli, All Rights Reserved.
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *     MA 02111-1307 USA
 *
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <unistd.h>

#include <fcntl.h>
#include <string.h>


#include <openobex/obex.h>

#include "obex_io.h"

#include <glib.h>

#include "util.h"

extern obex_t *handle;
int obex_protocol_type = OBEX_PROTOCOL_GENERIC;

//
// Get the filesize in a "portable" way
//
int get_filesize(const char *filename)
{
#ifdef _WIN32
	HANDLE fh;
	int size;
	fh = CreateFile(filename, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if(fh == INVALID_HANDLE_VALUE) {
		printf("Cannot open %s\n", filename);
		return -1;
	}
	size = GetFileSize(fh, NULL);
	printf("fize size was %d\n", size);
	CloseHandle(fh);
	return size;

#else
	struct stat stats;
	/*  Need to know the file length */
	stat(filename, &stats);
	return (int) stats.st_size;
#endif
}


//
// Read a file and alloc a buffer for it
//
uint8_t* easy_readfile(const char *filename, int *file_size)
{
	int actual;
	int fd;
	uint8_t *buf;

	*file_size = get_filesize(filename);
	printf("name=%s, size=%d\n", filename, *file_size);

#ifdef _WIN32
	fd = open(filename, O_RDONLY | O_BINARY, 0);
#else
	fd = open(filename, O_RDONLY, 0);
#endif

	if (fd == -1) {
		return NULL;
	}

	if(! (buf = malloc(*file_size)) )	{
		return NULL;
	}

	actual = read(fd, buf, *file_size);
	close(fd);

#ifdef _WIN32
	if(actual != *file_size) {
		free(buf);
		buf = NULL;
	}
#else
	*file_size = actual;
#endif
	return buf;
}


/*
 * Function safe_save_file ()
 *
 */
int safe_save_file(char *name, const uint8_t *buf, int len,
				   const char *dir)
{
  gchar *filename;
  int fd;
  int actual;

  printf("Filename = %s\n", name);

  filename=get_safe_unique_filename(name, dir);

#ifdef _WIN32
  fd = open(filename, O_RDWR | O_CREAT, 0);
#else
  fd = open(filename, O_RDWR | O_CREAT, DEFFILEMODE);
#endif

  if (fd < 0) {
	perror(filename);
	g_free(filename);
	return -1;
  }

  actual = write(fd, buf, len);
  close(fd);

  printf( "Wrote %s (%d bytes)\n", filename, actual);

  g_free(filename);

  return actual;
}
