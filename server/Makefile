##Makefile

# Defining the compiler to use, which can be set externally using CROSS_COMPILE.
CC ?= $(CROSS_COMPILE)gcc

# Defining compilation flags. -Wall: enable all warning messages. -Werror: treat warnings as errors.
CFLAGS ?= -Wall -Werror -g 
LDFLAG ?= -pthread -lrt 

# Defining the target executable file name.
TARGET = aesdsocket 

all: $(TARGET)
default: $(TARGET)

# Rule to build the 'writer' executable. It depends on 'writer.o'.
aesdsocket: aesdsocket.o
	$(CC) $(CFLAGS) $(LDFLAG) -o aesdsocket aesdsocket.o

# Rule to remove object files and the 'writer' executable	
clean:
	rm -f *.o $(TARGET)

.PHONY: clean
