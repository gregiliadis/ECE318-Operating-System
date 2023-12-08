/*
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  There are a couple of symbols that need to be #defined before
  #including all the headers.
*/

#ifndef _PARAMS_H_
#define _PARAMS_H_

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
#define FUSE_USE_VERSION 26

// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
#define _XOPEN_SOURCE 500

// maintain bbfs state in here
#include <openssl/sha.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

struct bb_state {
    FILE *logfile;
    char *rootdir; 
    FILE *storage;
    FILE *temp;
};

#define BB_DATA ((struct bb_state *) fuse_get_context()->private_data)

#define BLOCK_SIZE 512
#define HASH_SIZE 20
#define SHA_DIGEST_LENGTH 20

int find_block (FILE *storage, unsigned char *buf, unsigned char *hash);
void create_temp_file (int fd_storage, int temp_file, int file_to_read, off_t file_length);
void garbage_collector (int );
void fragment_temp_file (int fd_storage, int fd_dest_file, int fd_temp_file, off_t file_length);

#endif
