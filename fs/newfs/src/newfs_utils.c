#include "newfs.h"

extern struct newfs_super_m  newfs_super_m; //用来管理全局位图和全局超级块
extern struct custom_options newfs_options;
boolean is_mounted;
int global_fd;
int global_sz_io;
int global_sz_disk;

/**
 * @brief 获取目录项 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct sfs_dentry* 
 */
struct newfs_dentry_m* newfs_get_dentry(struct newfs_inode_m * inode, int dir) {
    struct newfs_dentry_m* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* newfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int newfs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * @brief 
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 * 
 * @param path 
 * @return struct sfs_inode* 
 */
struct newfs_dentry_m* newfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct newfs_dentry_m* dentry_cursor = newfs_super_m.root_dentry;
    struct newfs_dentry_m* dentry_ret = NULL;
    struct newfs_inode_m*  inode; 
    int   total_lvl = newfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = newfs_super_m.root_dentry;
    }
    fname = strtok(path_cpy, "/"); //strtok和split差不多      
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            newfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (NEWFS_IS_FILE(inode) && lvl < total_lvl) {
            NEWFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (NEWFS_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            if (!is_hit) {
                *is_find = FALSE;
                NEWFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }

    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = newfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    //如果是文件则返回上一级目录的目录项,如果是目录则返回对应的目录项
    return dentry_ret;
}

/**
 * @brief 为一个inode分配dentry，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int newfs_alloc_dentry(struct newfs_inode_m* inode, struct newfs_dentry_m* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}

/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct sfs_inode* 
 */
struct newfs_inode_m* newfs_read_inode(struct newfs_dentry_m * dentry, int ino) {
    //初始化
    struct newfs_inode_m* inode = (struct newfs_inode_m*)malloc(sizeof(struct newfs_inode_m));
    struct newfs_inode inode_d;
    struct newfs_dentry_m* sub_dentry;
    struct newfs_dentry dentry_d;
    int    dir_cnt = 0, i;

    //读出指定的索引节点
    if (newfs_driver_read(NEWFS_INO_OFS(ino,newfs_super_m.data_offset), (uint8_t *)&inode_d, 
                        sizeof(struct newfs_inode),global_fd) != NEWFS_ERROR_NONE) {
        NEWFS_DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    memcpy(inode->target_path, inode_d.target_path, MAX_NAME_LEN);
    inode->dentry = dentry;
    inode->dentrys = NULL;
    if (NEWFS_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        for (i = 0; i < dir_cnt; i++)
        {
            if (newfs_driver_read(NEWFS_DATA_OFS(ino,newfs_super_m.data_offset) + i * sizeof(struct newfs_dentry), 
                                (uint8_t *)&dentry_d, 
                                sizeof(struct newfs_dentry),global_fd) != NEWFS_ERROR_NONE) {
                NEWFS_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
            sub_dentry = new_dentry_m(dentry_d.name, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino    = dentry_d.ino; 
            newfs_alloc_dentry(inode, sub_dentry);
        }
    }
    else if (NEWFS_IS_FILE(inode)) {
        inode->data = (uint8_t *)malloc(NEWFS_BLKS_SZ(NEWFS_DATA_PER_FILE));
        if (newfs_driver_read(NEWFS_DATA_OFS(ino,newfs_super_m.data_offset), (uint8_t *)inode->data, 
                            NEWFS_BLKS_SZ(NEWFS_DATA_PER_FILE),global_fd) != NEWFS_ERROR_NONE) {
            NEWFS_DBG("[%s] io error\n", __func__);
            return NULL;                    
        }
    }
    return inode;
}

/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int newfs_sync_inode(struct newfs_inode_m * inode) {
    struct newfs_inode  inode_d;
    struct newfs_dentry_m*  dentry_cursor;
    struct newfs_dentry dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    memcpy(inode_d.target_path, inode->target_path, MAX_NAME_LEN);
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    int offset;
    
    if (newfs_driver_write(NEWFS_INO_OFS(ino,newfs_super_m.data_offset), (uint8_t *)&inode_d, 
                     sizeof(struct newfs_inode),global_fd) != NEWFS_ERROR_NONE) {
        NEWFS_DBG("[%s] io error\n", __func__);
        return -NEWFS_ERROR_IO;
    }
                                                      /* Cycle 1: 写 INODE */
                                                      /* Cycle 2: 写 数据 */
    if (NEWFS_IS_DIR(inode)) {                          
        dentry_cursor = inode->dentrys;
        offset        = NEWFS_DATA_OFS(ino,newfs_super_m.data_offset);
        while (dentry_cursor != NULL)
        {
            memcpy(dentry_d.name, dentry_cursor->fname, MAX_NAME_LEN);
            dentry_d.ftype = dentry_cursor->ftype;
            dentry_d.ino = dentry_cursor->ino;
            if (newfs_driver_write(offset, (uint8_t *)&dentry_d, 
                                 sizeof(struct newfs_dentry),global_fd) != NEWFS_ERROR_NONE) {
                NEWFS_DBG("[%s] io error\n", __func__);
                return -NEWFS_ERROR_IO;                     
            }
            
            if (dentry_cursor->inode != NULL) {
                newfs_sync_inode(dentry_cursor->inode);
            }

            dentry_cursor = dentry_cursor->brother;
            offset += sizeof(struct newfs_dentry);
        }
    }
    else if (NEWFS_IS_FILE(inode)) {
        if (newfs_driver_write(NEWFS_DATA_OFS(ino,newfs_super_m.data_offset), inode->data, 
                             NEWFS_BLKS_SZ(NEWFS_DATA_PER_FILE),global_fd) != NEWFS_ERROR_NONE) {
            NEWFS_DBG("[%s] io error\n", __func__);
            return -NEWFS_ERROR_IO;
        }
    }
    return NEWFS_ERROR_NONE;
}

/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return sfs_inode
 */
struct newfs_inode_m* newfs_alloc_inode(struct newfs_dentry_m * dentry) {
    //初始化游标
    struct newfs_inode_m* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find_free_entry = FALSE;

    //寻找inode位图上未使用的inode节点
    for (byte_cursor = 0; byte_cursor < NEWFS_BLKS_SZ(newfs_super_m.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((newfs_super_m.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                newfs_super_m.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    //为目录项分配inode节点并建立联系
    if (!is_find_free_entry || ino_cursor == newfs_super_m.max_ino)
        return -NEWFS_ERROR_NOSPACE;

    inode = (struct newfs_inode_m*)malloc(sizeof(struct newfs_inode_m));
    inode->ino  = ino_cursor; 
    inode->size = 0;
                                                      /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;

    if(ino_cursor!=0){
        //只要不是根节点都为文件分配一个数据块
        //更新数据位图
        int data_no=ino_cursor;
        int data_byte_cursor=data_no/8;
        int start=data_no%8;
        newfs_super_m.map_data[data_byte_cursor]|=(0x1 << start);
    }

    //当前索引节点是否是一个文件的索引节点，若是则需要为索引节点分配数据区
    if (NEWFS_IS_FILE(inode)) {
        inode->data = (uint8_t *)malloc(NEWFS_BLKS_SZ(NEWFS_DATA_PER_FILE));
        // int data_no=ino_cursor*6;
        // int data_byte_cursor=data_no/8;
        // int start=data_no%8;
        // if(start<=2){
        //     for(int i=start;i<=start+5;i++){
        //         newfs_super_m.map_data[data_byte_cursor]|=(0x1 << i);
        //     }
        // }
        // else{
        //     //块1
        //     for(int i=start;i<=7;i++){
        //         newfs_super_m.map_data[data_byte_cursor]|=(0x1 << i);
        //     }

        //     //块2
        //     data_byte_cursor+=1;
        //     int end=start-2;
        //     for(int i=0;i<end;i++){
        //         newfs_super_m.map_data[data_byte_cursor]|=(0x1 << i);
        //     }
        // }
    }

    return inode;
}

/**
 * @brief 挂载sfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data Map | Data |
 * 
 * IO_SZ = BLK_SZ=512B
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int newfs_mount(struct custom_options options){
    int                 ret = NEWFS_ERROR_NONE;
    int                 driver_fd;
    struct newfs_super  newfs_super; 
    struct newfs_dentry_m*  root_dentry_m;
    struct newfs_inode_m*   root_inode_m;

    int                 inode_num;
    int                 map_inode_blks;
    int                 map_data_blks;
    int                 super_blks;
    boolean             is_init = FALSE;

    is_mounted = FALSE;
    newfs_super_m.is_mounted=is_mounted;
    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);

    if (driver_fd < 0) {
        return driver_fd;
    }

    global_fd = driver_fd;
    newfs_super_m.driver_fd=driver_fd;
    newfs_super.fd=driver_fd;
    ddriver_ioctl(global_fd, IOC_REQ_DEVICE_SIZE,  &global_sz_disk);
    ddriver_ioctl(global_fd, IOC_REQ_DEVICE_IO_SZ, &global_sz_io);
    newfs_super_m.sz_io=global_sz_io;
    newfs_super_m.sz_disk=global_sz_disk;

    root_dentry_m = new_dentry_m("/", NEWFS_DIR);

    if (newfs_driver_read(NEWFS_SUPER_OFS, (uint8_t *)(&newfs_super), 
                        sizeof(struct newfs_super),global_fd) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }   
                                                      /* 读取super */
    if (newfs_super.magic != NEWFS_MAGIC_NUM) {     /* 幻数无 */
                                                      /* 估算各部分大小 */
        //超级块占用的块数量
        super_blks = NEWFS_ROUND_UP(sizeof(struct newfs_super), NEWFS_IO_SZ) / NEWFS_IO_SZ;
        
        //索引位图的数量(位图大小，单位bit)
        inode_num  =  global_sz_disk / ((NEWFS_DATA_PER_FILE + NEWFS_INODE_PER_FILE) * NEWFS_IO_SZ);

        //索引位图的块数(Inode Map)
        map_inode_blks = NEWFS_ROUND_UP(NEWFS_ROUND_UP(inode_num, UINT32_BITS), (NEWFS_IO_SZ*8)) 
                         / (NEWFS_IO_SZ*8);
        
        //数据位图的大小(所占磁盘块的块数)
        map_data_blks=map_inode_blks;
                                                      /* 布局layout */
        newfs_super_m.max_ino = (inode_num - super_blks - map_inode_blks-map_data_blks);
        newfs_super.max_ino=(inode_num - super_blks - map_inode_blks-map_data_blks); 
        newfs_super.map_inode_offset = NEWFS_SUPER_OFS + NEWFS_BLKS_SZ(super_blks);
        newfs_super.map_data_offset = newfs_super.map_inode_offset + NEWFS_BLKS_SZ(map_inode_blks);
        newfs_super.data_offset=newfs_super.map_data_offset+NEWFS_BLKS_SZ(map_data_blks);
        newfs_super.map_inode_blks  = map_inode_blks;
        newfs_super.map_data_blks=map_data_blks;
        newfs_super.sz_usage    = 0;
        NEWFS_DBG("inode map blocks: %d\n", map_inode_blks);
        NEWFS_DBG("inode map blocks: %d\n", map_data_blks);

        is_init = TRUE;
    }
    newfs_super_m.sz_usage   = newfs_super.sz_usage;      /* 建立 in-memory 结构 */

    //初始化索引位图    
    newfs_super_m.map_inode = (uint8_t *)malloc(NEWFS_BLKS_SZ(newfs_super.map_inode_blks));
    newfs_super_m.map_inode_blks = newfs_super.map_inode_blks;
    newfs_super_m.map_inode_offset = newfs_super.map_inode_offset;

    //初始化数据位图
    newfs_super_m.map_data = (uint8_t *)malloc(NEWFS_BLKS_SZ(newfs_super.map_data_blks));
    newfs_super_m.map_data_blks = newfs_super.map_data_blks;
    newfs_super_m.map_data_offset = newfs_super.map_data_offset;

    //初始化数据区的偏移
    newfs_super_m.data_offset=newfs_super.data_offset;

    //读取索引位图的数据
    if (newfs_driver_read(newfs_super.map_inode_offset, (uint8_t *)(newfs_super_m.map_inode), 
                        NEWFS_BLKS_SZ(newfs_super.map_inode_blks),global_fd) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    //读取数据位图的数据
    if (newfs_driver_read(newfs_super.map_data_offset, (uint8_t *)(newfs_super_m.map_data), 
                        NEWFS_BLKS_SZ(newfs_super.map_data_blks),global_fd) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    if (is_init) {                                    /* 分配根节点 */
        root_inode_m = newfs_alloc_inode(root_dentry_m);
        newfs_sync_inode(root_inode_m);
    }
    
    root_inode_m            = newfs_read_inode(root_dentry_m, NEWFS_ROOT_INO);
    root_dentry_m->inode    = root_inode_m;
    newfs_super_m.root_dentry = root_dentry_m;
    newfs_super_m.is_mounted  = TRUE;
    
    newfs_dump_map();
    return ret;
}

/**
 * @brief 
 * 
 * @return int 
 */
int newfs_umount() {
    struct newfs_super  newfs_super; 

    if (!newfs_super_m.is_mounted) {
        return NEWFS_ERROR_NONE;
    }

    newfs_sync_inode(newfs_super_m.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    newfs_super.magic               = NEWFS_MAGIC_NUM;
    newfs_super.map_inode_blks      = newfs_super_m.map_inode_blks;
    newfs_super.map_inode_offset    = newfs_super_m.map_inode_offset;
    newfs_super.map_data_blks       = newfs_super_m.map_data_blks;
    newfs_super.map_data_offset     = newfs_super_m.map_data_offset;
    newfs_super.data_offset         = newfs_super_m.data_offset;
    newfs_super.sz_usage            = newfs_super_m.sz_usage;
    newfs_super.max_ino             = newfs_super_m.max_ino;

    //将超级块数据写入磁盘中
    if (newfs_driver_write(NEWFS_SUPER_OFS, (uint8_t *)&newfs_super, 
                     sizeof(struct newfs_super),global_fd) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    //将索引位图写入磁盘
    if (newfs_driver_write(newfs_super.map_inode_offset, (uint8_t *)(newfs_super_m.map_inode), 
                         NEWFS_BLKS_SZ(newfs_super.map_inode_blks),global_fd) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    //将数据位图写入磁盘
    if (newfs_driver_write(newfs_super.map_data_offset, (uint8_t *)(newfs_super_m.map_data), 
                         NEWFS_BLKS_SZ(newfs_super.map_data_blks),global_fd) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    //清空位图并关闭磁盘
    free(newfs_super_m.map_inode);
    free(newfs_super_m.map_data);
    ddriver_close(global_fd);

    return NEWFS_ERROR_NONE;
}


/**
 * @brief 驱动读
 * 
 * @param offset 偏移
 * @param out_content 读出的数据 
 * @param size 数据的大小
 * @param fd   磁盘的句柄
 * @return int 
 */
int newfs_driver_read(int offset, uint8_t *out_content, int size,int fd) {
    //此时读数据是按512B一块来读,未来可能需要改为1024B,只需要把NEWFS_IO_SZ替换乘1024B
    int      offset_aligned = NEWFS_ROUND_DOWN(offset, NEWFS_IO_SZ);
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NEWFS_ROUND_UP((size + bias), NEWFS_IO_SZ);
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(fd, offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(SFS_DRIVER(), cur, SFS_IO_SZ());
        ddriver_read(fd, cur, NEWFS_IO_SZ);
        cur          += NEWFS_IO_SZ;
        size_aligned -= NEWFS_IO_SZ;   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return NEWFS_ERROR_NONE;
}

/**
 * @brief 驱动写
 * 
 * @param offset 偏移
 * @param in_content 写入的数据
 * @param size  数据的大小
 * @param fd  磁盘句柄 
 * @return int 
 */
int newfs_driver_write(int offset, uint8_t *in_content, int size,int fd) {
    //此处存在的问题,同read
    int      offset_aligned = NEWFS_ROUND_DOWN(offset, NEWFS_IO_SZ);
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NEWFS_ROUND_UP((size + bias), NEWFS_IO_SZ);
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    newfs_driver_read(offset_aligned, temp_content, size_aligned,fd);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(fd, offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(SFS_DRIVER(), cur, SFS_IO_SZ());
        ddriver_write(fd, cur, NEWFS_IO_SZ);
        cur          += NEWFS_IO_SZ;
        size_aligned -= NEWFS_IO_SZ;   
    }

    free(temp_content);
    return NEWFS_ERROR_NONE;
}