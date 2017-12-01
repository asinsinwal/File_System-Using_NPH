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
#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define BLOCK_SIZE 8192
extern struct nphfuse_state *nphfuse_data;

int npheap_fd = 1;
uint64_t inode_off = 2;
uint64_t data_off = 504;

//Getting the root directory
static npheap_store *getRootDirectory(void){
    npheap_store *temp1 = NULL;

    log_msg("Get Root Directory function called.!");
    temp1 = (npheap_store *)npheap_alloc(npheap_fd, 2, BLOCK_SIZE);

    if(temp1 == NULL){
        log_msg("\tRoot directory not found.\n");
        return NULL;
    }
    log_msg("\tRoot directory found.\n");
    return &(temp1[0]);
}

static npheap_store *retrieve_inode(const char *path){
    char dir[236];
    char filename[128];
    uint64_t offset = 2;
    npheap_store *start = NULL;

    //Get from directory
    int extract = extract_directory_file(dir, filename, path);

    //If couldn't extract the file and directory
    if(extract == 1){
        return NULL;
    }

    //Iterate through the inodes
    for(offset = 2; offset < 502; offset++){
        start = (npheap_store *)npheap_alloc(npheap_fd, offset, BLOCK_SIZE);
        log_msg("Into inode loop for offset %d\n", offset);
        if(start == 0){
            log_msg("Couldn't allocate the memory for offset, something went wrong\n");
            return NULL;
        }

        for(int i = 0; i < 16; i++){
            if((strcmp(start[i].dirname, dir)==0) && (strcmp(start[i].filename, filename)==0)){
                return &start[i];
            }
        }
    }
    //Didn't find the inode for given path
    return NULL;
}

static npheap_store *get_free_inode(uint64_t *ind_val){
    uint64_t inode_index = 0;
    int flag = 0;
    npheap_store *temp = NULL;
    uint64_t offset = 2;
    uint64_t index = 0;
    log_msg("Into get free inode function.\n");

    for(offset = 2; offset < 502; offset++){
        temp= (npheap_store *)npheap_alloc(npheap_fd, offset, BLOCK_SIZE);
        log_msg("INODE main loop.\n");
        //If returned value is null
        if(!temp){
            log_msg("NPheap alloc failed for offset : %d\n",offset);
        }
        // internal block check
        for (index = 0; index < 16; index++){
            log_msg("Search %d in offset %d for free inode\n", index, offset);
            if (temp[index].dirname[0] == '\0' &&
                temp[index].filename[0] == '\0'){
                log_msg("Free inode found at %d in offset %d\n", index, offset);
                flag = 1;
                inode_index = index;
                break;
            }
        }
        //If value obtained in inner loop
        if(flag==1){
            break;
        }
    }
    //Return inode
    if(flag==1){
        *ind_val = inode_index;
        return &temp[inode_index];
    }

    log_msg("Couldn't find the free space.\n");
    return NULL;
}

int extract_directory_file(char *dir, char *filename, const char *path) {
    // char *slash = path, *next;
    // while ((next = strpbrk(slash + 1, "\\/"))) slash = next;
    // if (path != slash) slash++;
    // *dir = strndup(path, slash - path);
    // *filename = strdup(slash);

    char *copy = NULL;
    char *next = NULL;
    char *prnt = NULL;

    if(!path){
        return 1;
    }
    memset(dir, 0, 236);
    memset(filename, 0, 128);

    if(!strcmp(path, "/")){
        strcpy(dir, "/");
        strcpy(filename, "/");
        log_msg("Sent: dir and filename as \"/\".\n");
        return 0;
    }

    copy = strdup(path);

    next = strtok(copy, "/");
    if(!next){
        log_msg("Splitting into tokens\n");
        free(copy);
        return 1;
    }

    prnt = next;

    while((next = strtok(NULL, "/")) != NULL){
        strncat(dir, "/", 236);
        strncat(dir, prnt, 128);
        prnt = next;
    }

    if(dir[0] == '\0'){
        strcpy(dir, "/");
    }
    strncpy(filename, prnt, 128);

    log_msg("Extracting: Directory is %s and Filename is %s for path\n", dir, filename, path);

    free(copy);
    return 0;
}

int checkAccess(npheap_store *inode){
    //Temperory flag
    int flag = 0;
    if(getuid() == 0 || getgid() == 0){
        flag = 1;
    }
    if(flag != 1){
        if(inode->mystat.st_uid == getuid() || inode->mystat.st_gid == getgid()){
            flag = 1;
        }
    }
    //else return correct value
    return flag;
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
int nphfuse_getattr(const char *path, struct stat *stbuf){
    // extract_directory_file(dir,filename,path);
    npheap_store *inode = NULL;
    
    if(strcmp(path,"/")==0){
        inode = getRootDirectory();
        if(inode==NULL)
        {
            log_msg("Root directory not found in getattr.\n");
            return -ENOENT;
        }
        else
        {
            log_msg("Assigning root stbuf in getattr\n");
            memcpy(stbuf, &inode->mystat, sizeof(struct stat));
            return 0;
        }
    }

    inode = retrieve_inode(path);

    if(inode == NULL){
        return -ENOENT;
    }

    // else return the proper value
    log_msg("Assigning normal stbuf in getattr\n");
    memcpy(stbuf, &inode->mystat, sizeof(struct stat));
    return 0;
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
int nphfuse_mknod(const char *path, mode_t mode, dev_t dev){
    struct timeval currTime;
    npheap_store *inode = NULL;
    char dir[236];
    char filename[128];
    char *data_block = NULL;
    uint64_t findex = -1;
    log_msg("Into mkdir functionality.\n");
    inode = get_free_inode(&findex);

    //If empty directory not found
    if(inode == NULL){
        log_msg("Empty Directory not found. \n");
        return -ENOENT; 
    }

    //Get directory and filename
    int extract = extract_directory_file(dir, filename, path);
    if(extract == 1){
        log_msg("Extraction failed. \n");
        return -EINVAL;
    }

    log_msg("Directory %s and Filename is %s \n", dir, filename);

    memset(inode, 0, sizeof(npheap_store));
    strcpy(inode->dirname, dir);
    strcpy(inode->filename, filename);

    // Set mystat
    inode->mystat.st_ino = inode_off;
    inode_off++;
    inode->mystat.st_mode = mode;
    inode->mystat.st_gid = getgid();
    inode->mystat.st_uid = getuid();
    inode->mystat.st_dev = dev;
    inode->mystat.st_nlink = 1;

    gettimeofday(&currTime, NULL);
    inode->mystat.st_atime = currTime.tv_sec;
    inode->mystat.st_mtime = currTime.tv_sec;
    inode->mystat.st_ctime = currTime.tv_sec;


    //Set the offset for data object
    if(npheap_getsize(npheap_fd, data_off) != 0){
        log_msg("Cannot allocate memory for data. Reverting...");
        inode_off--;
        return -ENOSPC;
    }

    data_block = (char *)npheap_alloc(npheap_fd, data_off, BLOCK_SIZE);

    //Check if allocated
    if(data_block == NULL){
        log_msg("Data block, couldn't be allocated\n");
        memset(inode, 0, sizeof(npheap_store));
        inode_off--;
        return -ENOMEM;
    }

    //Everything worked fine
    memset(data_block, 0, BLOCK_SIZE);
    inode->offset = data_off;
    data_off++;

    log_msg("mknod ran successfully in NPHeap");
    return 0;
}


/** Create a directory */
int nphfuse_mkdir(const char *path, mode_t mode){
    // char *filename, *dir;
    // extract_directory_file(&dir,&filename,path);
    struct timeval currTime;
    npheap_store *inode = NULL;
    char dir[236];
    char filename[128];
    uint64_t findex = -1;
    log_msg("Into mkdir functionality.\n");
    inode = get_free_inode(&findex);

    //If empty directory not found
    if(inode == NULL){
        log_msg("Empty Directory not found. \n");
        return -ENOENT; 
    }

    //Get directory and filename
    int extract = extract_directory_file(dir, filename, path);
    if(extract == 1){
        log_msg("Extraction failed. \n");
        return -EINVAL;
    }

    log_msg("Directory %s and Filename is %s \n", dir, filename);

    memset(inode, 0, sizeof(npheap_store));
    strcpy(inode->dirname, dir);
    strcpy(inode->filename, filename);

    inode->mystat.st_ino = inode_off;
    inode_off++;
    inode->mystat.st_mode = S_IFDIR | mode;
    inode->mystat.st_gid = getgid();
    inode->mystat.st_uid = getuid();
    inode->mystat.st_size = BLOCK_SIZE;
    inode->mystat.st_nlink = 2;

    gettimeofday(&currTime, NULL);
    inode->mystat.st_atime = currTime.tv_sec;
    inode->mystat.st_mtime = currTime.tv_sec;
    inode->mystat.st_ctime = currTime.tv_sec;

    log_msg("mkdir executed successfully.! %d st_ino\n", inode->mystat.st_ino);

    return 0;
}

/** Remove a file */
int nphfuse_unlink(const char *path)
{
    return -1;
}

/** Remove a directory */
int nphfuse_rmdir(const char *path)
{
    char *filename, *dir;
    extract_directory_file(&dir,&filename,path);
    npheap_store *temp;

    uint64_t       offset = 0;
    uint64_t       index = 0;
    uint64_t       found = -1;
    for(offset = 2; offset < 1000; offset++){
        temp= (npheap_store *)npheap_alloc(npheap_fd, offset, BLOCK_SIZE);
        if(temp==NULL)
        {
            log_msg("NPheap alloc failed for offset : %d",offset);
        }
        for (index = 0; index <16; index++)
        {
            if ((!strcmp (temp[index].dirname, dir)) &&
                (!strcmp (temp[index].filename, filename)))
            {
                /* Entry found in inode block */
                found=index;
                break;
            }
        }
        if(found!=-1)
        {
            break;
        }
    }
    if(found != -1)
    {
        log_msg("Directory found. \n");
        temp[found].filename[0] = '\0';
        temp[found].dirname[0] = '\0';
        memset(&temp[found].mystat, 0, sizeof(struct stat));
        log_msg("Directory deleted \n");
        return 0;
    }
    else
    {
        log_msg("Directory not found. \n");
        return -ENOENT;    
    }
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
    struct timeval currTime;
    npheap_store *temp = NULL;

    //Check for root directory
    if(strcmp(path,"/")==0){
        temp = getRootDirectory();
        if(temp==NULL)
        {
            log_msg("Root directory not found in open.\n");
            return -ENOENT;
        }
        else
        {
            int flag = checkAccess(temp);

            //If cannot access
            if(flag == 0){
                log_msg("Root access denied.\n");
                return -EACCES;
            }
            // Worked fine
            log_msg("Access granted to root.\n");
            return 0;
        }
    }

    temp = retrieve_inode(path);
    if(temp == NULL){
        return -ENOENT;
    }

    int flag1 = checkAccess(temp);

    //if cannot access
    if(flag1 == 0){
        log_msg("Access denied.\n");
        return -EACCES;
    }
    //Everything worked fine
    fi->fh = temp->mystat.st_ino;
    gettimeofday(&currTime, NULL);
    temp->mystat.st_atime = currTime.tv_sec;
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
int nphfuse_opendir(const char *path, struct fuse_file_info *fi){
    // char *filename, *dir;
    // extract_directory_file(&dir,&filename,path);
    npheap_store *inode = NULL;
    log_msg("Entry into OPENDIR.\n");
    if(strcmp (path,"/")==0){
        inode = getRootDirectory();
        if(inode==NULL)
        {
            log_msg("Root directory not found in opendir.\n");
            return -ENOENT;
        }
        else
        {
            //Check if accessibilty can be given
            int flag = checkAccess(inode);
            //Deny the access
            log_msg("Root access %d (if 1 then yes)",flag);
            if(flag == 0){
                return -EACCES;
            }
            return 0;
        }
    }

    inode = retrieve_inode(path);

    if(inode == NULL){
        log_msg("Couldn't find path - %s - in OPENDIR.\n", path);
        return -ENOENT;
    }
    //Check Accessibility
    int flag1 = checkAccess(inode);

    //Deny the access
    log_msg("Normal access %d (if 1 then yes)",flag1);
    if(flag1 == 0){
        return -EACCES;
    }
    //else return correct value
    log_msg("Exit into OPENDIR.\n");
    return 0;
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
    
    npheap_store *temp = NULL;
    struct dirent de;
    uint64_t u_offset = 2;
    uint64_t index = 0;
    char dir[236];
    char filename[128];

    log_msg("Into READDIR function.\n");
    // filler(buf, ".", NULL, 0);
    // filler(buf, "..", NULL, 0);

    int extract = extract_directory_file(dir, filename, path);
    log_msg("In readdir: directory - %s and file - %s for path %s \n", dir, filename,path);
    if(extract == 1){
        return -ENOENT;
    }

    for(u_offset = 2; u_offset < 502; u_offset++){
        temp= (npheap_store *)npheap_alloc(npheap_fd, u_offset, BLOCK_SIZE);
        if(temp==NULL){
            log_msg("NPheap alloc failed for offset : %d\n",u_offset);
        }

        for (index = 0; index < 16; index++){
            //log_msg("Search directory %s and file %s\n", dir, filename);
            //log_msg("Current directory %s and file %s\n", temp[index].dirname, temp[index].filename);
            if ((strcmp(temp[index].dirname, path) == 0) && (strcmp(temp[index].filename, "/")!=0)){
                /* Entry found in inode block */
                log_msg("Adding %s into dirent.\n", temp[index].filename);
                memset(&de, 0, sizeof(de));
                strcpy(de.d_name, temp[index].filename);
                if(filler(buf, temp[index].filename, NULL, 0) !=0){
                    return -ENOMEM;
                }
            }
        }
    }
    return 0;
}

/** Release directory
 */
int nphfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
    log_msg("Into release dir \n");
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

int nphfuse_access(const char *path, int mask){
    return 0;
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

//Allocate the superblock and the inode
static void initialAllocationNPheap(void){
    uint64_t offset = 1;
    npheap_store *npheap_dt = NULL;
    char *block_dt = NULL;
    npheap_store *head_dir = NULL;


    npheap_fd = open(nphfuse_data->device_name, O_RDWR);
    log_msg("Allocation started for %d.\n", offset);

    if(npheap_getsize(npheap_fd, offset) == 0){
        log_msg("Allocating root block into NPHeap!\n");
        block_dt = (char *)npheap_alloc(npheap_fd,offset, 8192);

        if(block_dt == NULL){
            log_msg("Allocation of root block failed.\n");
            return;
        }
        memset(block_dt,0, npheap_getsize(npheap_fd, offset));
    }

    log_msg("Allocation done for npheap %d.\n", npheap_getsize(npheap_fd, offset));

    for(offset = 2; offset < 502; offset++){
        //log_msg("Inode allocation for %d offset\n", offset);
        if(npheap_getsize(npheap_fd, offset)==0){
            block_dt = npheap_alloc(npheap_fd, offset, BLOCK_SIZE);
            memset(block_dt, 0, npheap_getsize(npheap_fd, offset));
        }
        log_msg("Inode allocation for %d offset and %d size\n", offset,(npheap_getsize(npheap_fd, offset)));
    }

    head_dir = getRootDirectory();
    log_msg("Assigning stat values\n");
    strcpy(head_dir->dirname, "/");
    strcpy(head_dir->filename, "/");
    head_dir->mystat.st_ino = inode_off;
    inode_off++;
    head_dir->mystat.st_mode = S_IFDIR | 0755;
    head_dir->mystat.st_nlink = 2;
    head_dir->mystat.st_size = npheap_getsize(npheap_fd,1);
    head_dir->mystat.st_uid = getuid();
    head_dir->mystat.st_gid = getgid();

    return;
}


void *nphfuse_init(struct fuse_conn_info *conn)
{
    log_msg("\nnphfuse_init()\n");
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    log_msg("Into init function \n");
    initialAllocationNPheap();

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
