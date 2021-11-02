CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
CC=gcc

BINS= simplefs_test

HEADERS=bitmap.h\
	disk_driver.h\
	simplefs.h

OBJS=simplefs_test.o simplefs.o 

%.o: 	%.c $(HEADERS)
		$(CC) -c -o $@ $<


.phony:	clean all

simplefs_test: $(OBJS) 
			$(CC) -o $@ $^

clean:
	rm -rf *.o *~  fs test1.txt simplefs_test