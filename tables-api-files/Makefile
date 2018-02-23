# Need to use h5cc here to simplify finding the includes and library
# locations. Can specify -I/-L values by hand if you wish, but this 
# is easier.
CC=h5cc
CXX=h5c++

all: table_example_cpp table_example

# Make sure that all the files are compiled as C code.
%.o : %.c ${HEADERS}
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

table_example: table_example.c
	$(CC) -o table_example table_example.c tables.c object_heap.c avl.c util.c

table_example_cpp: table_example_cpp.cc tables.o object_heap.o util.o avl.o
	$(CXX) -o table_example_cpp table_example_cpp.cc tables.o object_heap.o avl.o util.o

clean: 
	rm -f *.o table_example_cpp table_example
