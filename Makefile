CC=g++

sample: sample.c threads.o
	$(CC) -m32 -o sample sample.c threads.o
threads.o: threads.cpp
	$(CC) -m32 -c threads.cpp
clean:
	\rm sample
	\rm threads.o

