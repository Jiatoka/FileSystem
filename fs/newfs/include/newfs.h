#ifndef _NEWFS_H_
#define _NEWFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"

#define NEWFS_MAGIC                  /* TODO: Define by yourself */
#define NEWFS_DEFAULT_PERM    0777   /* 全权限打开 */
#define NEWFS_DBG(fmt, ...) do { printf("NEWFS_DBG: " fmt, ##__VA_ARGS__); } while(0) 

int 			   newfs_driver_read(int offset, uint8_t *out_content, int size,int fd);
int 			   newfs_driver_write(int offset, uint8_t *in_content, int size,int fd);
int 			   newfs_mount(struct custom_options options);
int 			   newfs_umount();
struct newfs_inode_m* newfs_alloc_inode(struct newfs_dentry_m * dentry); 
int 			   newfs_sync_inode(struct newfs_inode_m * inode);
struct newfs_inode_m* newfs_read_inode(struct newfs_dentry_m * dentry, int ino);
int 			   newfs_alloc_dentry(struct newfs_inode_m* inode, struct newfs_dentry_m* dentry);
void 			   newfs_dump_map();
/******************************************************************************
* SECTION: newfs.c
*******************************************************************************/
void* 			   newfs_init(struct fuse_conn_info *);
void  			   newfs_destroy(void *);
int   			   newfs_mkdir(const char *, mode_t);
int   			   newfs_getattr(const char *, struct stat *);
int   			   newfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   newfs_mknod(const char *, mode_t, dev_t);
int   			   newfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   newfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   newfs_access(const char *, int);
int   			   newfs_unlink(const char *);
int   			   newfs_rmdir(const char *);
int   			   newfs_rename(const char *, const char *);
int   			   newfs_utimens(const char *, const struct timespec tv[2]);
int   			   newfs_truncate(const char *, off_t);
			
int   			   newfs_open(const char *, struct fuse_file_info *);
int   			   newfs_opendir(const char *, struct fuse_file_info *);

#endif  /* _newfs_H_ */