#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "vsfs.h"

// Member 1 Functions (Journal Management)
void init_journal(int fd){
    struct journal_header jh;
    
    //journal block idx 1, each block is 4096bytes
    off_t journal_offset = (off_t)JOURNAL_BLOCK_IDX *BLOCK_SIZE;
    //read the existing header to see if already initialized
    if (pread(fd, &jh, sizeof(struct journal_header), journal_offset)!= sizeof(struct journal_header)){
        perror ("failed to read journal header");
        return;
    }
    //if the number doesnt match, then initializing it
    if (jh.magic != JOURNAL_MAGIC){
        printf("initializing new journal header... ... ...\n")
        jh.magic = JOURNAL_MAGIC;
        jh.nbytes_used = sizeof(struct journal_header); //empty journal means nbytes used == size of journal header

        if(pwrite(fd, &jh, sizeof(struct journal_header), journal_offset)!= sizeof(struct journal_header)){
            perror("failed to write journal header");
            exit(1);
        }
    }
}

void journal_append(int fd, unit16_t type, unit32_t block_no, const void *data){
    struct journal_header jh;
    off_t journal_start = (off_t)JOURNAL_BLOCK_IDX*BLOCK_SIZE;
    
    pread(fd, &jh, sizeof(jh), journal_start);

    unint16_t rec_size = (type == REC_DATA)? sizeof(struct data_record): sizeof(struct commit_record);

    if (jh.nbytes_used +rec_size > JOURNAL_BLOCK*BLOCK_SIZE){
        fprintf(stderr, "journal full, please run 'install' first\n");
        exit(1);
    }

    off_t write_offset = journal_start+jh.nbytes_used;
    if(type == REC_DATA){
        struct data_record dr;
        dr.hdr.type = REC_DATA;
        dr.hdr.size = rec_size;
        dr.block_no = block_no;
        memcpy(dr.data, data, BLOCK_SIZE);
        pwrite(fd, &dr, rec_size, write_offset);
    } else{
        struct commit_record cr;
        cr.hdr.type = REC_COMMIT;
        cr.hdr.size = rec_size;
        pwrite(fd, &cr, rec_size, write_offset);
    }

    jh.nbytes_used+=rec_size;
    pwrite(fd, &jh, sizeof(jh), journal_start);
}

// Member 2 Functions (FS Logic)
void create_file(int fd, const char *name){
    //TODO
    (void)fd; (void)name;
    printf("Create command called for: %s\n", name);
}
// Member 3 Functions (Recovery/Install)
void install_journal(int fd){
    //Todo
    (void)fd;
    printf("Install command called\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./journal <create|install> [name]\n");
        return 1;
    }

    int fd = open("vsfs.img", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    if (strcmp(argv[1], "create") == 0 && argc == 3) {
        init_journal(fd);          //it ensure header exists
        create_file(fd, argv[2]); // 
    } else if (strcmp(argv[1], "install") == 0) {
        install_journal(fd); 
    } else {
        printf("Invalid command.\n");
    }

    close(fd);
    return 0;
}