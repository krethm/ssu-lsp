
CC = gcc

TARGET = ssu_sdup ssu_find-md5 ssu_find-sha1 ssu_help
all : $(TARGET)

ssu_sdup : ssu_sdup.o
	$(CC) -o $@ $^

ssu_find-md5 : linkedlist.o dirQueue.o ssu_find-md5.o
	$(CC) -o $@ $^ -lcrypto

ssu_find-sha1 : linkedlist.o dirQueue.o ssu_find-sha1.o
	$(CC) -o $@ $^ -lcrypto

ssu_help : ssu_help.o
	$(CC) -o $@ $^

linkedlist.o : linkedlist.c
	$(CC) -c $^

dirQueue.o : dirQueue.c
	$(CC) -c $^

ssu_sdup.o : ssu_sdup.c
	$(CC) -c $^

ssu_find-md5.o : ssu_find-md5.c
	$(CC) -c $^ -lcrypto

ssu_find-sha1.o : ssu_find-sha1.c
	$(CC) -c $^ -lcrypto

ssu_help.o : ssu_help.c
	$(CC) -c $^

clean :
	rm -f *.o
	rm $(TARGET)
