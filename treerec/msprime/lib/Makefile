CC=h5cc 
EXTRA_CFLAGS=-std=c99 -pedantic -Werror -Wall -W \
  -Wno-unused-parameter\
  -Wmissing-prototypes -Wstrict-prototypes \
  -Wconversion -Wshadow -Wpointer-arith \
  -Wcast-align -Wcast-qual \
  -Wwrite-strings -Wnested-externs \
  -fshort-enums -fno-common -Dinline= 
CFLAGS=-g -O2 -DH5_NO_DEPRECATED_SYMBOLS
LDFLAGS=-lgsl -lgslcblas -lhdf5 -lm

HEADERS=msprime.h util.h
COMPILED=msprime.o fenwick.o tree_sequence.o object_heap.o newick.o \
    hapgen.o recomb_map.o mutgen.o vargen.o vcf.o ld.o avl.o tables.o util.o

all: main simulation_tests tests table_example table_example_cpp

# We need a seperate rule for avl.c as it won't pass the strict checks.
avl.o: avl.c
	${CC} -Wall -g -O2 -c avl.c

argtable3.o: argtable3.c
	${CC} -Wall -g -O2 -c argtable3.c

%.o : %.c ${HEADERS}
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

main: CFLAGS+=${EXTRA_CFLAGS}
main: main.c ${COMPILED} ${HEADERS} argtable3.o
	${CC} ${CFLAGS} ${EXTRA_CFLAGS} -o main main.c ${COMPILED} argtable3.o ${LDFLAGS} -lconfig 

tests: tests.c ${COMPILED} ${HEADERS}
	${CC} ${CFLAGS} -Wall -o tests tests.c ${COMPILED} ${LDFLAGS} -lcunit 

simulation_tests: simulation_tests.c ${COMPILED} ${HEADERS}
	${CC} ${CFLAGS} -Wall -o simulation_tests simulation_tests.c ${COMPILED} ${LDFLAGS} -lcunit 

tags:
	etags *

clean:
	rm -f main tests simulation_tests table_example table_example_cpp *.o *.gcda *.gcno

debian-tests: CC=h5cc
debian-tests: CFLAGS=-DH5_NO_DEPRECATED_SYMBOLS --coverage
debian-tests: tests simulation_tests

ubuntu-tests: CC=gcc
ubuntu-tests: CFLAGS=-DH5_NO_DEPRECATED_SYMBOLS --coverage 
ubuntu-tests: tests simulation_tests

coverity-tests: CC=gcc 
coverity-tests: CFLAGS+=-I/usr/include/hdf5/serial
coverity-tests: LDFLAGS+=-L/usr/lib/x86_64-linux-gnu/hdf5/serial
coverity-tests: tests main simulation_tests

coverity: 
	# We must use CC=gcc for this target or coverity won't 
	# work properly.
	make clean
	/home/jk/admin/software/cov-analysis-linux64-8.5.0/bin/cov-build --dir cov-int make coverity-tests
	tar -zcvf cov-int.tar.gz cov-int

# Examples using just the tables API.
table_example: table_example.c
	h5cc -o table_example table_example.c tables.c object_heap.c avl.c util.c

# Make sure that all the files are compiled as C code.
table_example_cpp: table_example_cpp.cc tables.o object_heap.o util.o
	h5c++ -std=c++11 -o table_example_cpp table_example_cpp.cc tables.o object_heap.o avl.o util.o
