#include <stdio.h>

int main(argc,argv)
	int argc;
	char **argv;
	{
	FILE *in;
	int c;
	int x;

	if(argc!=2)
		{
		printf("need input arg\n");
		return 0;
		}

	in = fopen(argv[1],"r");
	if(!in)
		{
		printf("cannot open input file\n");
		return 0;
		}

	while(1)
		{
		c = fgetc(in);
		while(c!=EOF && !('0'<=c&&c<='9' || 'A'<=c&&c<='F' || 'a'<=c&&c<='f'))c = fgetc(in);
		if(c==EOF)break;

		if('0'<=c&&c<='9')x = c-'0';
		else if('A'<=c&&c<='F')x = c-'A'+10;
		else if('a'<=c&&c<='f')x = c-'a'+10;
		else
			{
			fprintf(stderr,"non hex digit\n");
			return 0;
			}

		c = fgetc(in);
		if(c==EOF)
			{
			fprintf(stderr,"unexpected EOF\n");
			return 0;
			}

		if('0'<=c&&c<='9')x = x<<4 | c-'0';
		else if('A'<=c&&c<='F')x = x<<4 | c-'A'+10;
		else if('a'<=c&&c<='f')x = x<<4 | c-'a'+10;
		else
			{
			fprintf(stderr,"non hex digit\n");
			return 0;
			}
		fputc(x,stdout);
		}

	fclose(in);

	return 0;
	}
