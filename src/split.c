/* $Id: split.c,v 1.3 2005/05/13 18:52:06 harbourn Exp $
 * dcfldd - The Enhanced Forensic DD
 * By Nicholas Harbour
 */
/* Copyright 85, 90, 91, 1995-2001, 2005 Free Software Foundation, Inc.
   Copyright 2008                        David Loveall <dave@loveall.org>
   Copyright 2012                        Miah Gregory <mace@debian.org>
   Copyright 2015-2019                   Joao Eriberto Mota Filho <eriberto@eriberto.pro.br>
   Copyright 2019                        Bernhard Übelacker <bernhardu@mailbox.org>
   Copyright 2022                        Tibor Szolnoki <szolnoki.tibor@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.  */

/* GNU dd originally written by Paul Rubin, David MacKenzie, and Stuart Kemp. */

#define _GNU_SOURCE 1

#include "dcfldd.h"

#include <stdio.h>

#include "split.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "full-write.h"
#include "log.h"


/* for portability, use these arrays for referencing numbers and letters */
static char *numbers = "0123456789";
#define NUM_NUMBERS 10
static char *letters = "abcdefghijklmnopqrstuvwxyz";
#define NUM_LETTERS 26

static char *getext(char *, int);
static int maxsplits(char *);

/* Generate a split file extension string based on
 * the specified format string and a given number
 */
static char *getext(char *fmt, int num)
{
    int fmtlen = strlen(fmt);
    int i;
    char *retval;

    assert(fmtlen > 0);

    if (strcmp(fmt, "MAC") == 0) {
      if (num == 0) {
	if(asprintf(&retval, "dmg")) {};
      } else {
	if(asprintf(&retval, "%03d.dmgpart", num+1)) {};
      }
      return retval;
    }

    if (strcmp(fmt, "WIN") == 0) {
      if(asprintf(&retval, "%03d", num+1)) {};
      return retval;
    }

    retval = malloc(fmtlen);

    /* Fill the retval in reverse while constantly dividing num apropriately */
    for (i = fmtlen - 1; i >= 0; i--) {
        int x;

        if (fmt[i] == 'a') {
            x = num % NUM_LETTERS;
            retval[i] = letters[x];
            num = num / NUM_LETTERS;
        } else {
            x = num % NUM_NUMBERS;
            retval[i] = numbers[x];
            num = num / NUM_NUMBERS;
        }
    }

    retval[fmtlen] = '\0';

    return retval;
}

/* Given a format string, determine the maximum number of splits
 * that can be used. */
static int maxsplits(char *fmt)
{
    int fmtlen = strlen(fmt);
    int i;
    int retval = 1;

    assert(fmtlen > 0);

    for (i = fmtlen - 1; i >= 0; i--)
        retval *= fmt[i] == 'a' ? NUM_LETTERS : NUM_NUMBERS;

    return retval;
}

/* Open the next extension in a split sequence */
static void open_split(split_t *split)
{
    int fd;
    int splitnum = split->total_bytes / split->max_bytes;
    mode_t perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    char *ext, *fname;

    ext = getext(split->format, splitnum);
    /* [FIX] split.c:105:5: warning: ignoring return value of ‘asprintf’, declared with attribute warn_unused_result [-Wunused-result] */
    if( asprintf(&fname, "%s.%s", split->name, ext) == -1) {
            free(ext);
            return;
    }
    free(ext);

    fd = open(fname, O_WRONLY | O_CREAT, perms);

    if (fd < 0)
        syscall_error(fname);

    close(split->currfd);
    split->currfd = fd;
    split->curr_bytes = 0;

    free(fname);
}

int split_write(split_t *split, const char *buf, size_t len, int diffwr)
{
    off_t left = split->max_bytes - split->curr_bytes;
    int nwritten = 0;

    if (left == 0 || split->currfd == -1) {
        open_split(split);
        left = split->max_bytes;
    }

    if (len <= left) {
        nwritten = full_write(split->currfd, buf, len, diffwr);
        split->total_bytes += nwritten;
        split->curr_bytes += nwritten;
    } else {
        nwritten = full_write(split->currfd, buf, left, diffwr);
        split->total_bytes += nwritten;
        split->curr_bytes += nwritten;
        nwritten += split_write(split, &buf[nwritten], len - nwritten, diffwr);
    }

    return nwritten;
}
