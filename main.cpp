#include "shttpd.h"
#include <string.h>
#include <stdio.h>


int main(int argc,char **argv)
{
	Configure conf;
	conf.Para_FileParse();
	conf.print();
	Worker this_work;
	this_work.Run();
	while(1);
}