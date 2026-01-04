//vsfs.h codes
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>


#define FS_MAGIC          0x56534653U
#define BLOCK_SIZE        4096U
#define INODE_SIZE        128U
#define TOTAL_BLOCKS      85U
#define JOURNAL_BLOCK_IDX   1U
#define JOURNAL_BLOCKS      16U
#define INODE_BMAP_IDX      17U
#define DATA_BMAP_IDX       18U
#define INODE_START_IDX     19U
#define DATA_START_IDX      21U
#define JOURNAL_MAGIC     0x4A524E4C
#define REC_DATA          1
#define REC_COMMIT        2


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

#define NAME_LEN 28
struct dirent {
    uint32_t inode;
    char name[NAME_LEN];
};


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


// --- Helper Functions ---
// journal.c code
void init_journal(int fd) {
    struct journal_header jh;
    off_t off = (off_t)JOURNAL_BLOCK_IDX * BLOCK_SIZE;

    // Read existing header
    if (pread(fd, &jh, sizeof(jh), off) != sizeof(jh)) return;

    // If magic doesn't match, initialize it
    if (jh.magic != JOURNAL_MAGIC) {
        jh.magic = JOURNAL_MAGIC;
        jh.nbytes_used = sizeof(jh);
        pwrite(fd, &jh, sizeof(jh), off);
    }
}

void journal_append(int fd, uint16_t type, uint32_t block_no, const void *data) {
    struct journal_header jh;
    off_t base = (off_t)JOURNAL_BLOCK_IDX * BLOCK_SIZE;

    // Read current journal state
    pread(fd, &jh, sizeof(jh), base);

    // Calculate size required for this new record
    uint16_t size = (type == REC_DATA) 
        ? sizeof(struct data_record) 
        : sizeof(struct commit_record);

    // Check if journal is full
    if (jh.nbytes_used + size > JOURNAL_BLOCKS * BLOCK_SIZE) {
        fprintf(stderr, "Journal full. Please run 'install' to clear it.\n");
        exit(1);
    }

    off_t off = base + jh.nbytes_used;

    if (type == REC_DATA) {
        struct data_record dr;
        dr.hdr.type = REC_DATA;
        dr.hdr.size = size;
        dr.block_no = block_no;
        memcpy(dr.data, data, BLOCK_SIZE);
        pwrite(fd, &dr, size, off);
    } else {
        struct commit_record cr;
        cr.hdr.type = REC_COMMIT;
        cr.hdr.size = size;
        pwrite(fd, &cr, size, off);
    }

    // Update global header
    jh.nbytes_used += size;
    pwrite(fd, &jh, sizeof(jh), base);
}

// --- Main Operations ---

void create_file(int fd, const char *name) {
    struct superblock sb;
    pread(fd, &sb, sizeof(sb), 0);

    // 1. Read Inode Bitmap & Find Free Inode
    uint8_t inode_bmap[BLOCK_SIZE];
    // Use the superblock's pointer
    pread(fd, inode_bmap, BLOCK_SIZE, (off_t)sb.inode_bitmap * BLOCK_SIZE);

    int inum = -1;
    for (uint32_t i = 1; i < sb.inode_count; i++) {
        if (!(inode_bmap[i / 8] & (1 << (i % 8)))) {
            inum = i;
            inode_bmap[i / 8] |= (1 << (i % 8)); 
            break;
        }
    }

    if (inum < 0) {
        printf("No free inodes\n");
        return;
    }

    // 2. Read Inode Block
    // Inode 0 (Root) and the new inode are in the same block.
    uint32_t inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
    uint32_t inode_block_idx = sb.inode_start + (inum / inodes_per_block);
    
    uint8_t inode_block[BLOCK_SIZE];
    pread(fd, inode_block, BLOCK_SIZE, (off_t)inode_block_idx * BLOCK_SIZE);

    struct inode *itable = (struct inode *)inode_block;
    struct inode *root_inode = &itable[0];      
    struct inode *new_inode = &itable[inum % inodes_per_block]; 

    // 3. Initialize New Inode
    memset(new_inode, 0, sizeof(struct inode));
    new_inode->type  = 1; // File
    new_inode->links = 1; 
    new_inode->size  = 0;
    new_inode->ctime = new_inode->mtime = time(NULL);

    // 4. Update Root Inode Size
    root_inode->size += sizeof(struct dirent);
    
    // 5. Update Directory Data Block
    uint32_t dir_block_no = root_inode->direct[0];
    uint8_t dir_block[BLOCK_SIZE];
    pread(fd, dir_block, BLOCK_SIZE, (off_t)dir_block_no * BLOCK_SIZE);

    struct dirent *entries = (struct dirent *)dir_block;
    int found = 0;
    int max_entries = BLOCK_SIZE / sizeof(struct dirent);

    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == '\0') {
            entries[i].inode = inum;
            strncpy(entries[i].name, name, NAME_LEN - 1);
            entries[i].name[NAME_LEN - 1] = '\0';
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("Directory full\n");
        return;
    }

    // 6. Journal Everything
    journal_append(fd, REC_DATA, sb.inode_bitmap, inode_bmap);
    journal_append(fd, REC_DATA, inode_block_idx, inode_block);
    journal_append(fd, REC_DATA, dir_block_no, dir_block);
    journal_append(fd, REC_COMMIT, 0, NULL);

    printf("File '%s' journaled (inode %d).\n", name, inum);
}

// Returns 0 on success, -1 on failure
int install_journal(int fd) {
    off_t base = (off_t)JOURNAL_BLOCK_IDX * BLOCK_SIZE;
    struct journal_header jh;

    if (pread(fd, &jh, sizeof(jh), base) != sizeof(jh)) return -1;
    
    // fail if journal does not exist
    if (jh.magic != JOURNAL_MAGIC) {
        fprintf(stderr, "No valid journal found.\n");
        return -1; 
    }
    
    if (jh.nbytes_used == sizeof(jh)) {
        // Empty journal is valid, just nothing to do
        return 0;
    }

    off_t off = base + sizeof(jh);

    // Buffer for atomic commit simulation
    struct {
        uint32_t block;
        uint8_t data[BLOCK_SIZE];
    } buf[32];

    int n = 0;

    // Replay Journal
    while (off < base + jh.nbytes_used) {
        struct rec_header rh;
        pread(fd, &rh, sizeof(rh), off);

        if (rh.type == REC_DATA) {
            
            if (n >= 32) {
                fprintf(stderr, "Journal install buffer overflow.\n");
                return -1;
            }
            struct data_record dr;
            pread(fd, &dr, sizeof(dr), off);
            buf[n].block = dr.block_no;
            memcpy(buf[n].data, dr.data, BLOCK_SIZE);
            n++;
            off += rh.size;
        } else if (rh.type == REC_COMMIT) {
            // Apply buffered blocks to disk only on COMMIT
            for (int i = 0; i < n; i++) {
                pwrite(fd, buf[i].data, BLOCK_SIZE, (off_t)buf[i].block * BLOCK_SIZE);
            }
            n = 0;
            off += rh.size;
        } else {
            break;
        }
    }

    // Clear Journal
    jh.nbytes_used = sizeof(jh);
    pwrite(fd, &jh, sizeof(jh), base);
    
    printf("Journal installed successfully.\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;

    int fd = open("vsfs.img", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    int result = 0;
    if (argc == 3 && strcmp(argv[1], "create") == 0) {
        init_journal(fd);
        create_file(fd, argv[2]);
    } else if (argc == 2 && strcmp(argv[1], "install") == 0) {
        result = install_journal(fd);
    } else {
        fprintf(stderr, "Usage: %s <create|install> [filename]\n", argv[0]);
        result = 1;
    }

    close(fd);
    return result == 0 ? 0 : 1;
}
