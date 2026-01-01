#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "vsfs.h"

// Member 1 Functions (Journal Management)
void init_journal(int fd){
    (void)fd;
}

// Member 2 Functions (FS Logic)
void create_file(int fd, const char *name){
    (void)fd; (void)name;
    printf("Create command called for: %s\n", name);
}
// Member 3 Functions (Recovery/Install)
void install_journal(int fd){
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
        create_file(fd, argv[2]); // 
    } else if (strcmp(argv[1], "install") == 0) {
        install_journal(fd); // 
    } else {
        printf("Invalid command.\n");
    }

    close(fd);
    return 0;
}