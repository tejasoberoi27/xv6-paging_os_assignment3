#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

int
main(int argc, char *argv[]){
	int block_allocated = balloc_page(1);
	printf(1, "block_allocated number: %d\n",block_allocated);
	return 0;
}
