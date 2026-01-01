#ifndef VSFS_H
#define VSFS_H

#include <stdint.h>

// Core FS Constants 
#define FS_MAGIC          0x56534653U
#define BLOCK_SIZE        4096U
#define INODE_SIZE        128U
#define TOTAL_BLOCKS      85U

// Layout Indices 
#define JOURNAL_BLOCK_IDX   1U
#define JOURNAL_BLOCKS      16U
#define INODE_BMAP_IDX      17U
#define DATA_BMAP_IDX       18U
#define INODE_START_IDX     19U
#define DATA_START_IDX      21U

// Journal Specifics 
#define JOURNAL_MAGIC     0x4A524E4C
#define REC_DATA          1
#define REC_COMMIT        2

// FS Structures 
struct superblock {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_count;
    uint32_t journal_block;
    uint32_t inode_bitmap;
    uint32_t data_bitmap;
    uint32_t inode_start;
    uint32_t data_start;
    uint8_t  _pad[128 - 9 * 4];
};

struct inode {
    uint16_t type;  // 0=free, 1=file, 2=dir 
    uint16_t links;
    uint32_t size;
    uint32_t direct[8];
    uint32_t ctime;
    uint32_t mtime;
    uint8_t _pad[128 - (2 + 2 + 4 + 8 * 4 + 4 + 4)];
};

struct dirent {
    uint32_t inode;
    char name[28];
};

// Journaling Structures 
struct journal_header {
    uint32_t magic;
    uint32_t nbytes_used;
};

struct rec_header {
    uint16_t type;
    uint16_t size;
};

struct data_record {
    struct rec_header hdr;
    uint32_t block_no;
    uint8_t data[BLOCK_SIZE];
};

struct commit_record {
    struct rec_header hdr;
};

#endif