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
#include <conio.h>
#include <process.h>
#include <string.h>
#include <ctype.h>
#include "bpindex.h"
#include "dbtext.h"


static char	  textbuf[1025];
static FILE_CACHE idxcache(10), datcache(10);
static DB_TEXT	  dat;
static DB_RECORD  rec;
static BP_FILE	  idx_file;
static BP_INDEX   idx;
static BP_ENTRY   entry;


main(void)
{
	BP_RES	res;
	char	buffer[128];
	int	c;
	char   *p;

	rec.set_text(textbuf);
	if (!dat.open(&datcache,"bptest.dat")) {
	   printf("! Can't open/create data file or different format version\n");
	   exit (1);
	}
	if (!idx_file.open(&idxcache,"bptest.idx")) {
	   printf("! Can't open/create index file or different format version\n");
	   exit (1);
	}
	if (!idx.init(&idx_file,"naam",1,BP_NOCASE)) {
	   printf("! Can't init index 'naam' within file BPTEST.IDX\n");
	   exit (1);
	}

	printf("= Operating on BPTEST.DAT/BPTEST.IDX\n");

	do {
printf("> A)dd, D)elete, S)earch, F)irst, L)ast, N)ext, P)revious, !)List, Q)uit\n");
	      c = toupper(getch());
	      switch (c) {
		     case 'A':
			  printf("* Add: "); gets(buffer);
			  strcpy(entry.key[0],buffer);
			  entry.key[0][BP_MAXKEYSIZE - 1] = '\0';
			  if (idx.find(&entry) == BP_OK)
			     printf("- Already in index\n");
			  else {
			     rec.clear();
			     rec.set(buffer,"naam");
			     printf("Comment: "); gets(buffer);
			     rec.set(buffer,"comment");
			     entry.datarec = dat.put(&rec);
			     idx.add(&entry);
			     printf("+ Added entry (datastart %ld)\n",
				    entry.datarec);
			  }
			  break;

		     case 'D':
			  printf("* Delete: "); gets(buffer);
			  strcpy(entry.key[0],buffer);
			  entry.key[0][BP_MAXKEYSIZE - 1] = '\0';
			  if (idx.find(&entry) != BP_OK)
			     printf("- Entry not found\n");
			  else {
			     dat.del(entry.datarec);
			     idx.del(&entry);
			     printf("+ Deleted entry\n");
			  }
			  break;

		     case 'S':
			  printf("* Search: "); gets(buffer);
			  strcpy(entry.key[0],buffer);
			  entry.key[0][BP_MAXKEYSIZE - 1] = '\0';
			  if (idx.find(&entry) == BP_OK) {
			     printf("+ Found entry (%s)\n",entry.key[0]);
			     dat.get(&rec,entry.datarec);
			     for (p = rec.first(); p; p = rec.next())
				 printf("  %s: %s\n", p, rec.get());
			  }
			  else
			     printf("- Entry not found\n");
			  break;

		     case 'F':
			  printf("* First\n");
			  if (idx.first(&entry) == BP_OK) {
			     printf("+ Found entry (%s)\n",entry.key[0]);
			     dat.get(&rec,entry.datarec);
			     for (p = rec.first(); p; p = rec.next())
				 printf("  %s: %s\n", p, rec.get());
			  }
			  else
			     printf("- Index empty\n");
			  break;

		     case 'L':
			  printf("* Last\n");
			  if (idx.last(&entry) == BP_OK) {
			     printf("+ Found entry (%s)\n",entry.key[0]);
			     dat.get(&rec,entry.datarec);
			     for (p = rec.first(); p; p = rec.next())
				 printf("  %s: %s\n", p, rec.get());
			  }
			  else
			     printf("- Index empty\n");
			  break;

		     case 'N':
			  printf("* Next\n");
			  if (idx.next(&entry) == BP_OK) {
			     printf("+ Found entry (%s)\n",entry.key[0]);
			     dat.get(&rec,entry.datarec);
			     for (p = rec.first(); p; p = rec.next())
				 printf("  %s: %s\n", p, rec.get());
			  }
			  else
			     printf("- No more entries in this direction\n");
			  break;

		     case 'P':
			  printf("* Previous\n");
			  if (idx.prev(&entry) == BP_OK) {
			     printf("+ Found entry (%s)\n",entry.key[0]);
			     dat.get(&rec,entry.datarec);
			     for (p = rec.first(); p; p = rec.next())
				 printf("  %s: %s\n", p, rec.get());
			  }
			  else
			     printf("- No more entries in this direction\n");
			  break;

		     case 'Q':
			  printf("* Quit\n");
			  break;

		     case '!':
			  printf("* List all from first to last\n");
			  res = idx.first(&entry);
			  while (res == BP_OK) {
				printf("= %s\n",entry.key[0]);
				dat.get(&rec,entry.datarec);
				for (p = rec.first(); p; p = rec.next())
				    printf("  %s: %s\n", p, rec.get());
				res = idx.next(&entry);
			  }
			  break;

		     default:
			  break;
	      }
	} while (c != 'Q');

	dat.close();
	idx_file.close();
}


/* end of bptest.cpp */
