#define _FILE_OFFSET_BITS 64
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
int main(int argc, char **argv) {
	unsigned x;
	struct stat st;
	open(*argv,O_RDONLY);
	fstat(0,&st);
	read(0,&x,sizeof(x));
	write(1,"null",4);
}
