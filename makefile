CC = gcc
CFLAGS = -Wall -Wextra -g

all: mkfs validator journal

mkfs: mkfs.c
	$(CC) $(CFLAGS) -o mkfs mkfs.c

validator: validator.c
	$(CC) $(CFLAGS) -o validator validator.c

journal: journal.c vsfs.h
	$(CC) $(CFLAGS) -o journal journal.c

clean:
	rm -f mkfs validator journal vsfs.img