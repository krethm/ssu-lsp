#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	puts("Usage:");
	puts(" > fmd5/fsha1 [FILE_EXTENSION] [MINSIZE] [MAXSIZE] [TARGET_DIRECTORY]");
	puts("    >> [SET_INDEX] [OPTION ... ]");
	puts("       [OPTION ... ]");
	puts("       d [LIST_IDX] : delete [LIST_IDX] file");
	puts("       i : ask for confirmation before delete");
	puts("       f : force delete except the recently modified file");
	puts("       t : force move to Trash except the recently modified file");
	puts("       e : delete duplicate file lists without deleting file");
	puts("       p : print the contents of the file");
	puts("       m : move the selected file to a different location");
	puts("    size : calculate duplication file");
	puts(" > help");
	puts(" > exit");
	puts("");
	exit(0);
}