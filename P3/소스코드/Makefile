CC = gcc

TARGET = ssu_sfinder

ssu_sfinder : linkedlist.o dirQueue.o ssu_sfinder.o
	$(CC) -o $@ $^ -lcrypto -lpthread

ssu_sfinder.o : ssu_sfinder.c
	$(CC) -c $^

linkedlist.o : linkedlist.c
	$(CC) -c $^

dirQueue.o : dirQueue.c
	$(CC) -c $^

clean :
	rm -rf *.o
	rm -rf $(TARGET)
