CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
CC=gcc

BINS= simplefs_test simplefs

HEADERS=bitmap.h\
	disk_driver.h\
	simplefs.h\
	shell.h

OBJS=simplefs_test.o simplefs.o 

%.o: 	%.c $(HEADERS)
		$(CC) -c -o $@ $<


.phony:	clean all

all: $(BINS)

simplefs_test: $(OBJS) 
			$(CC) -o $@ $^

simplefs: bitmap.o disk_driver.o simplefs.o shell.o 
				$(CC) -o simplefs bitmap.o disk_driver.o simplefs.o shell.o 

clean:
	rm -rf *.o *~  fs test1.txt simplefs_test simplefs
