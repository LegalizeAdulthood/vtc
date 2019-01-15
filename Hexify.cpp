#include <stdio.h>

int main(int argc,char **argv)
	{
	FILE *in;
	int i;
	int c;

	if(argc!=2)
		{
		printf("need name of binary file as first arg\n");
		return 0;
		}

	in = fopen(argv[1],"rb");
	if(!in)
		{
		printf("cannot open input file\n");
		return 0;
		}

	i=0;
	while((c=getc(in))!=EOF)
		{
		printf("%02x",c&255);
		if(++i>=16)
			{
			printf("\n");
			i=0;
			}
		}

	if(i)printf("\n");

	fclose(in);

	return 0;
	}
