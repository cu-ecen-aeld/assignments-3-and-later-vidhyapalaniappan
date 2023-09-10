##Makefile

# Defining the compiler to use, which can be set externally using CROSS_COMPILE.
CC=$(CROSS_COMPILE)gcc

# Defining compilation flags. -Wall: enable all warning messages. -Werror: treat warnings as errors.
CFLAGS= -Wall -Werror

# Defining the target executable file name.
TARGET = writer 

# Rule to build the 'writer' executable. It depends on 'writer.o'.
writer: writer.o
	$(CC) $(CFLAGS) -o writer writer.o

# Rule to remove object files and the 'writer' executable	
clean:
	rm -f *.o $(TARGET)

.PHONY: clean
