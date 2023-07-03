#include <stdio.h>
#include <stdlib.h>
#include "bpindex.h"


RAW_FILE f;
word	 num_keys;
BP_LEVEL level;


void show_block (FILE_OFS file_ofs)
{
	char	       buf[30];
	BP_BLOCK      *block;
	BP_ENTOFS      cur = 0;
	BP_SYS_ENTRY  *ep;
	register int   i;
	register char *p;


	if (!(block = new BP_BLOCK)) {
	   f.close();
	   exit (1);
	}

	level++;
	sprintf(buf,"%d:                         ",level);
	buf[3 + (level - 1) * 3] = '\0';

	f.seek(file_ofs,f.seek_set);
	f.read(block,sizeof (BP_BLOCK));

	printf("%s---block(%ld)\n",buf,file_ofs);
	if (block->lower != OFS_NONE) {
	   printf("%slower---\n",buf);
	   show_block(block->lower);
	}

	cur = 0;
	while (cur < block->next) {
	      ep = (BP_SYS_ENTRY *) (block->entries + cur);
	      printf("%sentry(ofs=%d,datarec=%ld", buf, cur, ep->datarec);
	      p = (char *) (ep->key + 1);
	      for (i = 1; i <= num_keys; i++) {
		  printf(",key%d=%s",i,p);
		  p += (strlen(p) + 1);
	      }
	      printf(")\n");
	      if (ep->higher != OFS_NONE) {
		 printf("%shigher---\n",buf);
		 show_block(ep->higher);
	      }

	      cur += ((2 * sizeof (FILE_OFS)) + (sizeof (byte)) + (*ep->key));
	}

	level--;
}


void main (int argc, char *argv[])
{
	BP_HEADER header;
	register int i;

	if (argc != 3) {
	   printf("Usage: showtree <filename> <index>\n");
	   exit(1);
	}

	if (!f.open(argv[1], f.o_rdwr, f.sh_denynone)) {
	   printf("Can't open file '%s'\n",argv[1]);
	   exit (1);
	}

	f.read(&header,sizeof (BP_HEADER));
	if (header.version != BP_VERSION) {
	   printf("Index file '%s' has different format version!\n",argv[1]);
	   f.close();
	   exit (1);
	}

	for (i = 0; i < BP_MAXINDEX; i++) {
	    if (header.index[i].root != OFS_NONE &&
		!stricmp(header.index[i].nametag,argv[2]))
	       break;
	}
	if (i == BP_MAXINDEX) {
	   printf("Can't find index '%s' in file '%s'\n",argv[2],argv[1]);
	   f.close();
	   exit (1);
	}

	printf("---Information on index '%s' in file '%s'\n",argv[2],argv[1]);
	printf("   Duplicate entries allowed....: %s\n",
	       (header.index[i].flags & BP_DUPKEYS) ? "Yes" : "No");
	printf("   Case insensitive key compare.: %s\n",
	       (header.index[i].flags & BP_NOCASE) ? "Yes" : "No");
	printf("   Number of keys per entry.....: %u\n",
	       header.index[i].keys);
	printf("---Showing tree structure, start at root (%ld)\n",
	       header.index[i].root);

	num_keys = header.index[i].keys;
	show_block(header.index[i].root);
	printf("---Finished\n");

	f.close();
}


/* end of showtree.cpp */
