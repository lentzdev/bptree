/*
	B+Tree Indexing routines
	Design & COPYRIGHT (C) 1990,91 by A.G. Lentz & LENTZ SOFTWARE-DEVELOPMENT

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <string.h>
#include "bpindex.h"


void main()
{
	FILE_CACHE	cache(10);
	BP_FILE 	idx_file;
	BP_INDEX	idx;
	BP_ENTRY	entry;
	BP_RES		res;
	char		buf[128];
	int		i;

//	unlink("test.idx");
	if (!idx_file.open(&cache,"test.idx")) {
	   printf("! Can't create/open index file\n");
	   exit (1);
	}
	if (!idx.init(&idx_file,"naam",1,BP_NONE)) {
	   printf("! Can't init index 'name' within file BPTEST.IDX\n");
	   exit (1);
	}

	printf("Adding 100 indexes: ");
	for (i=0; i<100; i++) {
	    printf("%-3d\b\b\b",i);
	    sprintf(buf, "item%04d", i);
	    strcpy(entry.key[0],buf);
	    entry.datarec = i;
	    if (idx.add(&entry) != BP_OK) {
	       printf("\n! Can't add %s to index\n", buf);
//	       exit (2);
	    }
	}

	printf("\nRe-reading indexes, first to last: ");
	i  = 0;
	res = idx.first(&entry);
	while (res==BP_OK) {
	       printf("%-3d\b\b\b",i);
//	       printf("\n%ld <--> %d", entry.datarec, i);
	       if (entry.datarec!=i) {
		   printf("\nWrong number returned: %ld vrs %d\n", entry.datarec, i);
		   exit (1);
	       }
	       res = idx.next(&entry);
	       i++;
	}
	if (i!=100) {
	   printf("\nUnexpected end of index\n");
	   exit (1);
	}

	printf("\nRe-Reading index, last to first: ");
	i  = 99;
	res = idx.last(&entry);
	while (res==BP_OK) {
	       printf("%-3d\b\b\b",i);
//	       printf("\n%ld <--> %d", entry.datarec, i);
	       if (entry.datarec!=i) {
		  printf("\nWrong number returned: %ld vrs %d\n", entry.datarec, i);
		  exit(1);
	       }
	       res = idx.prev(&entry);
	       i--;
	}
	if (i != -1) {
	   printf("\nUnexpected end of index\n");
	   exit(1);
	}

	printf("\nDoing find: ");
	for (i = 0; i < 100; i++) {
	    printf("%-3d\b\b\b",i);
	    sprintf(buf, "item%04d", i);
	    strcpy(entry.key[0],buf);
	    entry.datarec = i;
	    if (idx.find(&entry) != BP_OK) {
	       printf("\n! Can't find %s in index\n", buf);
//	       exit (1);
	    }
//	    printf("\n%ld <--> %d", entry.datarec, i);
	    if (entry.datarec != i) {
		printf("\nWrong number returned: %ld vrs %d\n", entry.datarec, i);
		exit (1);
	    }
	}

	printf("\nDone.\n");
	idx_file.close();
}


/* end of test.cpp */
