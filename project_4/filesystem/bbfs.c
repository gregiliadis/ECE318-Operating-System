
/*
  Big Brother File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
  
  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

  This might be called a no-op filesystem:  it doesn't impose
  filesystem semantics on top of any other existing structure.  It
  simply reports the requests that come in, and passes them to an
  underlying filesystem.  The information is saved in a logfile named
  bbfs.log, in the directory from which you run bbfs.
*/
#include "config.h"
#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <openssl/sha.h>
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void bb_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, BB_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
				    // break here

    log_msg("    bb_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
	    BB_DATA->rootdir, path, fpath);
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int bb_getattr(const char *path, struct stat *statbuf)
{
    int retstat;
    char fpath[PATH_MAX];
    int fd;
    log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
    bb_fullpath(fpath, path);
    
    retstat = log_syscall("lstat", lstat(fpath, statbuf), 0);
    fd = open (fpath, O_RDONLY);
    log_syscall ("read", read (fd, &statbuf->st_size, sizeof(off_t)), 0);
    log_msg("%s Size is %d",fpath, statbuf->st_size);
    log_stat(statbuf);
    
    return retstat;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to bb_readlink()
// bb_readlink() code by Bernardo F Costa (thanks!)
int bb_readlink(const char *path, char *link, size_t size)
{
    int retstat;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    bb_fullpath(fpath, path);

    retstat = log_syscall("readlink", readlink(fpath, link, size - 1), 0);
    if (retstat >= 0) {
	link[retstat] = '\0';
	retstat = 0;
	log_msg("    link=\"%s\"\n", link);
    }
    
    return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int bb_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	  path, mode, dev);
    bb_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, dev)' but this
    // tries to be be more portable by honoring the quote in the Linux
    // mknod man page stating the only portable use of mknod() is to
    // make a fifo, but saying it should never actually be used for
    // that.
    if (S_ISREG(mode)) {
	retstat = log_syscall("open", open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
	if (retstat >= 0)
	    retstat = log_syscall("close", close(retstat), 0);
    } else
	if (S_ISFIFO(mode))
	    retstat = log_syscall("mkfifo", mkfifo(fpath, mode), 0);
	else
	    retstat = log_syscall("mknod", mknod(fpath, mode, dev), 0);
    
    return retstat;
}

/** Create a directory */
int bb_mkdir(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
    bb_fullpath(fpath, path);

    return log_syscall("mkdir", mkdir(fpath, mode), 0);
}

/** Remove a file */
int bb_unlink(const char *path)
{
    char fpath[PATH_MAX];
    int fd, fd_storage;
    int storage_pos = 0;
    int references;
    unsigned char hash[SHA_DIGEST_LENGTH];
    int buffer_size = SHA_DIGEST_LENGTH + sizeof(off_t) + BLOCK_SIZE + sizeof(int);
    char buffer[buffer_size];
    off_t file_length;
    int index, blocks;

    log_msg("bb_unlink(path=\"%s\")\n",path);
    bb_fullpath(fpath, path);
    //CALCULATE FILE'S SIZE
    fd = open (fpath, O_RDONLY);
    fd_storage = fileno (BB_DATA->storage);
    //move cursor to the last file position.
    file_length = lseek (fd, (size_t) 0, SEEK_END);
    lseek (fd, (size_t) 0, SEEK_SET);
    blocks = (file_length - sizeof(off_t)) / HASH_SIZE;
    log_msg ("There are %d blocks.\n");	
    /*Remove the file's block with 0 references.*/
    for (index = 0; index < blocks; index++){	
	/*Read #index block hash*/
        pread (fd, hash, HASH_SIZE, index * HASH_SIZE + sizeof(off_t));
    	storage_pos = find_block (BB_DATA->storage, NULL, hash);
	pread (fd_storage, &references, sizeof(int), storage_pos + buffer_size - sizeof(int));
	references--;
	if(references == 0){/*Move the last block to the position of the block will be deleted.*/
            log_syscall("pread", pread(fd_storage, buffer, buffer_size, lseek(fd_storage, 0, SEEK_END) - buffer_size), 0);

            log_syscall("pwrite", pwrite(fd_storage, buffer, buffer_size, storage_pos), 0); 

            ftruncate(fd_storage, lseek(fd_storage, 0, SEEK_END)- buffer_size);      
        }
	else{
	    log_syscall("pwrite", pwrite (fd_storage, &references, sizeof(int), storage_pos + buffer_size - sizeof(int)), 0);
	}
    }	
    return log_syscall("unlink", unlink(fpath), 0);
}

/** Remove a directory */
int bb_rmdir(const char *path)
{
    char fpath[PATH_MAX];
    
    log_msg("bb_rmdir(path=\"%s\")\n",
	    path);
    bb_fullpath(fpath, path);

    return log_syscall("rmdir", rmdir(fpath), 0);
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int bb_symlink(const char *path, const char *link)
{
    char flink[PATH_MAX];
    
    log_msg("\nbb_symlink(path=\"%s\", link=\"%s\")\n",
	    path, link);
    bb_fullpath(flink, link);

    return log_syscall("symlink", symlink(path, flink), 0);
}

/** Rename a file */
// both path and newpath are fs-relative
int bb_rename(const char *path, const char *newpath)
{
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    
    log_msg("\nbb_rename(fpath=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    bb_fullpath(fpath, path);
    bb_fullpath(fnewpath, newpath);

    return log_syscall("rename", rename(fpath, fnewpath), 0);
}

/** Create a hard link to a file */
int bb_link(const char *path, const char *newpath)
{
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    log_msg("\nbb_link(path=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    bb_fullpath(fpath, path);
    bb_fullpath(fnewpath, newpath);

    return log_syscall("link", link(fpath, fnewpath), 0);
}

/** Change the permission bits of a file */
int bb_chmod(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_chmod(fpath=\"%s\", mode=0%03o)\n",
	    path, mode);
    bb_fullpath(fpath, path);

    return log_syscall("chmod", chmod(fpath, mode), 0);
}

/** Change the owner and group of a file */
int bb_chown(const char *path, uid_t uid, gid_t gid)
  
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_chown(path=\"%s\", uid=%d, gid=%d)\n",
	    path, uid, gid);
    bb_fullpath(fpath, path);

    return log_syscall("chown", chown(fpath, uid, gid), 0);
}

/** Change the size of a file */
int bb_truncate(const char *path, off_t newsize)
{
    char fpath[PATH_MAX];
    int fd, temp_file, fd_storage, ret;
    off_t file_length;

    log_msg("\nbb_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);
    bb_fullpath(fpath, path);
    /*Since no fuse file info struct is passed, we have to take 
      the file descriptor from the file image in order to apply
      truncate operation.*/
    fd = log_syscall("open",open(fpath, O_RDWR),0);
     
    file_length = lseek (fd, (size_t) 0, SEEK_END);
    
    log_msg ("\nbb_truncate-> %d bytes\n", file_length);
    //how many blocks the file has?
    if(file_length > 0 ) {
        lseek(fd, (size_t)0, SEEK_SET); 
        log_syscall ("read", read(fd, &file_length, sizeof(off_t)), 0);
    }
 
    temp_file = fileno (BB_DATA->temp);
    
    fd_storage = fileno(BB_DATA->storage); 
    if (file_length > 0){
    	create_temp_file (fd_storage, temp_file, fd, file_length);
        file_length = lseek(temp_file, (size_t)0, SEEK_END);
    }
     
    //execute operation in temporary file and store back to storage file
    ret = log_syscall("ftruncate", ftruncate(temp_file, newsize), 0);
    /* Compute new size. */
    file_length = newsize;
    fragment_temp_file (fd_storage, fd, temp_file, file_length); 	
    /*Store the changes at fd.*/
    log_syscall ("fsync", fsync (fd), 0);
    ftruncate (temp_file, 0);
    lseek (temp_file, 0, SEEK_SET);
    /*Remove the blocks that no file is using.*/  
    garbage_collector (fd_storage);

    return ret;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int bb_utime(const char *path, struct utimbuf *ubuf)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_utime(path=\"%s\", ubuf=0x%08x)\n",
	    path, ubuf);
    bb_fullpath(fpath, path);

    return log_syscall("utime", utime(fpath, ubuf), 0);
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int bb_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_open(path\"%s\", fi=0x%08x)\n", path, fi);
    bb_fullpath(fpath, path);
    if (strcmp (path, ".storage") == 0 || strcmp (path, ".temp") == 0){
	fi->fh = -1;
	retstat = -1;
  
	return retstat;
    }
    // if the open call succeeds, my retstat is the file descriptor,
    // else it's -errno.  I'm making sure that in that case the saved
    // file descriptor is exactly -1.  
    /*Since we see the fpath file as an inode, we have to be able to 
      read and write from this file, so O_RDWR flag is passed to open.*/
    fd = log_syscall("open", open(fpath, O_RDWR), 0);
    if (fd < 0)
	retstat = log_error("open");
	
    fi->fh = fd;

    log_fi(fi);
    
    return retstat;
}
/*This function finds the block in storage file given its hash code*/
int find_block (FILE *storage, unsigned char *buf, unsigned char *hash){
    int fd = fileno (storage);
    off_t offset = 0;
    off_t size_of_block;
    char temp [HASH_SIZE];	
    int ret; 
   
    lseek (fd, (size_t) 0, SEEK_SET);    
    while(1){
        ret = log_syscall("read",read (fd, temp, HASH_SIZE),0);
	
        if (ret == 0){ /*No more to read; break*/
                 break; 
	}
        
	if (memcmp (temp, hash, HASH_SIZE) == 0){/*Hash read equal to hash given.*/
		if (buf != NULL){
			log_syscall ("read",read (fd, &size_of_block, sizeof(off_t)),0);
			log_syscall ("read",read (fd, buf, size_of_block),0);
			offset = lseek(fd, 0, SEEK_CUR) - HASH_SIZE - size_of_block - sizeof(off_t);
		}
		else{
			offset = lseek(fd, 0, SEEK_CUR) - HASH_SIZE;
		}
	    	return (offset);
        }
        lseek (fd, BLOCK_SIZE + sizeof(int) + sizeof(off_t), SEEK_CUR);
    }	       
   
   return (-1);
}
/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int bb_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    off_t file_length;
    int blocks;
    int index, first_block, last_block;
    unsigned char data [BLOCK_SIZE] = { '\0' };
    unsigned char hash [HASH_SIZE];
    int block_offset;
    int request = 0;
    off_t bytes_read = 0;

    log_msg("\nbb_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
    
    //CALCULATE FILE'S SIZE
    //move cursor to the last file position.
    file_length = lseek (fi->fh, (size_t) 0, SEEK_END);
    log_msg ("\nbb_read-> %d bytes\n", file_length);
    //how many blocks the file has?
    blocks = (file_length - sizeof (off_t)) / HASH_SIZE; //20 stands for size of hascode
    
    //which blocks should i check?
    first_block = offset / BLOCK_SIZE;
    
    last_block = ((offset + size ) / BLOCK_SIZE) > blocks ? blocks - 1: (offset + size ) / BLOCK_SIZE ;
    
    for (index = first_block; index <= last_block; index++){	
    	log_syscall ("pread", pread (fi->fh, hash, HASH_SIZE, index * HASH_SIZE + sizeof(off_t)),0);
    	find_block (BB_DATA->storage, data, hash);
	// fragment
        if (index == first_block ){
		block_offset = offset - first_block * BLOCK_SIZE;
 		memcpy (buf, data + block_offset, BLOCK_SIZE - block_offset);
                bytes_read = BLOCK_SIZE - block_offset;
                size = size - BLOCK_SIZE + block_offset;
 		request += BLOCK_SIZE - block_offset;
 	}
	else if ( index == last_block){
		memcpy (buf + bytes_read, data, size);
                 
 	 	request += size;
        }
        else{
            memcpy (buf + bytes_read, data, BLOCK_SIZE);
            bytes_read += BLOCK_SIZE;  
	    size = size - BLOCK_SIZE;	
	    request += BLOCK_SIZE;	
        }
    } 
    
    return (request);          	
} 

/*This function creates a temporary file to use it for any operation.*/
void create_temp_file (int fd_storage, int temp_file, int file_to_read, off_t file_length){
    unsigned char hash[HASH_SIZE] = {'\0'};
    unsigned char data[BLOCK_SIZE] = {'\0'};

    int index, blocks, storage_pos;
    int reference;
        
    blocks = (file_length % BLOCK_SIZE) != 0 ? (file_length / BLOCK_SIZE) + 1 : (file_length / BLOCK_SIZE); //20 stands for size of hascode
    
    //read disk and recreate the temporary file
    for (index = 0 ; index < blocks; index++){
        pread (file_to_read, hash, HASH_SIZE, (index * HASH_SIZE) + sizeof(off_t) );
        storage_pos = find_block (BB_DATA->storage, data, hash);
	
        log_syscall("pread",pread(fd_storage, &reference, sizeof(int), storage_pos + HASH_SIZE + sizeof(off_t) + BLOCK_SIZE),0);
        /*Every time we read a block from the storage we assume that will be changed.*/
	reference--;

	log_syscall("pwrite",pwrite(fd_storage, &reference, sizeof(int), storage_pos + HASH_SIZE + sizeof(off_t) + BLOCK_SIZE),0);
	/*Write to temp file.*/
        if( file_length > BLOCK_SIZE ) {
	    	log_syscall("write",write(temp_file, data, BLOCK_SIZE),0);
	    	file_length -= BLOCK_SIZE;    
	}
        else { 
        	log_syscall("write",write(temp_file, data, file_length),0);
            	file_length = 0;
        }

    }
}

/*Make new blocks from the temp file and store them to storage file*/
void fragment_temp_file (int fd_storage, int fd_dest_file, int fd_temp_file, off_t file_length){
    unsigned char data[BLOCK_SIZE] = { '\0' };
    unsigned char hash[HASH_SIZE]  = { '\0' };
    int block_counter;
    int storage_pos, reference; 
    off_t length; 
    
    ftruncate(fd_dest_file, 0);
    log_msg ("\nFile length of temp_file is %d.\n", file_length);
    log_syscall ("pwrite", pwrite(fd_dest_file, &file_length, sizeof(off_t), (size_t)0), 0);
    
    //blocks = (file_length % BLOCK_SIZE) != 0 ? (file_length / BLOCK_SIZE) + 1 : (file_length / BLOCK_SIZE); //20 stands for size of hascode
    //open disk
    lseek( fd_storage, 0, SEEK_SET);
    
    block_counter = 0;
    //fragment in blocks
    if (file_length <= 0){ return; }
    do{
       /*How many bytes should i read?*/
	if(file_length > BLOCK_SIZE) {
            log_syscall("pread",pread(fd_temp_file, data, BLOCK_SIZE, BLOCK_SIZE*block_counter),0);
            length = BLOCK_SIZE;
        }
        else {
            log_syscall("pread",pread(fd_temp_file, data, file_length, BLOCK_SIZE*block_counter),0);
            length = file_length;
        }
	block_counter++;
        SHA1(data, length, hash);
	/*Write hash to inode aka file*/
        log_syscall ("pwrite1",pwrite(fd_dest_file, hash, SHA_DIGEST_LENGTH, (block_counter - 1)*(SHA_DIGEST_LENGTH) + sizeof(off_t)), 0);
        storage_pos = find_block (BB_DATA->storage, NULL, hash); 
        if( storage_pos == -1 ) { /*Block not in storage.*/ 
            //write block
	    lseek (fd_storage, 0, SEEK_END);
	    log_syscall ("write", write(fd_storage, hash, SHA_DIGEST_LENGTH), 0 );
            log_syscall ("write", write(fd_storage, &length, sizeof(off_t)), 0 );
            log_syscall ("write", write(fd_storage, data, BLOCK_SIZE), 0);

            reference = 1;

            log_syscall("write", write(fd_storage, &reference, sizeof(int)), 0);
        }
        else{/*Block found in storage.*/
            log_syscall ("pread", pread(fd_storage, &reference, sizeof(int), storage_pos + HASH_SIZE +sizeof(off_t) + BLOCK_SIZE), 
                         0);
            reference++;
            log_syscall ("pwrite", pwrite(fd_storage, &reference, sizeof(int), storage_pos + HASH_SIZE+sizeof(off_t) + BLOCK_SIZE),
                         0);

        }

        //remaining file size
        file_length -= length;
    }while(file_length > 0);

    return;
}
/*Function that deletes blocks from storage that are no longer needed.*/
void garbage_collector (int fd_storage){
    int buffer_size = SHA_DIGEST_LENGTH + sizeof(off_t) + BLOCK_SIZE + sizeof(int);
    char buffer[buffer_size];
    int index;
    int reference;
    off_t storage_size;
	
    storage_size = lseek(fd_storage, 0, SEEK_END);
    /*Garbage collector.*/	
    for(index = buffer_size - sizeof(int); index < storage_size ; index += buffer_size){

        log_syscall("pread",pread(fd_storage, &reference, sizeof(int), index),0);
        if(reference == 0){
            log_syscall("pread",pread(fd_storage, buffer, buffer_size, storage_size - buffer_size),0);

            log_syscall("pwrite",pwrite(fd_storage, buffer, buffer_size, index - (BLOCK_SIZE + sizeof(off_t) + SHA_DIGEST_LENGTH)),0); 

            ftruncate(fd_storage, storage_size - buffer_size);   

	    storage_size = storage_size - buffer_size;   
   	    
            index -= buffer_size;
        }

    }
}


/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int bb_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int fd_storage;
    int bytes_written;
    int temp_file;
    off_t file_length;
	
    log_msg("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);


    //CALCULATE FILE'S SIZE

    //move cursor to the last file position.
    file_length = lseek (fi->fh, (size_t) 0, SEEK_END);

    //how many blocks the file has?
    if(file_length > 0 ) {
        lseek(fi->fh, (size_t)0, SEEK_SET); 
        log_syscall ("!!!!!!!read", read(fi->fh, &file_length, sizeof(off_t)), 0);
    }
 
    /*Get filedescriptors from the FILESYSTEM'S STORAGE AND TEMP FILE.*/
    temp_file = fileno (BB_DATA->temp);
    
    fd_storage = fileno(BB_DATA->storage); 
    if (file_length > 0){
    	create_temp_file (fd_storage, temp_file, fi->fh, file_length);
        file_length = lseek(temp_file, (size_t)0, SEEK_END);
    }
     
    //execute operation in temporary file and store back to storage file
    bytes_written = log_syscall("pwrite",pwrite(temp_file, buf, size, offset),0);
    /* Compute new size. */
    file_length = lseek(temp_file, (size_t)0, SEEK_END); 
    
    fragment_temp_file (fd_storage, fi->fh, temp_file, file_length); 	
    
    log_syscall ("fsync", fsync (fi->fh), 0);

    ftruncate (temp_file, 0);
    lseek (temp_file, 0, SEEK_SET);
    
    garbage_collector (fd_storage);
    //delete temp_file
    //close disk-fileno: no operation
    log_msg ("\n ###################### Everything Okay ################### \n");
    return bytes_written;
}
/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int bb_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_statfs(path=\"%s\", statv=0x%08x)\n",
	    path, statv);
    bb_fullpath(fpath, path);
    
    // get stats for underlying filesystem
    retstat = log_syscall("statvfs", statvfs(fpath, statv), 0);
    
    log_statvfs(statv);
    
    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
// this is a no-op in BBFS.  It just logs the call and returns success
int bb_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nbb_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
	
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int bb_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nbb_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    log_fi(fi);

    // We need to close the file.  Had we allocated any resources
    // (buffers etc) we'd need to free them here as well.
    return log_syscall("close", close(fi->fh), 0);
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int bb_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("\nbb_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    
    // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
    if (datasync)
	return log_syscall("fdatasync", fdatasync(fi->fh), 0);
    else
#endif	
	return log_syscall("fsync", fsync(fi->fh), 0);
}

#ifdef HAVE_SYS_XATTR_H
/** Note that my implementations of the various xattr functions use
    the 'l-' versions of the functions (eg bb_setxattr() calls
    lsetxattr() not setxattr(), etc).  This is because it appears any
    symbolic links are resolved before the actual call takes place, so
    I only need to use the system-provided calls that don't follow
    them */

/** Set extended attributes */
int bb_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
	    path, name, value, size, flags);
    bb_fullpath(fpath, path);

    return log_syscall("lsetxattr", lsetxattr(fpath, name, value, size, flags), 0);
}

/** Get extended attributes */
int bb_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	    path, name, value, size);
    bb_fullpath(fpath, path);

    retstat = log_syscall("lgetxattr", lgetxattr(fpath, name, value, size), 0);
    if (retstat >= 0)
	log_msg("    value = \"%s\"\n", value);
    
    return retstat;
}

/** List extended attributes */
int bb_listxattr(const char *path, char *list, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char *ptr;
    
    log_msg("\nbb_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
	    path, list, size
	    );
    bb_fullpath(fpath, path);

    retstat = log_syscall("llistxattr", llistxattr(fpath, list, size), 0);
    if (retstat >= 0) {
	log_msg("    returned attributes (length %d):\n", retstat);
	if (list != NULL)
	    for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
		log_msg("    \"%s\"\n", ptr);
	else
	    log_msg("    (null)\n");
    }
    
    return retstat;
}

/** Remove extended attributes */
int bb_removexattr(const char *path, const char *name)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_removexattr(path=\"%s\", name=\"%s\")\n",
	    path, name);
    bb_fullpath(fpath, path);

    return log_syscall("lremovexattr", lremovexattr(fpath, name), 0);
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int bb_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    bb_fullpath(fpath, path);

    // since opendir returns a pointer, takes some custom handling of
    // return status.
    dp = opendir(fpath);
    log_msg("    opendir returned 0x%p\n", dp);
    if (dp == NULL)
	retstat = log_error("bb_opendir opendir");
    
    fi->fh = (intptr_t) dp;
    
    log_fi(fi);
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int bb_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp;
    struct dirent *de;
    
    log_msg("\nbb_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
	    path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (DIR *) (uintptr_t) fi->fh;

    // Every directory contains at least two entries: . and ..  If my
    // first call to the system readdir() returns NULL I've got an
    // error; near as I can tell, that's the only condition under
    // which I can get an error from readdir()
    de = readdir(dp);
    log_msg("    readdir returned 0x%p\n", de);
    if (de == 0) {
	retstat = log_error("bb_readdir readdir");
	return retstat;
    }

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    do {
	/*Storage and temp should not be visual for user.*/
	if (strcmp (de->d_name,".storage") == 0 || strcmp (de->d_name, ".temp") == 0){
	    continue;
        }
	if (filler(buf, de->d_name, NULL, 0) != 0) {
	    log_msg("    ERROR bb_readdir filler:  buffer full");
	    return -ENOMEM;
	}
    } while ((de = readdir(dp)) != NULL);
    
    log_fi(fi);
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int bb_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x)\n",
	    path, fi);
    log_fi(fi);
    
    closedir((DIR *) (uintptr_t) fi->fh);
    
    return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? >>> I need to implement this...
int bb_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nbb_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    
    return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *bb_init(struct fuse_conn_info *conn)
{
    char path[PATH_MAX] = {'\0'};
    struct bb_state *bb_data = BB_DATA;

    log_msg("\nbb_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    strcpy (path, bb_data->rootdir);
    strcat (path, "/.storage");
    log_msg ("NEW PATH IS %s.\n", path);
    /* Open or create the storage File.
     * Storage file will contain file's block.
     * Storage of our fuse is implemented in hidden file storage. 
     */
    bb_data->storage = fopen (path, "r+");    
    if (bb_data->storage == NULL) { 
        /*Storage has not be created.*/
	bb_data->storage = fopen (path, "w+");
    }
    strcpy (path, bb_data->rootdir);
    strcat (path, "/.temp");

    bb_data->temp = fopen (path, "r+");      
    bb_data->temp = fopen (path, "r+");    
    if (bb_data->temp == NULL) { 
        /*Temp has not be created.*/
	bb_data->temp = fopen (path, "w+");
    }

    return bb_data;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void bb_destroy(void *userdata)
{
    struct bb_state* bb_data = BB_DATA;
    char path[PATH_MAX] = {'\0'};
    /*Close FILE* of storage. */	
    fclose (bb_data->storage);
    
    strcpy (path, bb_data->rootdir);
    strcat (path, "/.temp");
    
    log_syscall ("remove", remove(path), 0);
    fclose (bb_data->temp);
    log_msg("\nbb_destroy(userdata=0x%08x)\n", userdata);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int bb_access(const char *path, int mask)
{
    int retstat = 0;
    char fpath[PATH_MAX];
   
    log_msg("\nbb_access(path=\"%s\", mask=0%o)\n",
	    path, mask);
    bb_fullpath(fpath, path);
    
    retstat = access(fpath, mask);
    
    if (retstat < 0)
	retstat = log_error("bb_access access");
    
    return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
// Not implemented.  I had a version that used creat() to create and
// open the file, which it turned out opened the file write-only.

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int bb_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int temp_file, fd_storage, ret;
    off_t file_length;

    /*Check bb_truncate.*/
    file_length = lseek (fi->fh, (size_t) 0, SEEK_END);
    
    log_msg ("\nbb_truncate-> %d bytes\n", file_length);
    //how many blocks the file has?
    if(file_length > 0 ) {
        lseek(fi->fh, (size_t)0, SEEK_SET); 
        log_syscall ("!!!!!!!read", read(fi->fh, &file_length, sizeof(off_t)), 0);
    }
 
    temp_file = fileno (BB_DATA->temp);
    
    fd_storage = fileno(BB_DATA->storage); 
    if (file_length > 0){
    	create_temp_file (fd_storage, temp_file, fi->fh, file_length);
        file_length = lseek(temp_file, (size_t)0, SEEK_END);
    }
     
    log_msg ("\nFile length of temp_file is %d.\n", file_length);	
    //execute operation in temporary file and store back to storage file
    ret = log_syscall("ftruncate", ftruncate(temp_file, offset), 0);
    /* Compute new size. */
    
    file_length = lseek(temp_file, 0, SEEK_END);
    log_msg ("Length is %d.\n", file_length);
    fragment_temp_file (fd_storage, fi->fh, temp_file, file_length); 	
    

    log_syscall ("fsync", fsync (fi->fh), 0);

    ftruncate (temp_file, 0);
    lseek (temp_file, 0, SEEK_SET);
    
    garbage_collector (fd_storage);

    return ret;

}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int bb_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    log_msg("\nbb_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
	    path, statbuf, fi);
    log_fi(fi);
    	
    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/"))
	return bb_getattr(path, statbuf);
    
    bb_fullpath(fpath, path);

    retstat = fstat(fi->fh, statbuf);
    if (retstat < 0)
	retstat = log_error("bb_fgetattr fstat");
    
    lseek(fi->fh, 0, SEEK_SET);
    /*Update st_size with the proper one.*/
    log_syscall ("read", read (fi->fh, &statbuf->st_size, sizeof(off_t)), 0);

    log_stat(statbuf);
    
    return retstat;
}

struct fuse_operations bb_oper = {
  .getattr = bb_getattr,
  .readlink = bb_readlink,
  // no .getdir -- that's deprecated
  .getdir = NULL,
  .mknod = bb_mknod,
  .mkdir = bb_mkdir,
  .unlink = bb_unlink,
  .rmdir = bb_rmdir,
  .symlink = bb_symlink,
  .rename = bb_rename,
  .link = bb_link,
  .chmod = bb_chmod,
  .chown = bb_chown,
  .truncate = bb_truncate,
  .utime = bb_utime,
  .open = bb_open,
  .read = bb_read,
  .write = bb_write,
  /** Just a placeholder, don't set */ // huh???
  .statfs = bb_statfs,
  .flush = bb_flush,
  .release = bb_release,
  .fsync = bb_fsync,
  
#ifdef HAVE_SYS_XATTR_H
  .setxattr = bb_setxattr,
  .getxattr = bb_getxattr,
  .listxattr = bb_listxattr,
  .removexattr = bb_removexattr,
#endif
  
  .opendir = bb_opendir,
  .readdir = bb_readdir,
  .releasedir = bb_releasedir,
  .fsyncdir = bb_fsyncdir,
  .init = bb_init,
  .destroy = bb_destroy,
  .access = bb_access,
  .ftruncate = bb_ftruncate,
  .fgetattr = bb_fgetattr
};

void bb_usage()
{
    fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct bb_state *bb_data;

    // bbfs doesn't do any access checking on its own (the comment
    // blocks in fuse.h mention some of the functions that need
    // accesses checked -- but note there are other functions, like
    // chown(), that also need checking!).  Since running bbfs as root
    // will therefore open Metrodome-sized holes in the system
    // security, we'll check if root is trying to mount the filesystem
    // and refuse if it is.  The somewhat smaller hole of an ordinary
    // user doing it with the allow_other flag is still there because
    // I don't want to parse the options string.
    if ((getuid() == 0) || (geteuid() == 0)) {
    	fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
    	return 1;
    }

    // See which version of fuse we're running
    fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
    
    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	bb_usage();

    bb_data = malloc(sizeof(struct bb_state));
    if (bb_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the rootdir out of the argument list and save it in my
    // internal data
    bb_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
       
    bb_data->logfile = log_open();
    bb_data->storage = NULL;
    bb_data->temp = NULL;    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &bb_oper, bb_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
