/*
 * file:        fs5600.c
 * description: skeleton file for CS 5600 system
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2019
 *
 * Modified by CS5600 staff, fall 2021.
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "fs5600.h"


/* if you don't understand why you can't use these system calls here,
 * you need to read the assignment description another time
 */
#define stat(a,b) error do not use stat()
#define open(a,b) error do not use open()
#define read(a,b,c) error do not use read()
#define write(a,b,c) error do not use write()


// === block manipulation functions ===

/* disk access.
 * All access is in terms of 4KB blocks; read and
 * write functions return 0 (success) or -EIO.
 *
 * read/write "nblks" blocks of data
 *   starting from block id "lba"
 *   to/from memory "buf".
 *     (see implementations in misc.c)
 */
extern int block_read(void *buf, int lba, int nblks);
extern int block_write(void *buf, int lba, int nblks);



/* bitmap functions
 */
void bit_set(unsigned char *map, int i)
{
    map[i/8] |= (1 << (i%8));
}
void bit_clear(unsigned char *map, int i)
{
    map[i/8] &= ~(1 << (i%8));
}
int bit_test(unsigned char *map, int i)
{
    return map[i/8] & (1 << (i%8));
}



unsigned char block_bitmap[FS_BLOCK_SIZE];
/*
 * Allocate a free block from the disk.
 *
 * success - return free block number
 * no free block - return -ENOSPC
 *
 * hint:
 *   - bit_set/bit_test might be useful.
 */
int alloc_blk() {
    // Read the block bitmap from the disk
    if (block_read(block_bitmap, 1, 1) < 0) {
        return -EIO;
    }

    // Find the first free block by checking each bit in the block bitmap
    int total_blocks = FS_BLOCK_SIZE * 8; // each bit represents a block
    for (int i = 0; i < total_blocks; i++) {
        if (!bit_test(block_bitmap, i)) {
            // Mark the found block as used
            bit_set(block_bitmap, i);

            // Write the updated block bitmap back to the disk
            if (block_write(block_bitmap, 1, 1) < 0) {
                return -EIO;
            }

            // Return the free block number
            return i;
        }
    }

    // If no free block is found, return -ENOSPC
    return -ENOSPC;
}

/*
 * Return a block to disk, which can be used later.
 *
 * hint:
 *   - bit_clear might be useful.
 */
void free_blk(int i) {
    // printf("\nfreeing block #%d\n", i);
    // Clear the corresponding bit in the block bitmap
    bit_clear(block_bitmap, i);

    // Write the updated block bitmap back to the disk
    block_write(block_bitmap, 1, 1);

}


// === FS global states ===

uint32_t num_blocks = 0;

int DIR_ENTRY_NUM = FS_BLOCK_SIZE / sizeof(struct fs_dirent);


// === FS helper functions ===


/* Two notes on path translation:
 *
 * (1) translation errors:
 *
 *   In addition to the method-specific errors listed below, almost
 *   every method can return one of the following errors if it fails to
 *   locate a file or directory corresponding to a specified path.
 *
 *   ENOENT - a component of the path doesn't exist.
 *   ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *             /a/b/c) is not a directory
 *
 * (2) note on splitting the 'path' variable:
 *
 *   the value passed in by the FUSE framework is declared as 'const',
 *   which means you can't modify it. The standard mechanisms for
 *   splitting strings in C (strtok, strsep) modify the string in place,
 *   so you have to copy the string and then free the copy when you're
 *   done. One way of doing this:
 *
 *      char *_path = strdup(path);
 *      int inum = ... // translate _path to inode number
 *      free(_path);
 */


/* EXERCISE 2:
 * convert path into inode number.
 *
 * how?
 *  - first split the path into directory and file names
 *  - then, start from the root inode (which inode/block number is that?)
 *  - then, walk the dirs to find the final file or dir.
 *    when walking:
 *      -- how do I know if an inode is a dir or file? (hint: mode)
 *      -- what should I do if something goes wrong? (hint: read the above note about errors)
 *      -- how many dir entries in one inode? (hint: read Lab4 instructions about directory inode)
 *
 * hints:
 *  - you can safely assume the max depth of nested dirs is 10
 *  - a bunch of string functions may be useful (e.g., "strtok", "strsep", "strcmp")
 *  - "block_read" may be useful.
 *  - "S_ISDIR" may be useful. (what is this? read Lab4 instructions or "man inode")
 * 
 *
 * programing hints:
 *  - there are several functionalities that you will reuse; it's better to
 *  implement them in other functions.
 */
void print_node_info(int inode_num, struct fs_inode curr_inode) {
    printf("inode#=%d\n",inode_num);
    printf("uid=%d\n", curr_inode.uid);
    printf("gid=%d\n", curr_inode.gid);
    printf("mode=%o\n", curr_inode.mode);
    printf("size=%d\n", curr_inode.size);
    printf("ctime=%d\n", curr_inode.ctime);
    printf("ctime=%d\n", curr_inode.mtime);

}

int path2inum(const char *path) {
    char *_path = strdup(path);
    char *token;
    char *tokens[10]; // 11 excl root dir
    int depth = 0;
    
    // 1. store all the dir name in array
    token = strtok(_path, "/");
    while (token != NULL) {
        tokens[depth] = token;
        depth++;
        token = strtok(NULL, "/");
    }
    
    // 2. start from root dir
    int curr_inode_num = 2; 
    if (depth==0) {
        return curr_inode_num;
    }

    // 3. curr i node that we will put in mem
    struct fs_inode inode_mem; 

    // 1. if cannot read this inode  "/" from disk quit
    if (block_read(&inode_mem, curr_inode_num, 1) < 0) {
        free(_path);
        printf("p2i=cannot read this inode root\n");
        return -EIO;
    }
    // 4. iterate trhough each token not incl root
    for (int token_i = 0; token_i < depth; token_i++) {

        char* token_name = tokens[token_i];
        // printf("token_i=%d, token_name=%s\n",token_i, token_name);
        // printf("isdir=%d\n",S_ISDIR(curr_inode.mode));
        
        // 2. make sure this is a directory. it its not a dir, then we cannot find the token name
        if (!S_ISDIR(inode_mem.mode)) {
            free(_path);
            printf("p2i=this is not a dir\n");
            return -ENOTDIR;
        }

        // 3.  load all the entries and find one that match the token name
        int token_found = 0;
            
        // get all its entries from disk (4096/32B = 128 entries) to memory
        // int DIR_ENTRY_NUM = FS_BLOCK_SIZE / sizeof(struct fs_dirent);
        struct fs_dirent dir_entries[DIR_ENTRY_NUM];

        if (block_read(dir_entries, inode_mem.ptrs[0], 1) < 0) {
            free(_path);
            printf("p2i=cannot read entries\n");
            return -EIO;
        }

        // Iterate through all the entries to find one that matches the curr token name
        for (int dir_entry_i = 0; dir_entry_i < DIR_ENTRY_NUM; dir_entry_i++) {
            // printf("entry name %d=%s, tokenname=%s\n", dir_entry_i, dir_entries[dir_entry_i].name, token_name);
            if (strcmp(dir_entries[dir_entry_i].name, token_name ) == 0 && dir_entries[dir_entry_i].valid == 1) {
                // printf("entry name MATCH\n");
                curr_inode_num = dir_entries[dir_entry_i].inode;
                token_found = 1;
                break;
            }
        }

        // 4. if token not found
        if (!token_found) {
            free(_path);
            // printf("p2i=no token found %d\n", -ENOENT);
            return -ENOENT; // file or dir not found erro

        // 5. if found, get the inode. it could be another directory or file inode
        } else {

            // if this is not the last token, get the inode for next iteraton
            if (token_i < depth -1) {
                // printf("\ngetinode for next\n");
                if (block_read(&inode_mem, curr_inode_num, 1) < 0) {
                    free(_path);
                    printf("p2i=cannot read inode for next iteration\n");
                    return -EIO;
                }
            }
        }

    } // finish iterate token. the last inode could be a file or dir inode

    return curr_inode_num;
}

/* EXERCISE 2:
 * Helper function:
 *   copy the information in an inode to struct stat
 *   (see its definition below, and the full Linux definition in "man lstat".)
 *
 *  struct stat {
 *        ino_t     st_ino;         // Inode number
 *        mode_t    st_mode;        // File type and mode
 *        nlink_t   st_nlink;       // Number of hard links (set to 1)
 *        uid_t     st_uid;         // User ID of owner
 *        gid_t     st_gid;         // Group ID of owner
 *        off_t     st_size;        // Total size, in bytes
 *        blkcnt_t  st_blocks;      // Number of blocks allocated
 *                                  // (note: block size is FS_BLOCK_SIZE;
 *                                  // and this number is an int which should be round up)
 *
 *        struct timespec st_atim;  // Time of last access (same as st_mtime)
 *        struct timespec st_mtim;  // Time of last modification 
 *        struct timespec st_ctim;  // Time of last status change
 *    };
 *
 *  [hints:
 *
 *    - what you should do is mostly copy.
 *
 *    - read fs_inode in fs5600.h and compare with struct stat.
 *
 *    - you can safely treat the types "ino_t", "mode_t", "nlink_t", "uid_t"
 *      "gid_t", "off_t", "blkcnt_t" as "unit32_t" in this lab.
 *
 *    - read "man clock_gettime" to see "struct timespec" definition
 *
 *    - the above "struct stat" does not show all attributes, but we don't care
 *      the rest attributes.
 *
 *    - for several fields in 'struct stat' there is no corresponding
 *    information in our file system:
 *      -- st_nlink - always set it to 1  (recall that fs5600 doesn't support links)
 *      -- st_atime - set to same value as st_mtime
 *  ]
 */

void inode2stat(struct stat *sb, struct fs_inode *in, uint32_t inode_num)
{
    memset(sb, 0, sizeof(*sb));

    /* your code here */
    sb->st_ino = inode_num;
    sb->st_mode = in->mode;
    sb->st_nlink = 1;  // fs5600 doesn't support links, so this is set to 1
    sb->st_uid = in->uid;
    sb->st_gid = in->gid;
    sb->st_size = in->size;
    sb->st_blocks = (in->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE; // Round up to the nearest block

    sb->st_atime= in->mtime;
    sb->st_mtime = in->mtime;
    sb->st_ctime = in->ctime;
}




// ====== FUSE APIs ========

/* init - this is called once by the FUSE framework at startup.
 *
 * The function reads the superblock and check if the magic number matches
 * FS_MAGIC. It also initializes two global variables:
 * "num_blocks" and "block_bitmap".
 *
 * notes:
 *   - use "block_read" to read data (if you don't know how it works, read its
 *     implementation in misc.c)
 *   - if there is an error, exit(1)
 */
void* fs_init(struct fuse_conn_info *conn)
{

    struct fs_super sb;
    // read super block from disk
    if (block_read(&sb, 0, 1) < 0) { exit(1); }

    // check if the magic number matches fs5600
    if (sb.magic != FS_MAGIC) { exit(1); }

    // EXERCISE 1:
    //  - get number of block and save it in global variable "numb_blocks"
    //    (where to know the block number? check fs_super in fs5600.h)
    num_blocks = sb.disk_size; // from superbloc
    
    //  - read block bitmap to global variable "block_bitmap"
    //    (this is a cache in memory; in later exercises, you will need to
    //    write it back to disk. When? whenever bitmap gets updated.)
    if (block_read(block_bitmap, 1, 1) < 0) {exit(1);}

    return NULL;
}


/* EXERCISE 1:
 * statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none.
 */

int calc_used_blocks() {
    int used_blocks_count = 0;
    // int used_blocks_indexes[num_blocks]; 

    // For each block
    for (int i = 0; i < num_blocks; i++) {
        // Iterate through each bit
        for (int j = 0; j < 8; j++) {
            if (block_bitmap[i] & (1 << j)) {
                // used_blocks_indexes[used_blocks_count] = i * 8 + j;
                used_blocks_count++;
            }
        }
    }

    // Print used blocks information
    // printf("%d = [", used_blocks_count);
    // for (int i = 0; i < used_blocks_count; i++) {
    //     printf("%d", used_blocks_indexes[i]);
    //     if (i < used_blocks_count - 1) {
    //         printf(", ");
    //     }
    // }
    // printf("]\n");
    return used_blocks_count;
}


int fs_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (ignore others):
     *   [DONE] f_bsize = FS_BLOCK_SIZE
     *   [DONE] f_namemax = <whatever your max namelength is>
     *   [TODO] f_blocks = total image - (superblock + block map)
     * # total data blocks in fs
     *   [TODO] f_bfree = f_blocks - blocks used
     *   [TODO] f_bavail = f_bfree
     *
     * it's okay to calculate this dynamically on the rare occasions
     * when this function is called.
     */

    st->f_bsize = FS_BLOCK_SIZE;
    st->f_namemax = 27;  // why? see fs5600.h
    
    st->f_blocks = num_blocks;
    st->f_bfree = st->f_blocks - calc_used_blocks();
    st->f_bavail = st->f_bfree;
    return 0;
}


/* EXERCISE 2:
 * getattr - get file or directory attributes. For a description of
 * the fields in 'struct stat', read 'man 2 stat'.
 *
 * You should:
 *  1. parse the path given by "const char * path",
 *     find the inode of the specified file,
 *       [note: you should implement the helfer function "path2inum"
 *       and use it.]
 *  2. copy inode's information to "struct stat",
 *       [note: you should implement the helper function "inode2stat"
 *       and use it.]
 *  3. and return:
 *     ** success - return 0
 *     ** errors - path translation, ENOENT
 */
void print_stat(struct stat *sb) {
    printf("\n>>>>>>>st_ino: %llu\n", (unsigned long long) sb->st_ino);
    printf("st_uid: %u\n", sb->st_uid);
    printf("st_gid: %u\n", sb->st_gid);
    printf("st_mode: %o\n", sb->st_mode);
    printf("st_size: %lld\n", (long long) sb->st_size);
    printf("st_blocks: %lld\n", (long long) sb->st_blocks);
    printf("st_atime: %lld\n", (long long) sb->st_atime);
    printf("st_mtime: %lld\n", (long long) sb->st_mtime);
    printf("st_ctime: %lld\n", (long long) sb->st_ctime);
}


int fs_getattr(const char *path, struct stat *sb)
{
    
    // 1. get the inode num and inode
    char *_path = strdup(path);
    int inodenum = path2inum(_path);
    

    // 2. check if inodenum returns error
    if (inodenum < 0) {
        printf("getattr-> cannot get inode num for %s\n", path);
        return inodenum; // return the error code
    }

    // 3. get the inode
    struct fs_inode curr_inode; 
    if (block_read(&curr_inode, inodenum, 1) < 0) {
        free(_path);
        printf("getattr-> cannot blockread for %s\n", path);
        return -EIO;
    }

    // 4. copy inodes info to struct stat
    inode2stat(sb, &curr_inode, inodenum);


    // 5. return success
    return 0;
}

// given a dir path, get the dir entries (array of dirrectory entries)
int get_dir_entries(const char *path, struct fs_dirent *dir_entries) {
    char *_path = strdup(path);
    int dir_inodenum = path2inum(_path);
    
    // 2. check if inodenum returns error
    if (dir_inodenum < 0) {
        return dir_inodenum; // return the error code
    }

    // 3. get the inode
    struct fs_inode dir_inode; 
    if (block_read(&dir_inode, dir_inodenum, 1) < 0) {
        free(_path);
        return -EIO;
    }
    // 4. make sure this is a dir's inode
    if (!S_ISDIR(dir_inode.mode)) {
        free(_path);
        return -ENOTDIR;
    }

    // 5. copy the dir_entries to mem 
    // get all its entries from disk (4096/32B = 128 entries) to memory
    if (block_read(dir_entries, dir_inode.ptrs[0], 1) < 0) {
        free(_path);
        return -EIO;
    }
    return 0;
}

/* EXERCISE 2:
 * readdir - get directory contents.
 *
 * call the 'filler' function for *each valid entry* in the
 * directory, as follows:
 *     filler(ptr, <name>, <statbuf>, 0)
 * where
 *   ** "ptr" is the second argument
 *   ** <name> is the name of the file/dir (the name in the direntry)
 *   ** <statbuf> is a pointer to the struct stat (of the file/dir)
 *
 * success - return 0
 * errors - path resolution, ENOTDIR, ENOENT
 *
 * hints:
 *   - this process is similar to the fs_getattr:
 *     -- you will walk file system to find the dir pointed by "path",
 *     -- then you need to call "filler" for each of
 *        the *valid* entry in this dir
 *   - you can ignore "struct fuse_file_info *fi" (also apply to later Exercises)
 */
int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{

    char *_path = strdup(path);

    // 1. get the directory entries of this dir
    struct fs_dirent dir_entries[DIR_ENTRY_NUM];
    int isValidDir = get_dir_entries(_path, dir_entries);
    if (isValidDir <0) {
        return isValidDir;
    }

    // 2. iterate trhough each entry
    for (int dir_entry_i = 0; dir_entry_i < DIR_ENTRY_NUM; dir_entry_i++) {
        if (dir_entries[dir_entry_i].valid == 1) {
            // 1. get the name of this entry
            char* entry_name = dir_entries[dir_entry_i].name;
            uint32_t entry_inodenum = dir_entries[dir_entry_i].inode;

            // 2. get the statbuf of this entry
            // - get inode
            struct fs_inode entry_inode;
            if (block_read(&entry_inode, entry_inodenum, 1) < 0) {
                free(_path);
                return -EIO;
            }

            // - use the inode to get statbuf
            struct stat entry_statbuf;
            inode2stat(&entry_statbuf, &entry_inode , entry_inodenum);

            // 3. fill
            filler(ptr, entry_name , &entry_statbuf, 0);

        }
    }
    // printf("finish iterating entries>>>>>>>>>>>\n");
    // return success
    return 0;
}


/* EXERCISE 3:
 * 1) read - read data from an open file.
 * 2) success: should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return #bytes from offset to end
 *   - on error, return <0
 * 3) Errors - path resolution, ENOENT, EISDIR
 * 
 * 4) argument interpretation:
 * - path: is the path of the file we want to open 
 * - buf: is the mem address we want to write the data to
 * - offset: starting point of the data we want to read from
 * - len: the number of bytes we want to read from file
 */
int fs_read(const char *path, char *buf, size_t len, off_t offset,
        struct fuse_file_info *fi)
{
    size_t bytes_num_to_read = len;
    off_t start_ith_byte = offset;
    
    // Part1: get the inode of the file
    int file_inum = path2inum(path);
    if (file_inum < 0) {
        return file_inum; // return the error code from path2inum
    }

    // Read the file inode from disk
    struct fs_inode file_inode;
    if (block_read(&file_inode, file_inum, 1) < 0) {
        return -EIO;
    }

    // Part 2: Validate
    // Check if the inode is a directory, return EISDIR if it is
    if (S_ISDIR(file_inode.mode)) {
        return -EISDIR;
    }
    
    // If the start_ith_byte is greater than or equal to the file size, return 0
    if (start_ith_byte >= file_inode.size) {
        return 0;
    }

    // Part 3: adjust size of bytes we want to read
    // Adjust the length if the start_ith_byte+len is greater than the file size
    if (start_ith_byte + bytes_num_to_read > file_inode.size) {
        // bytes_num_to_read we want to read from will be total inodesize - start_ith_byte
        // since we just want to read till the end
        bytes_num_to_read = file_inode.size - start_ith_byte;
    }
    // printf("\npath=%s, bytes_to_read=%ld, start_ith_byte=%d, file size=%d\n",
    // path, bytes_num_to_read, start_ith_byte, file_inode.size);

    off_t end_ith_byte = start_ith_byte + bytes_num_to_read;
    // Part 4: calculate the starting pointer and end pointer index
    int start_ptr_i = start_ith_byte / FS_BLOCK_SIZE;
    int end_ptr_i = (start_ith_byte + bytes_num_to_read -1) / FS_BLOCK_SIZE;
    // printf("start_ptr_i=%d, end_ptr_i=%d num_blocks_r=%d,bytes_num_to_read=%ld\n", 
    // start_ptr_i, end_ptr_i, num_blocks_r, bytes_num_to_read);


    // Part 5. iterate through each datablock and read
    for (int i = start_ptr_i; i < end_ptr_i + 1; i++) {
        
        // 1. get the whole block from disk to memory
        char mem_block[FS_BLOCK_SIZE];
        if (block_read(mem_block, file_inode.ptrs[i], 1) < 0) {
            return -EIO;
        }
        
        // 2. Calculate the start and end index to read within the current block
        // if this is the first block, get the start index of the bloxk is the starting ith byte mod blocksize
        // otherwise if its the 2nd, 3rd,.. block we have to read from 0
        off_t block_start_i = (i == start_ptr_i) ? start_ith_byte % FS_BLOCK_SIZE : 0;
        off_t block_end_i;
        
        // If it's the last block and
        // the last byte is not the last of the block, get the end index of the block
        if (i == end_ptr_i &&  (end_ith_byte % FS_BLOCK_SIZE != 0)) {
            block_end_i = end_ith_byte % FS_BLOCK_SIZE; // exclusive
        } else {
            // if ithe end happens to be the last index, just get the last index
            block_end_i = FS_BLOCK_SIZE;
        }

        // 3. Copy the data from the current block in memory to the buffer ptr
        const void *src = mem_block + block_start_i;

        size_t sz = block_end_i - block_start_i;
        memcpy(buf, src, sz);

        // 4. increment buffer address for next data block
        buf += (sz);


    }
    // Return the number of bytes read
    return bytes_num_to_read;
}

char *copy_string_with_length(const char *stringOriginal, size_t length) {
    char *stringCopy = malloc(length + 1);
    if (!stringCopy) {
        return NULL;
    }
    
    strncpy(stringCopy, stringOriginal, length);
    stringCopy[length] = '\0';

    return stringCopy;
}

/* EXERCISE 3:
 * rename - rename a file or directory
 * success - return 0
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */

char *get_parent_directory(const char *path) {

    // get the last slash = /dir1/file.txt becomes /file.txt
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        return NULL;
    }
    size_t parent_length = last_slash - path;

    // If parent_length is 0, it means the parent directory is the root directory
    if (parent_length == 0) {
        parent_length = 1; // Set parent_length to 1 to include the root directory '/'
    }

    // 1. init result
    char *parent_directory = malloc(parent_length + 1);
    if (!parent_directory) {
        return NULL;
    }

    // 2. cpy the path only up to certain length to parent dir
    strncpy(parent_directory, path, parent_length);

    // 3. end the char
    parent_directory[parent_length] = '\0';

    // 4. return
    return parent_directory;
}

int fs_rename(const char *src_path, const char *dst_path)
{
    // printf("\nsrc_path=%s, dst_path=%s\n", src_path, dst_path);
    char *src_parent = get_parent_directory(src_path);
    char *dst_parent = get_parent_directory(dst_path);
    // printf("src_parent=%s\n", src_parent);

    // 1. handle error    
    // -src_file path doesnt exist
    int src_inum = path2inum(src_path);
    if (src_inum < 0) {
        printf("src doesntexist=%d\n", src_inum);
        free(src_parent);
        free(dst_parent);
        return -ENOENT; // return the error code from path2inum
    }
    
    // - dst_parent doesnt exist
    int dst_parent_inum = path2inum(dst_parent);
    if (dst_parent_inum < 0) {
        printf("dst_parent doesntexist=%d\n", dst_parent_inum);
        free(src_parent);
        free(dst_parent);
        return dst_parent_inum; // return the error code from path2inum
    }

    // - exactly the same path
    if (strcmp(src_path, dst_path) == 0) {
        printf("destination already exists=%d\n", -EEXIST);
        free(dst_parent);
        free(src_parent);
        return -EEXIST;
    }

    // - different dir
    if (strcmp(src_parent, dst_parent) != 0) {
        printf("different dir EINVAL=%d\n", EINVAL);
        free(dst_parent);
        free(src_parent);
        return -EINVAL;
    }
    

    // 3. get the new name  including slash
    const char *dst_last_slash = strrchr(dst_path, '/');
    size_t new_name_len = strlen(dst_path) - (dst_last_slash + 1 - dst_path);
    char *new_name = copy_string_with_length(dst_last_slash + 1, new_name_len);
        
    // 4. get the old name including slash
    const char *src_last_slash = strrchr(src_path, '/');
    size_t old_name_len = strlen(src_path) - (src_last_slash + 1 - src_path);
    char *old_name = copy_string_with_length(src_last_slash + 1, old_name_len);

   
    // 5. get dir entries we know it must be valid here
    struct fs_dirent dir_entries[DIR_ENTRY_NUM];
    int isValidDir = get_dir_entries(src_parent, dir_entries);
    if (isValidDir < 0) {
        free(src_parent);
        free(dst_parent);
        printf("fsdf\n %d", isValidDir);
        return isValidDir;
    }

    
    // 6. iterate through each dir_entry and match the new name
    for (int dir_entry_i = 0; dir_entry_i < DIR_ENTRY_NUM; dir_entry_i++) {

        // if this is a valid entry 
        if (dir_entries[dir_entry_i].valid == 1) {

            // - get the name of this entry
            char* entry_name = dir_entries[dir_entry_i].name;

            // - found a match 
            if (strcmp(entry_name, old_name) == 0) {
                // - change the name
                strncpy(dir_entries[dir_entry_i].name, new_name, strlen(new_name));
                dir_entries[dir_entry_i].name[strlen(new_name)] = '\0';
            
                // - get the inode of the parent directory so we can rewrite their 
                // entries
                struct fs_inode src_parent_inode; 
                int src_parent_inum = path2inum(src_parent);

                // - get the inode of the parrent dir
                if (block_read(&src_parent_inode, src_parent_inum, 1) < 0) {
                    printf("EIO\n");
                    return -EIO;

                }
                // - Write the updated block in mem(dir_entries)
                // back to disk src_parent_inode.ptrs[0]
                if (block_write(dir_entries, src_parent_inode.ptrs[0], 1) < 0) {
                    // print("error writing dir_entries to disk\n");
                    free(new_name);
                    free(old_name);
                    free(dst_parent);
                    free(src_parent);
                    return -EIO;
                }

                printf("new entry name=%s\n", dir_entries[dir_entry_i].name);
                printf("success=%d\n", 0);
                free(new_name);
                free(old_name);
                free(dst_parent);
                free(src_parent);
                return 0;
            }
        }
    }

    // not file matches
    printf("enoent=%d\n", -ENOENT);
    free(new_name);
    free(old_name);
    free(dst_parent);
    free(src_parent);
    return -ENOENT;
    
}

/* EXERCISE 3:
 * chmod - change file permissions
 *
 * success - return 0
 * Errors - path resolution, ENOENT.
 *
 * hints:
 *   - You can safely assume the "mode" is valid.
 *   - notice that "mode" only contains permissions
 *     (blindly assign it to inode mode doesn't work;
 *      why? check out Lab4 instructions about mode)
 *   - S_IFMT might be useful. S_IFMT  = 0o0170000  # bit mask for the file type bit field
 */
int fs_chmod(const char *path, mode_t mode)
{   

    // 1. handle error
    int inum = path2inum(path);
    if (inum < 0) {
        return -ENOENT; 
    }

    // 2. get the inode
    struct fs_inode inode; 
    if (block_read(&inode, inum, 1) < 0) {
        return -EIO;
    }

    // 3. chnge the inoode mode
    // Preserve the file type bits and apply the new permissions
    inode.mode = (inode.mode & S_IFMT) | (mode & ~S_IFMT);


    // 4. write this bloc back to the inum
    if (block_write(&inode, inum, 1) < 0) {
        
        return -EIO;
    }

    return 0;
}


/* EXERCISE 4:
 * create - create a new file with specified permissions
 *
 * success - return 0
 * errors - path resolution, EEXIST
 *          in particular, for create("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If there are already 128 entries in the directory (i.e. it's filled an
 * entire block), you are free to return -ENOSPC instead of expanding it.
 * If the name is too long (longer than 27 letters), return -EINVAL.
 *
 * notes:
 *   - that 'mode' only has the permission bits. You have to OR it with S_IFREG
 *     before setting the inode 'mode' field.
 *   - Ignore the third parameter.
 *   - you will have to implement the helper funciont "alloc_blk" first
 *   - when creating a file, remember to initialize the inode ptrs.
 */


/**
 * Helper 4.1 to get parent's data block to memory and insert a fs_dirent entry to this mem
*/
int insert_entry(struct fs_dirent *parent_data, struct fs_inode parent_inode, struct fs_dirent newdir_entry) {
    if (block_read(parent_data, parent_inode.ptrs[0],1) <0) {
        printf("cannot read parent data\n");
        return -EIO;
    }

    // int entry_x = 0;
    // iterate through each entry and find invalid_entry_i
    for ( int entry_i = 0; entry_i <DIR_ENTRY_NUM; entry_i++ ) {

        // found an empty slot 
        if (parent_data[entry_i].valid == 0) {
            // fil in with fs_dirent 
            parent_data[entry_i] = newdir_entry;
            return 0;
        }
    }

    // all entries have been filled
    return -ENOSPC;
}

/**
 * Helper 4.2 to validate create and mkdirs inode, inum, newdirfile_inum, new_df_name_len
*/
int isvalid_create_mkdr(int parent_inum, struct fs_inode *parent_inode, int newdifi_inum, int new_difiname_len) {

    // 1. parent path error
    // 1A. Parent doesnt exist
    if (parent_inum <0) {
        printf("cannot find parent inum\n");
        return -ENOENT; 
    }

    // 1B parent isnt directory
    // get inode from inum
    if (block_read(parent_inode, parent_inum, 1) < 0) {
        printf("cannot read parent inode\n");
        return -EIO;
    }

    // check if inode is a dir
    if (!S_ISDIR(parent_inode->mode)){
        printf("parent is not a dir\n");
        return -ENOTDIR;
    }

    // 2 new directory error
    // 2A dir already exist
    if (newdifi_inum >= 0) {
        return -EEXIST;
    }

    // 2B num too long
    if (new_difiname_len > 27) {
        printf("name too long\n");
        return -EINVAL;
    }
    return 0;
}

/**
 * Helper 4.3 finds free block number from blockbitmap
*/
int find_free_block_number(int exclude_block_num) {
    for (int i = 0; i < num_blocks; ++i) {
        if (i == exclude_block_num) {
            continue;
        }
        if (!bit_test(block_bitmap, i)) {
            return i; // Found a free block
        }
    }

    return -1; // No free blocks found
}

/**
 * Helper 4.4 that implements both fs_create and fs_mkdir
*/
int create_mkdir_helper(const char *path, mode_t mode, int isDir) {
    // printf("\nmkdir/create path=>>>>%s\n", path);
    uint32_t cur_time = time(NULL);
    struct fuse_context *ctx = fuse_get_context();
    uint16_t uid = ctx->uid;
    uint16_t gid = ctx->gid;
    
    // Part 0: handle errors:
    // - get parent's path, inum, inode
    char * parent_path = get_parent_directory(path);
    int parent_inum = path2inum(parent_path);
    struct fs_inode parent_inode; 

    // - get dir/file 's inum
    int newdifi_inum = path2inum(path);

    // - get dir/file's name' length
    const char *path_last_slash = strrchr(path, '/');
    size_t new_difiname_len = strlen(path) - (path_last_slash + 1 - path);

    // - validate
    int isValid = isvalid_create_mkdr(parent_inum, &parent_inode, newdifi_inum, new_difiname_len);
    if ( isValid < 0) {
        free(parent_path);
        return isValid;
    }

    // PART 1: find two empty block number from bitmap 1 for newdifi inode 1 for newdifi data
    // read bitmap block assume theres always free node
    int newdifi_inode_num = find_free_block_number(-1);
    int newdifi_datablock_num = find_free_block_number(newdifi_inode_num);

    // PART 2B: create the inode of this new dir and fillin
    struct fs_inode newdifi_inode;
    newdifi_inode.gid = gid;
    newdifi_inode.uid = uid;
    newdifi_inode.ctime = cur_time;
    newdifi_inode.mtime = cur_time;
    newdifi_inode.size = 0;
    newdifi_inode.ptrs[0] = newdifi_datablock_num;

    // get the permission bit of given mode
    // permission bits = ~S_IFMT --> not the type bits
    // permission bits of given moode = mode & ~S_IFMT --> not the type bits
    mode_t permission_bit_given_mode = mode & ~S_IFMT;
    if (isDir > 0) {
        newdifi_inode.mode = S_IFDIR | permission_bit_given_mode;
    } else {
        newdifi_inode.mode = S_IFREG | permission_bit_given_mode;
    }
    

    // PART 3: fill in the fs_dirent
    struct fs_dirent newdifi_entry;
    newdifi_entry.valid = 1;
    newdifi_entry.inode = newdifi_inode_num;
    strncpy(newdifi_entry.name, path_last_slash + 1, new_difiname_len);
    newdifi_entry.name[new_difiname_len] = '\0';
    
    // PART 4: get the data of parent_dir and insert new entry to it
    struct fs_dirent parent_data[DIR_ENTRY_NUM];
    int entry_i = insert_entry(parent_data, parent_inode, newdifi_entry);
    if (entry_i <0) {
        free(parent_path);
        return entry_i;
    }

    // PART 5: update parent's inode meta
    parent_inode.ctime = cur_time;
    parent_inode.mtime = cur_time;
    parent_inode.size += sizeof(struct fs_dirent);

    // PART 6: update bitmap set to 1s on the number we used for inode and data
    bit_set(block_bitmap, newdifi_inode_num);
    bit_set(block_bitmap, newdifi_datablock_num);

    // PART 7: write everything back to disk
    // printf("parent_inum=%d\n", newdifi_inode_num);
    // parent's inode and data
    int rv;
    block_write(&parent_inode, parent_inum, 1); // mem address, block number in disk , length
    block_write(parent_data, parent_inode.ptrs[0], 1);

    // new inode and data
    block_write(&newdifi_inode, newdifi_inode_num, 1);
    

    // PART 2A: create the data of this new dir
    if (isDir > 0) {
        struct fs_dirent empty_entries[DIR_ENTRY_NUM];
        memset(empty_entries, 0, sizeof(empty_entries));  
        block_write(empty_entries, newdifi_inode.ptrs[0], 1);
    } else {
        char empty_text[FS_BLOCK_SIZE];
        memset(empty_text, 0, sizeof(empty_text));
        block_write(empty_text, newdifi_inode.ptrs[0], 1);
    }

    block_write(block_bitmap, 1, 1);

    // check path:
    // int testnewinum = path2inum(path);
    // printf("freeblock is=>%d, after assign block=>%d\n", newdifi_inode_num, testnewinum);
    
    free(parent_path);


    
    return 0;
}

int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    return create_mkdir_helper(path, mode, -1);
}

/* EXERCISE 4:
 * mkdir - create a directory with the given mode.
 *
 * Note that 'mode' only has the permission bits. You
 * have to OR it with S_IFDIR before setting the inode 'mode' field.
 *
 * success - return 0
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 *
 * hint:
 *   - there is a lot of similaries between fs_mkdir and fs_create.
 *     you may want to reuse many parts (note: reuse is not copy-paste!)
 */
int fs_mkdir(const char *path, mode_t mode)
{
    return create_mkdir_helper(path,mode, 1);
}

/**
 * Helper 5.1 to validate create and mkdirs inode, inum, newdirfile_inum, new_df_name_len
*/
int isvalid_unlink_rmdir(int parent_inum, struct fs_inode *parent_inode, int difi_inum, struct fs_inode *difi_inode, int isDir) {

    // 1. parent path error
    // 1A. Parent doesnt exist
    if (parent_inum <0) {
        printf("cannot find parent inum\n");
        return -ENOENT; 
    }

    if (block_read(parent_inode, parent_inum, 1) < 0) {
        printf("cannot read parent inode\n");
        return -EIO;
    }

    // 1B parent isnt directory
    // get inode from inum
    // check if inode is a dir
    if (!S_ISDIR(parent_inode->mode)){
        printf("parent is not a dir\n");
        return -ENOTDIR;
    }

    // 2 file or dir to be deleted error
    // 2A file or dir doesnt exist
    if (difi_inum < 0) {
        printf("file or dir already exist\n");
        return -ENOENT;
    }

    if (block_read(difi_inode, difi_inum, 1) < 0) {
        printf("cannot read dir or file inode\n");
        return -EIO;
    }

    // 2B file's inode type is a dir
    if (isDir < 0) { // supposed to be a file
        if (S_ISDIR(difi_inode->mode)){
            printf("file is not a file type\n");
            return -EISDIR;
        }
    } else { // supoosed to be a dir

        // not dir
        if ( !S_ISDIR(difi_inode->mode)){
            printf("dir is not a file dir\n");
            return -ENOTDIR;
        // not empty
        } else if (difi_inode->size != 0) {
            return -ENOTEMPTY;
        }
    }
    return 0;
}

int helper_unlink_rmdir(const char *path, int isDir){

    
    char * parent_path = get_parent_directory(path);
    int parent_inum = path2inum(parent_path);
    int difi_inum = path2inum(path);

    struct fs_inode parent_inode;
    struct fs_inode difi_inode;

    // Part 0A : validate
    int isValid = isvalid_unlink_rmdir(parent_inum, &parent_inode, difi_inum, &difi_inode, isDir);
    if (isValid < 0) {
        free(parent_path);
        return isValid;
    }

    // Part 0B: get other necessary vars
    // - parent's data
    struct fs_dirent parent_data[DIR_ENTRY_NUM];
    if (block_read(parent_data, parent_inode.ptrs[0],1) <0) {
        printf("cannot read parent data\n");
        return -EIO;
    }

    // - get dir/file's name
    const char *difi_name = strrchr(path, '/')+1;

    // Part1: free the data
    if (isDir < 0) {
        int num_ptrs = (difi_inode.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;

        // if there's no existing ptr(empty textfile)
        if (num_ptrs == 0) {
            free_blk(difi_inode.ptrs[0]); // just remove that 1 empty datablock
        } else {
            for (int ptr_i = 0; ptr_i < num_ptrs; ptr_i++){
                free_blk(difi_inode.ptrs[ptr_i]);
            }
        }
    } else {
        free_blk(difi_inode.ptrs[0]);
    }

    // Part 2: update parents datablock and free the difi inode
    for (int entry_i=0; entry_i < DIR_ENTRY_NUM; entry_i++) {
        if (strcmp(difi_name,  parent_data[entry_i].name) == 0) {

            // set valid to 0
            parent_data[entry_i].valid = 0;

            // free the inode
            // printf("inode to free=%d\n",parent_data[entry_i].inode);
            free_blk(parent_data[entry_i].inode);
        }
    }

    // Part 3: Update parents inode
    parent_inode.mtime = time(NULL);
    parent_inode.size -= sizeof(struct fs_dirent); // there's 1 less entry in its data


    // Part 4: block write
    // block_write(block_bitmap, 1, 1);
    block_write(&parent_inode, parent_inum, 1);
    block_write(parent_data, parent_inode.ptrs[0], 1);
    free(parent_path);
    return 0;
}
/* EXERCISE 5:
 * unlink - delete a file
 *  success - return 0
 *  errors - path resolution, ENOENT, EISDIR
 *
 * hint:
 *   - you will have to implement the helper funciont "free_blk" first
 *   - remember to delete all data blocks as well
 *   - remember to update "mtime"
 */

int fs_unlink(const char *path)
{
   return helper_unlink_rmdir(path, -1);
}

/* EXERCISE 5:
 * rmdir - remove a directory
 *  success - return 0
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 *
 * hint:
 *   - fs_rmdir and fs_unlink have a lot in common; think of reuse the code
 */
int fs_rmdir(const char *path)
{
    return helper_unlink_rmdir(path, 1);
    // return -EOPNOTSUPP;
}


/**
 * 6.1 helper validate
*/

int helper_validate_write(const char* path, struct fs_inode* file_inode, int *file_inum, size_t bytes_num_to_write, off_t start_ith_byte) {
    // Part 1: path validation
    // inum not found
    *file_inum = path2inum(path);
    if (*file_inum < 0)  {
        printf("hvw: cannot find path\n");
        return -ENOENT;
    }
    if (block_read(file_inode, *file_inum, 1) < 0) {
        printf("hvw: cannot block read\n");
        return -EIO;
    }
    // inode is not dir
    if(S_ISDIR(file_inode->mode)) {
        printf("hvw: file is dir\n");
        return -EISDIR;
    }

    // Part 2: size validation
    // offset greater than current file length
    if (start_ith_byte > file_inode->size) {
        printf("hvw: offset greater than current size\n");
        return -EINVAL;
    }
    // calculate the maximum file size
    size_t max_file_size = FS_BLOCK_SIZE * (sizeof(file_inode->ptrs) / sizeof(file_inode->ptrs[0]));

    // total data exceed max size of file
    if (start_ith_byte + bytes_num_to_write > max_file_size) {
        printf("hvw: total data exceeds\n");
        return -ENOSPC;
    }
    return 0;
}
/* EXERCISE 6:
 * write - write data to a file
 * success - return number of bytes written. (this will be the same as
 *           the number requested, or else it's an error)
 *
 * Errors - path resolution, ENOENT, EISDIR, ENOSPC
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them,
 *   but we don't)
 *  return ENOSPC when the data exceed the maximum size of a file.
 * 
 */
int fs_write(const char *path, const char *buf, size_t len,
         off_t offset, struct fuse_file_info *fi)
{



    // Part 0. Validate
    size_t bytes_num_to_write = len;
    off_t start_ith_byte = offset;
    off_t end_ith_byte = start_ith_byte + bytes_num_to_write;




    struct fs_inode file_inode;
    int file_inum;
    
    int isValid = helper_validate_write(path, &file_inode, &file_inum, bytes_num_to_write, start_ith_byte);
    if (isValid < 0) {
        printf("is not valid\n");
        return isValid;
    }


    // Part 1. Get start_ptr_i and end_ptr_i so we can read all the required block number from the file_inode
    int start_ptr_i = start_ith_byte / FS_BLOCK_SIZE;
    int end_ptr_i = (end_ith_byte - 1) / FS_BLOCK_SIZE;
    // printf("    start_ptr_i=%d,end_ptr_i=%d\n", start_ptr_i, end_ptr_i);
    
    // PART 2&3. go to each block of disk
    for (int curr_ptr_i = start_ptr_i; curr_ptr_i <= end_ptr_i; curr_ptr_i++) {

        // Part 2: get the inum we want to write the data to
        // - 2.1 get the data inum = 
        int data_inum = file_inode.ptrs[curr_ptr_i];

        // - 2.2 init the mem_fullblock
        char mem_fullblock[FS_BLOCK_SIZE];  
        memset(mem_fullblock, 0, FS_BLOCK_SIZE);

        int allocateNewBlk = 0;
        // - 2.2 Case A datablock already exsit. block_read to mem
        if (data_inum >= 3 && data_inum < num_blocks && (bit_test(block_bitmap, data_inum) != 0)){
            
            // printf("    data already exist=%d\n", bit_test(block_bitmap, data_inum)); // gives me 16
            if (block_read(mem_fullblock, data_inum, 1) < 0) {
                return -EIO;
            }
        // - 2.3 case B doesnt exist alloc block for this
        } else {
            allocateNewBlk=1;
            // printf("    data not yet exist=%d\n", bit_test(block_bitmap, data_inum)); // gives me 32
            // - alloc to file inode ptrs i
            file_inode.ptrs[curr_ptr_i] = alloc_blk();
        }

        

        // Part 3: write block to disk: REMEMBER TO DO free alloc again if anythin fails here
        // - 3.1 get the block start_i
        off_t block_start_i;
        // if this is the first block, find the block_start_i manuallya
        if (curr_ptr_i == start_ptr_i) {
            block_start_i = start_ith_byte % FS_BLOCK_SIZE;
        } else {
            block_start_i = 0;
        }

        // - 3.2 get the block end_i
        off_t block_end_i;
        if (curr_ptr_i == end_ptr_i && (end_ith_byte % FS_BLOCK_SIZE != 0)) {
            block_end_i = end_ith_byte % FS_BLOCK_SIZE; // all the end i are exclusive
        } else {
            block_end_i = FS_BLOCK_SIZE;
        }

        // - 3.3 get the length of the data to write
        size_t len_write_perblock = block_end_i - block_start_i;

        // - 3.4 memcpy from buffer to dst (mem_full_bloc + block_start_i)
        void *dst = mem_fullblock + block_start_i;
        memcpy(dst, buf, len_write_perblock);
        // printf("    mem_fullblock=%p\n", mem_fullblock);
        // printf("    dst=%p\n", dst);
        
        // - 3.5 block_write this data to the inum we get from part 2
        if (block_write(mem_fullblock, file_inode.ptrs[curr_ptr_i], 1) < 0) {
            if (allocateNewBlk ==1) {
                free_blk(file_inode.ptrs[curr_ptr_i]);
            }
            // handle free blok again
            return -EIO;
        }

        // ---->Print what's been written on the mem_fullblock
        // printf("    buf contents (hex): ");
        // for (size_t i = 0; i < len; i++) {
        //     printf("%02x ", (unsigned char)buf[i]);
        // }

        // printf("\n");
        // printf("    mem_fullblock written portion: ");
        // for (off_t i = block_start_i; i < block_end_i; i++) {
        //     printf("%02x ", (unsigned char)mem_fullblock[i]);
        // }
        // printf("\n");

        // - 3.6 increment buffer for next write
        buf += len_write_perblock;
    }


        
    // Part 4. update file_inode'size
    if (end_ith_byte > file_inode.size) {
        file_inode.size = end_ith_byte;
    }
    file_inode.mtime = time(NULL);

    // Part 5. block write file_inode
    if (block_write(&file_inode, file_inum, 1) < 0) {
        // handle free bok again
        return -EIO;
    }

    return len;
}

/* EXERCISE 6:
 * truncate - truncate file to exactly 'len' bytes
 * note that CS5600 fs only allows len=0, meaning discard all data in this file.
 *
 * success - return 0
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
int fs_truncate(const char *path, off_t len)
{
    // Part 1. Validation
    if (len > 0) {
        return -EINVAL;        /* invalid argument */
    }
    int file_inum = path2inum(path);
    if (file_inum < 0) {
        return -ENOENT;
    }
    struct fs_inode file_inode;
    if (block_read(&file_inode, file_inum, 1) < 0) {
        return -EIO;
    }

    if (S_ISDIR(file_inode.mode)) {
        return -EISDIR; // Is a directory
    }

    // Part 2. Iterate through all data blocks of the inode apart from the first
    int num_ptrs = (file_inode.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;

    for (int i = 1; i < num_ptrs; i++) {
        int data_inum = file_inode.ptrs[i];
        if (data_inum >= 3 && data_inum < num_blocks && (bit_test(block_bitmap, data_inum) != 0)) {
            free_blk(data_inum); // will set that bit to 0
        }
    }
    // Update file size and write the updated inode to the disk
    file_inode.size = 0;
    if (block_write(&file_inode, file_inum, 1) < 0) {
        return -EIO;
    }
    


    return 0;
}

/* EXERCISE 6:
 * Change file's last modification time.
 *
 * notes:
 *  - read "man 2 utime" to know more.
 *  - when "ut" is NULL, update the time to now (i.e., time(NULL))
 *  - you only need to use the "modtime" in "struct utimbuf" (ignore "actime")
 *    and you only have to update "mtime" in inode.
 *
 * success - return 0
 * Errors - path resolution, ENOENT
 */
int fs_utime(const char *path, struct utimbuf *ut)
{
    // 1. get inum
    int inum = path2inum(path);
    if (inum < 0) {
        return -ENOENT;
    }

    // 2. get inode
    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        return -EIO;
    }

    time_t curr = time(NULL);
    // 3. if ut is null update time to now
    if (ut == NULL) {
        ut = malloc(sizeof(struct utimbuf));
        
        ut->modtime = curr;
        inode.mtime = curr;
    } else {
        ut->modtime = inode.mtime;
    }

    return 0;

}



/* operations vector. Please don't rename it, or else you'll break things
 */
struct fuse_operations fs_ops = {
    .init = fs_init,            /* read-mostly operations */
    .statfs = fs_statfs,
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .read = fs_read,
    .rename = fs_rename,
    .chmod = fs_chmod,

    .create = fs_create,        /* write operations */
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .write = fs_write,
    .truncate = fs_truncate,
    .utime = fs_utime,
};

