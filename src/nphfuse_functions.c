/*
  NPHeap File System
  Copyright (C) 2016 Hung-Wei Tseng, Ph.D. <hungwei_tseng@ncsu.edu>

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

*/

#include "nphfuse.h"
#include <npheap.h>
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
#include "log.h"

int set;

typedef struct listnode{
    char *filename;
    char *dirname;
    __u64 offset;
    struct listnode *next;
}llist;

void get_fullpath(char fp[PATH_MAX],char *path)
{
    log_msg("Into getfullpath \n");

    if(set == 0){
        system("mkdir $HOME/npheap");
        set = 1;
    }
    memset(fp,0,PATH_MAX);

    char *root_path="/";
    const char* s = getenv("HOME");
    strcpy(fp, s);
    strcat(fp, "/npheap");

    printf("%s and fp is %s \n",s , fp);

    if(strcmp(path,root_path)==0)
    {
        strcpy(path,"/");
        strncat(fp, path, PATH_MAX); 
    }
    else
    {
        strncat(fp, path, PATH_MAX);
    }

    printf("Fullpath is %s \n", fp);
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
int nphfuse_getattr(const char *path, struct stat *stbuf)
{
    log_msg("Into getattr function \n");
    
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);

    int retval;
    log_msg("Fullpath is %s \n", fullpath);

    retval = lstat(fullpath, stbuf);

    if(retval){
        printf("[%s] doesn't exist.!!!\n",path);
        return -ENOENT;
    }
    return retval;
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
// less than the size passed to nphfuse_readlink()
// nphfuse_readlink() code by Bernardo F Costa (thanks!)
int nphfuse_readlink(const char *path, char *link, size_t size)
{
    log_msg("Into readlink \n");

    int retval;
    char fullpath[PATH_MAX];

    get_fullpath(fullpath,path);

    retval = readlink(fullpath, link, size - 1);
    if (retval >= 0) {
        link[retval] = '\0';
        retval = 0;
    }
    
    return retval;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
int nphfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
    log_msg("Into mknod function");

    int retval;

    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);

    if (S_ISREG(mode)) {
        retval = open(fullpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (retval >= 0)
            retval = close(retval);
    } 
    else if (S_ISFIFO(mode))
        retval = mkfifo(fullpath, mode);
    else
        retval =  mknod(fullpath, mode, dev);
    
    return retval;
}

/** Create a directory */
int nphfuse_mkdir(const char *path, mode_t mode)
{
    log_msg("Into mkdir function\n");

    int retval;
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);

    retval=mkdir(fullpath, mode);

    return retval;
}

/** Remove a file */
int nphfuse_unlink(const char *path)
{
    log_msg("Into unlink function\n");

    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    int retval = unlink(fullpath);

    return retval;
}

/** Remove a directory */
int nphfuse_rmdir(const char *path)
{
    log_msg("Into rmdir function\n");

    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    int retval = -rmdir(fullpath);
    return retval;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int nphfuse_symlink(const char *path, const char *link)
{
    log_msg("Into symlink function\n");
    
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    int retval = symlink(path, fullpath);
    return retval;
}

/** Rename a file */
// both path and newpath are fs-relative
int nphfuse_rename(const char *path, const char *newpath)
{
    log_msg("Into rename function\n");

    char fullpath[PATH_MAX];
    char fullnewpath[PATH_MAX];
    get_fullpath(fullpath,path);
    get_fullpath(fullnewpath,newpath);
    int retval = rename(fullpath, fullnewpath);
    return retval;
}

/** Create a hard link to a file */
int nphfuse_link(const char *path, const char *newpath)
{
    log_msg("Into link function\n");

    char fullpath[PATH_MAX];
    char fullnewpath[PATH_MAX];
    get_fullpath(fullpath,path);
    get_fullpath(fullnewpath,newpath);
    int retval = link(fullpath, fullnewpath);
    return retval;
}

/** Change the permission bits of a file */
int nphfuse_chmod(const char *path, mode_t mode)
{
    log_msg("Into chmod function\n");

    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);

    int retval;

	retval = chmod(fullpath, mode);
	if (retval == -1)
		return -errno;

	return 0;
}

/** Change the owner and group of a file */
int nphfuse_chown(const char *path, uid_t uid, gid_t gid)
{
    log_msg("Into chown function\n");

    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    return lchown(fullpath, uid, gid);
}

/** Change the size of a file */
int nphfuse_truncate(const char *path, off_t newsize)
{
    return -ENOENT;
}

/** Change the access and/or modification times of a file */
int nphfuse_utime(const char *path, struct utimbuf *ubuf)
{
    log_msg("Into utime function\n");

    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);

    int retval=utime(fullpath, ubuf);

    printf("Actual time %d\n", ubuf->actime);
    return retval;
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
int nphfuse_open(const char *path, struct fuse_file_info *fi)
{
    log_msg("Into open function\n");

    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
	int retval;

	retval = open(fullpath, fi->flags);
	if (retval == -1)
		return -errno;

    fi->fh=retval;
    
    return 0;
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
int nphfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    log_msg("Into read function\n");
    
	int retval;
	retval = pread(fi->fh, buf, size, offset);
	if (retval == -1)
        retval = -errno;

	return retval;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 */
int nphfuse_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    log_msg("Into write function\n");
    
    int retval;
    retval = pwrite(fi->fh, buf, size, offset);
    if (retval == -1)
        retval = -errno;
    
        // close(fd);
    return retval;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int nphfuse_statfs(const char *path, struct statvfs *statv)
{
    log_msg("Into statfs function\n");
    int retval = 0;
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    retval = statvfs(fullpath, statv);

    return retval;
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

// this is a no-op in NPHFS.  It just logs the call and returns success
int nphfuse_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg("Into flush function\n");
    log_msg("\nnphfuse_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
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
int nphfuse_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("Into release function\n");
    int retval = close(fi->fh);
    return retval;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int nphfuse_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("Into fsync function\n");
    int retval = fsync(fi->fh);
    return retval;
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int nphfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    log_msg("Into setxattr function\n");
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    int retval = lsetxattr(fullpath, name, value, size, flags);
    return retval;
}

/** Get extended attributes */
int nphfuse_getxattr(const char *path, const char *name, char *value, size_t size)
{
    log_msg("Into getxattr function\n");
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    int retval = lgetxattr(fullpath, name, value, size);
    return retval;
}

/** List extended attributes */
int nphfuse_listxattr(const char *path, char *list, size_t size)
{
    log_msg("Into listxattr function\n");
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    int retval = llistxattr(fullpath, list, size);
    return retval;
}

/** Remove extended attributes */
int nphfuse_removexattr(const char *path, const char *name)
{
    log_msg("Into removexattr function\n");
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);
    int retval = lremovexattr(fullpath, name);
    return retval;
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this directory
 *
 * Introduced in version 2.3
 */
int nphfuse_opendir(const char *path, struct fuse_file_info *fi)
{
    log_msg("Into opendir function\n");
    
    DIR *dirp;
    int retval = 0;
    char fullpath[PATH_MAX];
    
    get_fullpath(fullpath,path);
    
    dirp = opendir(fullpath);
    
    if (dirp==NULL)
    {
        printf("Error thrown\n");
        return retval;
    }
    
    fi->fh = (intptr_t) dirp;
    return retval;
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

int nphfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
           struct fuse_file_info *fi)
{
    log_msg("Into readdir function\n");

    int retval = 0;
    DIR *dirp;

    struct dirent *dent;
   
    dirp = (DIR *) (uintptr_t) fi->fh;
    dent = readdir(dirp);

    if (dent == 0) {
	    return retval;
    }

    do {
	    if (filler(buf, dent->d_name, NULL, 0) != 0) {
		    printf("Error thrown \n");
	        return -ENOMEM;
	    }
    } while ((dent = readdir(dirp)) != NULL);
    
    return retval;
}

/** Release directory */
int nphfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
    log_msg("Into releasedir function\n");

    closedir((DIR *) (uintptr_t) fi->fh);
    return 0;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? 
int nphfuse_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("Into fsyncdir function\n");

    return -1;
}

int nphfuse_access(const char *path, int mask)
{
    log_msg("Into access function \n");
    int retval;
    
    char fullpath[PATH_MAX];
    get_fullpath(fullpath,path);

    printf("Fullpath in access is %s\n", fullpath );
    retval = access(fullpath, mask);
  
    return retval;
}

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
int nphfuse_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    log_msg("Into ftruncate function \n");
    int retval = 0;
    retval = ftruncate(fi->fh, offset);
    return retval;
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
 */
int nphfuse_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    log_msg("Into fgetattr function \n");
    int retval = nphfuse_getattr(path, statbuf);
    
  	return retval;
}

void *nphfuse_init(struct fuse_conn_info *conn)
{
    log_msg("\nnphfuse_init()\n");
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    
    log_msg("Into INIT function \n");

    set = 0;

    return NPHFS_DATA;
}


/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void nphfuse_destroy(void *userdata)
{
    log_msg("\nnphfuse_destroy(userdata=0x%08x)\n", userdata);
}