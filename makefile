OBJ = src/main.c \
		src/mc_listener.c \
		src/mc_caster.c \
		src/proxy.c

#	Any special libraries
LIBS = -pthread -g -Wall

#	Set any compiler flags
#CFLAGS = -lrt

CC = gcc

#	Set the filename extension
EXTENSION = .c

#define a rule that applies to all files ending in the .o suffix, which says that the .o file depends upon the .c 
#version of the file and all the .h files included in the DEPS macro.  Compile each object file
%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

#Combine them into the output file
#Set your desired exe output file name here
mc-remotelan: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

#Cleanup
.PHONY: clean

clean:
	rm -f *.o *~ core *~ 