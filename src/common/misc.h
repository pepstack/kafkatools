/***********************************************************************
 * Copyright (c) 2008-2080 pepstack.com, 350137278@qq.com
 *
 * ALL RIGHTS RESERVED.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **********************************************************************/

/**
 * @filename   misc.h
 *    miscellaneous tools for application.
 *
 * @author     Liang Zhang <350137278@qq.com>
 * @version    0.0.10
 * @create     2017-08-28 11:12:10
 * @update     2020-05-06 22:55:37
 */
#ifndef _MISC_H_
#define _MISC_H_

#if defined(__cplusplus)
extern "C"
{
#endif

#include <time.h>

#include "cstrbuf.h"

#if defined(__WINDOWS__)
    # include <Windows.h>

    typedef HANDLE filehandle_t;

    # if !defined(HAVE_MODE_T)
    typedef unsigned int mode_t;
    # endif

    # define filehandle_invalid  INVALID_HANDLE_VALUE
#else
    #include <sys/types.h>
    # include <sys/stat.h>
    # include <fcntl.h>

    typedef int    filehandle_t;

    # define filehandle_invalid  ((filehandle_t)(-1))
#endif


/**
 * time api
 */
const char * timezone_format (long tz, char *tzfmt);
long timezone_compute (time_t ts, char *tzfmt);
int daylight_compute (time_t ts);
void getnowtimeofday (struct timespec *now);
void getlocaltime_safe (struct tm *loc, int64_t t, int tz, int dst);


/**
 * file api
 */
filehandle_t file_create (const char *pathname, int flags, mode_t mode);
filehandle_t file_open_read (const char *pathname);
int file_close (filehandle_t *phf);

int file_readbytes (filehandle_t hf, char *bytesbuf, ub4 sizebytes);
int file_writebytes (filehandle_t hf, const char *bytesbuf, ub4 sizebytes);

int pathfile_exists (const char *pathname);
int pathfile_remove (const char *pathname);

int pathfile_move (const char *pathnameOld, const char *pathnameNew);

cstrbuf get_proc_abspath (void);
cstrbuf find_config_pathfile (const char *cfgpath, const char *cfgname, const char *envvarname, const char *etcconfpath);

#ifdef __cplusplus
}
#endif
#endif /* _MISC_H_ */
