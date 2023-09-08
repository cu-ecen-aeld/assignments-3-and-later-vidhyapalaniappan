CC=$(CROSS_COMPILE)gcc
CFLAGS=-I -g -Wall -Werror

all: writer

writer: writer.o
	$(CC) -o writer writer.o
	
.PHONY: clean
clean:
	rm -f writer *.o
