#ifndef _TYPES_H_
#define _TYPES_H_

#define NEWFS_ERROR_NOSPACE       ENOSPC
#define NEWFS_ERROR_IO            EIO     /* Error Input/Output */
#define NEWFS_ERROR_NOTFOUND      ENOENT
#define NEWFS_ERROR_EXISTS        EEXIST
#define NEWFS_ERROR_UNSUPPORTED   ENXIO

#define MAX_NAME_LEN    128     
#define UINT32_BITS             32
#define UINT8_BITS              8

#define NEWFS_MAGIC_NUM           0x52415453  
#define NEWFS_SUPER_OFS           0
#define NEWFS_ERROR_NONE          0
#define NEWFS_IO_SZ               512
#define NEWFS_INODE_PER_FILE      1
#define NEWFS_DATA_PER_FILE       12
#define NEWFS_ROOT_INO            0
#define NEWFS_DEFAULT_PERM        0777

#define NEWFS_BLKS_SZ(blks)               (blks * NEWFS_IO_SZ)
#define NEWFS_IS_FILE(pinode)             (pinode->dentry->ftype == NEWFS_FILE)
#define NEWFS_IS_DIR(pinode)              (pinode->dentry->ftype == NEWFS_DIR)
#define NEWFS_ROUND_DOWN(value, round)    (value % round == 0 ? value : (value / round) * round)
#define NEWFS_ROUND_UP(value, round)      (value % round == 0 ? value : (value / round + 1) * round)
#define NEWFS_ASSIGN_FNAME(psfs_dentry, _fname)\
                                        memcpy(psfs_dentry->fname, _fname, strlen(_fname))
#define NEWFS_INO_OFS(ino,data_offset)                (data_offset + ino * NEWFS_BLKS_SZ((\
                                        NEWFS_INODE_PER_FILE + NEWFS_DATA_PER_FILE)))
#define NEWFS_DATA_OFS(ino,data_offset)               (NEWFS_INO_OFS(ino,data_offset) + NEWFS_BLKS_SZ(NEWFS_INODE_PER_FILE))

typedef int          boolean;
#define TRUE                    1
#define FALSE                   0
typedef enum file_type {
    NEWFS_FILE,           // 普通文件
    NEWFS_DIR             // 目录文件
} FILE_TYPE;


struct custom_options {
	const char*        device;
};
struct newfs_dentry;
struct newfs_inode;
struct newfs_super;

/**磁盘介质**/
struct newfs_super {
    uint32_t magic;
    int      fd;
    /* TODO: Define yourself */
    int      max_ino;                     // 最多支持的文件数
    int      map_inode_blks;              // inode位图占用的块数
    int      map_inode_offset;            // inode位图在磁盘上的偏移
    int      map_data_blks;               // data位图占用的块数
    int      map_data_offset;             // data位图在磁盘上的偏移
    int      sz_usage;                    //已经使用的大小
    int      data_offset;                 //数据区的偏移
};

struct newfs_inode {
    uint32_t ino;     //ino是索引位图的下标
    /* TODO: Define yourself */
    int           size;               // 文件已占用空间
    int           link;               // 链接数
    FILE_TYPE     ftype;              // 文件类型（目录类型、普通文件类型）
    int           dir_cnt;            // 如果是目录类型文件，下面有几个目录项
    // int           block_pointer[6];   // 数据块指针（可固定分配）
    char          target_path[MAX_NAME_LEN];/* store traget path when it is a symlink */
};

struct newfs_dentry{
    char     name[MAX_NAME_LEN];
    uint32_t ino;
    /* TODO: Define yourself */
    FILE_TYPE    ftype;                         // 指向的ino文件类型
    int          valid;                         // 该目录项是否有效
};

/**内存介质**/
struct newfs_inode_m
{
    int                ino;                           /* 在inode位图中的下标 */
    int                size;                          /* 文件已占用空间 */
    char               target_path[MAX_NAME_LEN];/* store traget path when it is a symlink */
    int                dir_cnt;
    struct newfs_dentry_m* dentry;                        /* 指向该inode的dentry */
    struct newfs_dentry_m* dentrys;                       /* 所有目录项 */
    uint8_t*           data;                              /* 文件的数据 */
    // int                block_pointer[6];       
}; 

struct newfs_dentry_m
{
    char               fname[MAX_NAME_LEN];
    struct newfs_dentry_m* parent;                        /* 父亲Inode的dentry */
    struct newfs_dentry_m* brother;                       /* 兄弟 */
    int                ino;
    struct newfs_inode_m*  inode;                         /* 指向inode */
    FILE_TYPE      ftype;
};

struct newfs_super_m
{
    int                driver_fd;
    
    int                sz_io;
    int                sz_disk;
    int                sz_usage;
    
    int                max_ino;
    uint8_t*           map_inode;
    uint8_t*           map_data;
    int                map_inode_blks;
    int                map_inode_offset;
    int                map_data_blks;               // data位图占用的块数
    int                map_data_offset;             // data位图在磁盘上的偏移
    int                data_offset;                 //数据区的偏移
    boolean            is_mounted;

    struct newfs_dentry_m* root_dentry;
};

static inline struct newfs_dentry_m* new_dentry_m(char * fname, FILE_TYPE ftype) {
    struct newfs_dentry_m * dentry = (struct newfs_dentry_m *)malloc(sizeof(struct newfs_dentry_m));
    memset(dentry, 0, sizeof(struct newfs_dentry_m));
    NEWFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;                                            
}
#endif /* _TYPES_H_ */