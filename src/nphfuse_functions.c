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
#include <unistd.h>
#include <dirent.h>

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

//Assuming the nodemd is stored at objectID 999 in npheap.
extern struct nphfuse_state *nphfuse_data;
static int maxDirs;

int nphfuse_getattr(const char *path, struct stat *stbuf)
{
    void *curr;
    if (path == "/")
    {
        curr= npheap_alloc(nphfuse_data->devfd, 999, 8192);
        return 0;
    }
    else
    {   
        char *pathToken;     
        pathToken = strtok(path,"/");        
        int tempOffset=999;
        curr= npheap_alloc(nphfuse_data->devfd, tempOffset, 8192);
        struct dirent dirArray[maxDirs] ;
        int i;
        for(i=0;i<maxDirs;i++)
        {
             dirArray[i].d_ino = 0;
        }
        while(pathToken)
        {
            memcpy(curr + sizeof(struct nphfs_file_metadata)+sizeof(struct dirent), &dirArray, sizeof(struct dirent)*maxDirs);
            tempOffset=-1;
            for(i=0;i<maxDirs;i++)
            {
                 if(strcmp(pathToken, dirArray[i].d_name)==0)
                 {
                     tempOffset = dirArray[i].d_ino;
                     break;
                 }
                 
            }
            if(tempOffset!=-1)
            {   
                curr= npheap_alloc(nphfuse_data->devfd, tempOffset, 8192);
                pathToken = strtok(NULL,"/");
            }
        }
        if(tempOffset!=-1)
        {   
            memcpy(stbuf, &(((struct nphfs_file_metadata*)curr)->filestat), sizeof(struct stat));
            return 0;
        }
    }

    return -ENOENT;
    
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
    return -1;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
int nphfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
    return -ENOENT;
}

/** Create a directory */
int nphfuse_mkdir(const char *path, mode_t mode)
{
    void *curr;
//    void *prev;
        char *pathToken;     
        pathToken = strtok(path,"/");        
        int tempOffset=999;
        curr= npheap_alloc(nphfuse_data->devfd, tempOffset, 8192);
        struct dirent dirArray[maxDirs] ;
        int i;
        for(i=0;i<maxDirs;i++)
        {
             dirArray[i].d_ino = 0;
        }
        //Finding the dir to create the new dir
        while(pathToken)
        {
            memcpy(curr + sizeof(struct nphfs_file_metadata)+sizeof(struct dirent), &dirArray, sizeof(struct dirent)*maxDirs);
            tempOffset=-1;
            for(i=0;i<maxDirs;i++)
            {
                 if(strcmp(pathToken, dirArray[i].d_name)==0)
                 {
                     tempOffset = dirArray[i].d_ino;
                     break;
                 }                 
            }
            if(tempOffset!=-1)
            {   
                curr= npheap_alloc(nphfuse_data->devfd, tempOffset, 8192);
                pathToken = strtok(NULL,"/");
            }
            else{
                break;
            }
        }
        //Create node
        for(i=0;i<maxDirs;i++)
        {
             if(dirArray[i].d_ino==0)
             {
                 break;
             }                 
        }
        int offset =getNextFreeOffset();
        npheap_lock(nphfuse_data->devfd, offset);  
        //adding dirent
        dirArray[i].d_ino=offset;
        memcpy(dirent[i].d_name,pathToken,strlen(pathToken));
        //creating new object
        createObject(offset, pathToken, mode);
        npheap_unlock(nphfuse_data->devfd, offset);  

    return -ENOENT;
}

void createObject(int offset, char * filename, mode_t mode)
{
    npheap_lock(nphfuse_data->devfd, offset);    
    void *ptr = npheap_alloc(nphfuse_data->devfd, offset, 8192);
    memset(ptr, 0, 8192);
    struct nphfs_file_metadata nodemd;
    nodemd.filestat.st_ino = offset;
    nodemd.filestat.st_dev =  nphfuse_data->devfd;
    nodemd.filestat.st_mode = mode;
    nodemd.filestat.st_nlink = 1;
    nodemd.filestat.st_uid = getuid();
    nodemd.filestat.st_gid = getgid();
    nodemd.filestat.st_rdev = 0;
    nodemd.filestat.st_atime =time(NULL);
    nodemd.filestat.st_mtime =time(NULL);
    nodemd.filestat.st_ctime =time(NULL);
    nodemd.filestat.st_blksize = 8192; 
    nodemd.filestat.st_blocks = 1;
    nodemd.filename=filename;
    log_msg("\n Creating metadata for %s", filename);
    memcpy(ptr,&nodemd,sizeof(struct nphfs_file_metadata));
    log_msg("\n Created metadata for %s", filename);
    log_msg("\n Creating dir-entries array for %s", filename);
    maxDirs=(8192 -sizeof(struct nphfs_file_metadata)-sizeof(struct dirent))/sizeof(struct dirent);
    struct dirent dirs[maxDirs] ;
    int i;
    for(i=0;i<maxDirs;i++)
    {
         dirs[i].d_ino = 0;
    }
    memcpy(ptr + sizeof(struct nphfs_file_metadata)+sizeof(struct dirent), dirs, sizeof(struct dirent)*maxDirs);
    log_msg("\n Created dir-entries array for %s", filename);
    npheap_unlock(nphfuse_data->devfd, offset);
}

int getNextFreeOffset()
{
    int offset =1;
    while(npheap_getsize(nphfuse_data->devfd, offset)!=0)
    {
        offset++;
    }
    return offset;
}



/** Remove a file */
int nphfuse_unlink(const char *path)
{
    return -1;
}

/** Remove a directory */
int nphfuse_rmdir(const char *path)
{
    return -1;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int nphfuse_symlink(const char *path, const char *link)
{
    return -1;
}

/** Rename a file */
// both path and newpath are fs-relative
int nphfuse_rename(const char *path, const char *newpath)
{
    return -1;
}

/** Create a hard link to a file */
int nphfuse_link(const char *path, const char *newpath)
{
    return -1;
}

/** Change the permission bits of a file */
int nphfuse_chmod(const char *path, mode_t mode)
{
        return -ENOENT;
}

/** Change the owner and group of a file */
int nphfuse_chown(const char *path, uid_t uid, gid_t gid)
{
        return -ENOENT;
}

/** Change the size of a file */
int nphfuse_truncate(const char *path, off_t newsize)
{
        return -ENOENT;
}

/** Change the access and/or modification times of a file */
int nphfuse_utime(const char *path, struct utimbuf *ubuf)
{
        return -ENOENT;
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
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    return -ENOENT;

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
    return -ENOENT;
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
    return -ENOENT;
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
    return -1;
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
    return 0;
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
    return -1;
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int nphfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    return -61;
}

/** Get extended attributes */
int nphfuse_getxattr(const char *path, const char *name, char *value, size_t size)
{
    return -61;
}

/** List extended attributes */
int nphfuse_listxattr(const char *path, char *list, size_t size)
{
    return -61;
}

/** Remove extended attributes */
int nphfuse_removexattr(const char *path, const char *name)
{
    return -61;
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
    return -ENOENT;
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
    return -ENOENT;
}

/** Release directory
 */
int nphfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
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
    return 0;
}

int nphfuse_access(const char *path, int mask)
{
    return 0;
//    return -1;
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
    return -1;
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
        return -ENOENT;
}

void *nphfuse_init(struct fuse_conn_info *conn)
{
    log_msg("\nnphfuse_init()\n");
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    int size = npheap_getsize(nphfuse_data->devfd, 999);
    if(!size){
    npheap_lock(nphfuse_data->devfd, 999);    
    void *ptr = npheap_alloc(nphfuse_data->devfd, 999, 8192);
    memset(ptr, 0, 8192);
    struct nphfs_file_metadata nodemd;
    nodemd.filestat.st_ino = 999;
    nodemd.filestat.st_dev =  nphfuse_data->devfd;
    nodemd.filestat.st_mode = S_IFDIR | 0755;
    nodemd.filestat.st_nlink = 2;
    nodemd.filestat.st_uid = getuid();
    nodemd.filestat.st_gid = getgid();
    nodemd.filestat.st_rdev = 0;
    nodemd.filestat.st_atime =time(NULL);
    nodemd.filestat.st_mtime =time(NULL);
    nodemd.filestat.st_ctime =time(NULL);
    nodemd.filestat.st_blksize = 8192; 
    nodemd.filestat.st_blocks = 1;
    nodemd.filename="/";
    log_msg("\n Creating nodemd metadata");
    memcpy(ptr,&nodemd,sizeof(struct nphfs_file_metadata));
    log_msg("\n nodemd metadata written");

    maxDirs=(8192 -sizeof(struct nphfs_file_metadata)-sizeof(struct dirent))/sizeof(struct dirent);
    struct dirent dirs[maxDirs] ;
    int i;
    for(i=0;i<maxDirs;i++)
    {
         dirs[i].d_ino = 0;
    }
    memcpy(ptr + sizeof(struct nphfs_file_metadata)+sizeof(struct dirent), dirs, sizeof(struct dirent)*maxDirs);

    npheap_unlock(nphfuse_data->devfd, 999);
    }
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
