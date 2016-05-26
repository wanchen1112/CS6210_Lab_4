#### RVM Library Makefile

CFLAGS  = -Wall -g -I.
LFLAGS  =
CC      = gcc
RM      = /bin/rm -rf
AR      = ar rc
RANLIB  = ranlib

LIBRARY = librvm.a

LIB_SRC = rvm.c

LIB_OBJ = $(patsubst %.c,%.o,$(LIB_SRC))

%.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

$(LIBRARY): $(LIB_OBJ)
	$(AR) $(LIBRARY) $(LIB_OBJ)
	$(RANLIB) $(LIBRARY)

clean:
	$(RM) $(LIBRARY) $(LIB_OBJ) *.o rvm_main abort basic multi multiabort truncate
	rm -rf rvm_segments

rvm_main: librvm.a rvm_main.o
abort: librvm.a abort.o
basic: librvm.a basic.o
multi: librvm.a multi.o
multiabort: librvm.a multi-abort.o	

truncate: librvm.a truncate.o
