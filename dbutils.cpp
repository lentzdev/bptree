/* ------------------------------------------------------------------------- */
/* Pigeon Post (WAITS)	-  Database & B+Tree Index Maintenance		     */
/* Design & COPYRIGHT (C) 1990,91 by A.G. Lentz & LENTZ SOFTWARE-DEVELOPMENT */
/* ------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <signal.h>
#include <ctype.h>
#include "bpindex.h"
#include "dbtext.h"


#define PRJNAME "Pigeon Post"
#define PRGNAME "DBUTILS"
#define VERSION "0.00.02"
enum { MAX_BUFFER = 255, MAX_ARGS = 255, MAX_BASE = 25, MAX_TEXT = 30 * 1024 };

static FILE_CACHE    datcache(5), idxcache(25);
static DB_TEXT	     dat, newdat;
static DB_RECORD     rec;
static DB_HEADER     dbheader;
static DB_BLOCK      dbblock;
static BP_FILE	     idx_file;
static BP_ENTRY      entry;
static BP_HEADER     bpheader;
static BP_BLOCK      bpblock;

struct _index {
	char	      *name;
	boolean        dupkeys;
	boolean        nocase;
	char	      *field[BP_MAXKEYFIELDS];
	byte	       num_fields;
	BP_INDEX      *idx;

	 _index (void) {
	       if (!(idx = new BP_INDEX)) exit (255);
	}
	~_index (void) {
		delete idx;
	}
};
struct _base {
	byte	       shared;
	char	      *path,
		      *file;
	boolean        counter;
	struct _index  index[BP_MAXINDEX];
	byte	       num_indexes;
};

static char	     buffer[MAX_BUFFER + 1];
static char	    *av[MAX_ARGS];
static struct _base  base[MAX_BASE];
static byte	     num_bases = 0;
static struct _base *work_base;
static RAW_FILE      table_file;
static long	     table_size;
static char	     octal[]	= "01234567";
static char	     lowerhex[] = "01234567abcdef";
static char	     upperhex[] = "01234567ABCDEF";
static char	    *text = new char[MAX_TEXT];


/* ------------------------------------------------------------------------- */
int parse (register byte *line, byte *argv[])	 // Parse line, return num args
{
	register word c;
	register byte *p;
	register char *q;
	register int  argc;
	boolean inarg	  = false,
		inquote   = false,
		quotenext = false;

	for (argc = 0; argc < MAX_ARGS; line++) {
again:	    if (!(c = *line) || strchr("\r\n\032",c)) break;
	    if (quotenext) {
	       quotenext = false;
	       switch (c) {
		      case 'a': *p++ = '\a'; break;
		      case 'b': *p++ = '\b'; break;
		      case 'f': *p++ = '\f'; break;
		      case 'n': *p++ = '\n'; break;
		      case 'r': *p++ = '\r'; break;
		      case 't': *p++ = '\t'; break;
		      case 'v': *p++ = '\v'; break;
		      case '0': for (c = 0; (q = strchr(octal,*++line)) != NULL;
				     c <<= 3, c |= (word) (q - octal));
				*p++ = c; goto again;		   // <<3 = *8
		      case 'x': for (c = 0; (q = strchr(lowerhex,*++line)) != NULL;
				     c <<= 4, c |= (word) (q - lowerhex));
				*p++ = c; goto again;		   // <<4 = *16
		      case 'X': for (c = 0; (q = strchr(upperhex,*++line)) != NULL;
				     c <<= 4, c |= (word) (q - upperhex));
				*p++ = c; goto again;		   // <<4 = *16
		      default:	*p++ = c; break;	// covers "'?\ as well
	       }
	       continue;
	    }

	    if (!inarg) {
	       if (c == ';') break;
	       if (c == ' ' || c == '\t') continue;
	       argv[argc] = p = line;
	       inarg = true;
	       if (c == '\"') { inquote = true; continue; }
	       if (c == '\\') { quotenext = true; continue; }
	       /* fallthrough for normal characters */
	    }

	    switch (c) {
		   case '\"': inquote = inquote ? false : true; break;
		   case '\\': quotenext = true; break;
		   case ' ':
		   case '\t': if (!inquote) {
				 *p = '\0';
				 argc++;
				 inarg = false;
				 break;
			      }
			      /* fallthrough from " \t" to default */
		   default:   *p++ = c; break;
	    }
	}

	if (inarg) {
	   *p = '\0';
	   argc++;
	}
	argv[argc] = NULL;

#if 0
{ int i;
printf("---\n");
for (i = 0; i < argc; i++)
    printf("%d = '%s'\n",i,argv[i]);
}
#endif
	return (argc);
}/*parse()*/


/* ------------------------------------------------------------------------- */
void splitpath (char *filepath, char *path, char *file)
{
	char *p,*q;

	for (p=filepath;*p;p++) ;
	while (p!=filepath && *p!=':' && *p!='\\' && *p!='/') --p;
	if (*p==':' || *p=='\\' || *p=='/') ++p;	/* begin     */
	q=filepath;
	while (q!=p) *path++=*q++;			/* copy path */
	*path='\0';
	strcpy(file,p);
}/*splitpath()*/


/* ------------------------------------------------------------------------- */
static char ff_dta[58];

char *ffirst (char *filespec)
{
	struct SREGS sregs;
	union  REGS  regs;

#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
	sregs.ds  = FP_SEG(ff_dta);
	regs.x.dx = FP_OFF(ff_dta);
#else
	sregs.ds  = _DS;
	regs.x.dx = (word) ff_dta;
#endif
	regs.h.ah = 0x1a;
	intdosx(&regs,&regs,&sregs);

	regs.x.cx = 0;
#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
	sregs.ds  = FP_SEG(filespec);
	regs.x.dx = FP_OFF(filespec);
#else
	sregs.ds  = _DS;
	regs.x.dx = (word) filespec;
#endif
	regs.h.ah = 0x4e;
	intdosx(&regs,&regs,&sregs);
	if (regs.x.cflag)
	   return (NULL);
	return (ff_dta + 0x1e);
}/*ffirst()*/


char *fnext (void)
{
	struct SREGS sregs;
	union  REGS  regs;

#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
	sregs.ds  = FP_SEG(ff_dta);
	regs.x.dx = FP_OFF(ff_dta);
#else
	sregs.ds  = _DS;
	regs.x.dx = (word) ff_dta;
#endif
	regs.h.ah = 0x1a;
	intdosx(&regs,&regs,&sregs);

	regs.h.ah = 0x4f;
	intdosx(&regs,&regs,&sregs);
	if (regs.x.cflag)
	   return (NULL);
	return (ff_dta + 0x1e);
}/*fnext()*/


/* ------------------------------------------------------------------------- */
void unique_name (char *pathname)
{
	static char *suffix = ".000";
	register char *p;
	register int n;

	if (ffirst(pathname)) {
	   p = pathname;
	   while (*p && *p!='.') p++;
	   for (n=0; n<4; n++)
	       if (!*p) {
		  *p	 = suffix[n];
		  *(++p) = '\0';
	       }
	       else p++;
	   
	   while (ffirst(pathname)) {
		 p = pathname + strlen(pathname) - 1;
		 if (!isdigit(*p)) *p = '0';
		 else {
		    for (n=3; n--;) {
			if (!isdigit(*p)) *p = '0';
			if (++(*p) <= '9') break;
			else		   *p-- = '0';
		    } /* for */
		 }
	   } /* while */
	} /* if (exist) */
}/*unique_name()*/


/* ------------------------------------------------------------------------- */
void read_conf(void)
{
	FILE *fp;
	char path[80], file[20];
	register byte i;

	if (!(fp = fopen(PRGNAME".CTL","rt"))) {
	   fprintf(stderr,"! Can't open "PRGNAME".CTL\n");
	   exit (1);
	}

	while (fgets(buffer,MAX_BUFFER,fp)) {
	      i = parse((byte *) buffer, (byte **) av);
	      if (!i) continue;
	      if (i < 3) {
		 fprintf(stderr,"! Too few parms on line\n");
		 exit (1);
	      }

	      if (!stricmp(av[0],"base")) {
		 if (i != 4) {
		    fprintf(stderr,"! Invalid no. parms on base line\n");
		    exit (1);
		 }
		 if (num_bases && !base[num_bases - 1].num_indexes) {
		    fprintf(stderr,"! Base must have at least one index\n");
		    exit (1);
		 }
		 if (num_bases == MAX_BASE) {
		    fprintf(stderr,"! Too many bases\n");
		    exit (1);
		 }

		 splitpath(av[1],path,file);
		 if (!(base[num_bases].path = strdup(path)) ||
		     !(base[num_bases].file = strdup(file))) {
		    fprintf(stderr,"! Can't allocate memory\n");
		    exit (1);
		 }

		 if	 (!stricmp(av[2],"counter"))   base[num_bases].counter = true;
		 else if (!stricmp(av[2],"nocounter")) base[num_bases].counter = false;
		 else {
		    fprintf(stderr,"! Base option '%s' instead of NoCounter/Counter\n",av[2]);
		    exit (1);
		 }

		 if	 (!stricmp(av[3],"shared"))    base[num_bases].shared = true;
		 else if (!stricmp(av[3],"exclusive")) base[num_bases].shared = false;
		 else {
		    fprintf(stderr,"! Unknown base type '%s'\n",av[3]);
		    exit (1);
		 }

		 base[num_bases].num_indexes = 0;
		 num_bases++;
	      }

	      else if (!stricmp(av[0],"index")) {
		 if (i < 5) {
		    fprintf(stderr,"! Too few parms on index line\n");
		    exit (1);
		 }
		 if (base[num_bases - 1].num_indexes == BP_MAXINDEX) {
		    fprintf(stderr,"! Too many indexes for base\n");
		    exit (1);
		 }

		 if (strlen(av[1]) > 13) {
		    fprintf(stderr,"! Index name longer than 13 chars\n");
		    exit (1);
		 }
		 if (!(base[num_bases - 1].index[base[num_bases - 1].num_indexes].name = strdup(av[1]))) {
		    fprintf(stderr,"! Can't allocate memory\n");
		    exit (1);
		 }

		 if	 (!stricmp(av[2],"DupKeys")) base[num_bases - 1].index[base[num_bases - 1].num_indexes].dupkeys = true;
		 else if (!stricmp(av[2],"Unique"))  base[num_bases - 1].index[base[num_bases - 1].num_indexes].dupkeys = false;
		 else {
		    fprintf(stderr,"! Index option '%s' instead of Unique/DupKeys\n",av[2]);
		    exit (1);
		 }

		 if	 (!stricmp(av[3],"NoCase")) base[num_bases - 1].index[base[num_bases - 1].num_indexes].nocase = true;
		 else if (!stricmp(av[3],"Case"))   base[num_bases - 1].index[base[num_bases - 1].num_indexes].nocase = false;
		 else {
		    fprintf(stderr,"! Index option '%s' instead of Case/NoCase\n",av[3]);
		    exit (1);
		 }

		 for (i = 0; i < BP_MAXKEYFIELDS && av[4 + i]; i++) {
		     if (!(base[num_bases - 1].index[base[num_bases - 1].num_indexes].field[i] = strdup(av[4 + i]))) {
			fprintf(stderr,"! Can't allocate memory\n");
			exit (1);
		     }
		 }
		 if (av[4 + i]) {
		    fprintf(stderr,"! Too many fields for index\n");
		    exit (1);
		 }

		 base[num_bases - 1].index[base[num_bases - 1].num_indexes].num_fields = i;
		 base[num_bases - 1].num_indexes++;
	      }

	      else {
		 fprintf(stderr,"! Unknown control option '%s'\n",av[0]);
		 exit (1);
	      }
	}

	fclose (fp);

#if 0
	printf("--- Control info read from "PRGNAME".CTL\n");
	for (i = 0; i < num_bases; i++) {			   /* Bases  */
	    printf("Base  %s%-8s %s\n",
		   base[i].path, base[i].file,
		   base[i].shared ? "Shared" : "Exclusive");
	    for (j = 0; j < base[i].num_indexes; j++) { 	   /* Index  */
		printf("Index %-13s  %s %s",
		       base[i].index[j].name,
		       base[i].index[j].dupkeys ? "DupKeys" : "Unique ",
		       base[i].index[j].nocase	? "NoCase"  : "Case  ");
		for (k = 0; k < base[i].index[j].num_fields; k++)  /* Field  */
		    printf("  %s", base[i].index[j].field[k]);
		printf("\n");
	    }
	}
	printf("\n");
#endif
}/*read_conf()*/


/* ------------------------------------------------------------------------- */
boolean build_table (void)
{
	RAW_FILE f;
	FILE_OFS ofs, last_ofs;
	boolean  crosslinks = false;
	long	 i;

	sprintf(buffer,"%s%s.DAT", work_base->path, work_base->file);
	if (!f.open(buffer, f.o_rdwr)) {
	   fprintf(stderr,"! Can't open database file!\n");
	   exit (1);
	}

	table_size = (f.length() / DB_BLOCKSIZE);
	if (f.length() % DB_BLOCKSIZE)
	   fprintf(stderr,"- Database filelength is not a multiple of the blocksize\n");

	printf("+ Building table of records\n", work_base->file);

	table_file.open(PRGNAME".TBL",
			table_file.o_rdwr | table_file.o_creat | table_file.o_trunc,
			table_file.s_iread | table_file.s_iwrite);

	f.read(&dbheader,DB_BLOCKSIZE);
	ofs = dbheader.free;
	if (ofs != OFS_NONE) {
	   ofs /= DB_BLOCKSIZE;
	   ofs *= sizeof (FILE_OFS);
	}
	table_file.write(&ofs,sizeof (FILE_OFS));

	printf("* Scanning blocks\n");
	for (i = 1; f.read(&dbblock,DB_BLOCKSIZE); i++) {
	    printf("  %ld/%ld\r", i,table_size - 1);
	    ofs = dbblock.next;
	    if (ofs != OFS_NONE) {
	       ofs /= DB_BLOCKSIZE;
	       ofs *= sizeof (FILE_OFS);
	    }
	    table_file.write(&ofs,sizeof (FILE_OFS));
	}

	f.close();

	printf("* Tracing links\n");
	for (i = 0; i < table_size; i++) {
	    printf("  %ld/%ld\r", i,table_size - 1);
	    table_file.seek(i * sizeof (FILE_OFS), table_file.seek_set);
	    table_file.read(&ofs,sizeof (FILE_OFS));
	    if (ofs == 0L) continue;			// Already processed!
	    while (ofs != OFS_NONE) {			// Not end of list?
		  last_ofs = ofs;			// Save this offset
		  table_file.seek(ofs, table_file.seek_set);
		  table_file.read(&ofs,sizeof (FILE_OFS));     // Seek and read
		  if (ofs == 0L) {			// Already processed
		     crosslinks = true; 		// Meaning a cross-link
		     break;
		  }
		  table_file.seek(last_ofs, table_file.seek_set);
		  last_ofs = 0L;			// Seek and mark
		  table_file.write(&last_ofs,sizeof (FILE_OFS));  // write back
	    }
	}

	return (crosslinks);
}/*build_table()*/


/* ------------------------------------------------------------------------- */
byte do_reindex (void)
{
	struct _index *work_index;
	BP_FLAGS       bp_flags;
	DB_FLAGS       db_flags;
	FILE_OFS       ofs;
	long	       i;
	byte	       j;
	char	      *p;

	printf("+ REINDEX %s\n", work_base->file);

	if (build_table()) {
	   fprintf(stderr,"! Database contains cross-linked records, use REBUILD\n");
	   table_file.close();
	   unlink(PRGNAME".TBL");
	   return (1);
	}

	sprintf(buffer,"%s%s.DAT", work_base->path, work_base->file);
	db_flags = DB_NONE;
	if (work_base->counter) db_flags |= DB_COUNTER;
	if (!dat.open(&datcache,buffer,db_flags)) {
	   fprintf(stderr,"! Can't open database file or different format version!\n");
	   return (1);
	}

	sprintf(buffer,"%s%s.IDX", work_base->path, work_base->file);
	if (ffirst(buffer)) unlink (buffer);	// If exists, kill old idx file
	if (!idx_file.open(&idxcache,buffer)) { // Create our new idx file
	   fprintf(stderr,"! Can't create index file or different format version!\n");
	   return (1);
	}
	for (j = 0; j < work_base->num_indexes; j++) {	// Init all the indexes
	    work_index = &(work_base->index[j]);
	    bp_flags = BP_NONE;
	    if (work_index->dupkeys) bp_flags |= BP_DUPKEYS;
	    if (work_index->nocase)  bp_flags |= BP_NOCASE;
	    work_index->idx->init(&idx_file, work_index->name,
				 work_index->num_fields, bp_flags);
	}

	printf("+ Processing records for index\n");
	table_file.seek(sizeof (FILE_OFS),table_file.seek_set);
	for (i = 1; i < table_size; i++) {
	    printf("  %ld/%ld\r", i,table_size - 1);
	    table_file.read(&ofs,sizeof (FILE_OFS));
	    if (ofs == 0L) continue;
	    ofs = i * DB_BLOCKSIZE;
	    entry.datarec = ofs;
	    rec.set_text(text,MAX_TEXT);
	    dat.get(&rec,ofs);

	    if (work_base->shared) {
	       if (!(p = rec.get("tag"))) continue;
	       for (j = 0; j < work_base->num_indexes; j++)
		   if (!stricmp(p,work_base->index[j].name)) break;
	       if (j >= work_base->num_indexes) continue;
	       work_index = &(work_base->index[j]);
	       for (j = 0; j < work_index->num_fields; j++) {
		   p = rec.get(work_index->field[j]);
		   strcpy(entry.key[j], p ? p : "");
	       }
	       work_index->idx->add(&entry);
	    }
	    else {  /* exclusive base */
	       for (j = 0; j < work_base->num_indexes; j++) {
		   work_index = &(work_base->index[j]);
		   for (j = 0; j < work_index->num_fields; j++) {
		       p = rec.get(work_index->field[j]);
		       strcpy(entry.key[j], p ? p : "");
		   }
		   work_index->idx->add(&entry);
	       }
	    }
	}

	dat.close();
	idx_file.close();

	printf("* Total of %u records\n", table_size - 1);

	return (0);
}/*do_reindex()*/


/* ------------------------------------------------------------------------- */
byte do_rebuild (void)
{
	struct _index *work_index;
	BP_FLAGS       bp_flags;
	DB_FLAGS       db_flags;
	FILE_OFS       ofs;
	char	       work[MAX_BUFFER + 1];
	long	       i;
	byte	       j;
	char	      *p;

	printf("+ REBUILD %s\n", work_base->file);

	build_table();	 // No check, cross-links vanish when rebuilding anyway

	sprintf(buffer,"%s%s.DAT", work_base->path, work_base->file);
	db_flags = DB_NONE;
	if (work_base->counter) db_flags |= DB_COUNTER;
	if (!dat.open(&datcache,buffer,db_flags)) {
	   fprintf(stderr,"! Can't open old database file or different format version!\n");
	   return (1);
	}

	sprintf(buffer,"%s%s.IDX", work_base->path, work_base->file);
	if (ffirst(buffer)) unlink (buffer);	// If exists, kill old idx file
	if (!idx_file.open(&idxcache,buffer)) { // Create our new idx file
	   fprintf(stderr,"! Can't create index file!\n");
	   return (1);
	}
	for (j = 0; j < work_base->num_indexes; j++) {	// Init all the indexes
	    work_index = &(work_base->index[j]);
	    bp_flags = BP_NONE;
	    if (work_index->dupkeys) bp_flags |= BP_DUPKEYS;
	    if (work_index->nocase)  bp_flags |= BP_NOCASE;
	    work_index->idx->init(&idx_file, work_index->name,
				 work_index->num_fields, bp_flags);
	}

	sprintf(buffer,"%s%s.$$$", work_base->path, work_base->file);
	if (ffirst(buffer)) unlink (buffer);
	db_flags = DB_NONE;
	if (work_base->counter) db_flags |= DB_COUNTER;
	if (!newdat.open(&datcache,buffer,db_flags)) {
	   fprintf(stderr,"! Can't create new database file!\n");
	   return (1);
	}

	printf("+ Copying records and creating index\n");
	table_file.seek(sizeof (FILE_OFS),table_file.seek_set);
	for (i = 1; i < table_size; i++) {
	    printf("  %ld/%ld\r", i,table_size - 1);
	    table_file.read(&ofs,sizeof (FILE_OFS));
	    if (ofs == 0L) continue;
	    ofs = i * DB_BLOCKSIZE;
	    rec.set_text(text,MAX_TEXT);
	    dat.get(&rec,ofs);

	    if (work_base->shared) {
	       if (!(p = rec.get("tag"))) continue;
	       for (j = 0; j < work_base->num_indexes; j++)
		   if (!stricmp(p,work_base->index[j].name)) break;
	       if (j >= work_base->num_indexes) continue;
	       work_index = &(work_base->index[j]);
	       for (j = 0; j < work_index->num_fields; j++) {
		   p = rec.get(work_index->field[j]);
		   strcpy(entry.key[j], p ? p : "");
	       }
	       ofs = newdat.put(&rec);
	       entry.datarec = ofs;
	       work_index->idx->add(&entry);
	    }
	    else {  /* exclusive base */
	       ofs = newdat.put(&rec);
	       for (j = 0; j < work_base->num_indexes; j++) {
		   work_index = &(work_base->index[j]);
		   for (j = 0; j < work_index->num_fields; j++) {
		       p = rec.get(work_index->field[j]);
		       strcpy(entry.key[j], p ? p : "");
		   }
		   entry.datarec = ofs;
		   work_index->idx->add(&entry);
	       }
	    }
	}

	dat.close();
	newdat.close();
	idx_file.close();

	printf("+ Deleting old database and renaming new\n");
	sprintf(buffer,"%s%s.DAT", work_base->path, work_base->file);
	unlink(buffer);
	sprintf(buffer,"%s%s.$$$", work_base->path, work_base->file);
	sprintf(work,"%s.DAT", work_base->file);
	rename(buffer,work);

	printf("* Total of %u records\n", table_size - 1);

	return (0);
}/*do_rebuild()*/


/* ------------------------------------------------------------------------- */
byte do_datfree (word num_blocks)
{
	RAW_FILE      f;
	DB_FLAGS      db_flags;
	FILE_OFS      free;
	register word i;

	if (num_blocks < 1 || num_blocks > 32768U) {
	   fprintf(stderr,"! Range of valid no. blocks to add is 1 to 32768\n");
	   return (1);
	}

	printf("+ DATFREE %s %u\n", work_base->file, num_blocks);

	sprintf(buffer,"%s%s.DAT", work_base->path, work_base->file);
	db_flags = DB_NONE;
	if (work_base->counter) db_flags |= DB_COUNTER;
	if (!dat.open(&datcache,buffer,db_flags)) {
	   fprintf(stderr,"! Can't open/create database file or different format version!\n");
	   return (1);
	}
	dat.close();

	f.open(buffer, f.o_rdwr);
	f.seek(0L,f.seek_set);
	f.read(&dbheader,sizeof (DB_HEADER));
	free = dbheader.free;
	dbheader.free = f.length();

	memset(&dbblock,0,sizeof (DB_BLOCK));
	dbblock.next  = dbheader.free;

	for (i = 1; i <= num_blocks; i++) {
	    printf("  %u/%u\r", i,num_blocks);
	    f.seek(dbblock.next,f.seek_set);
	    if (i == num_blocks)
	       dbblock.next = free;
	    else
	       dbblock.next += DB_BLOCKSIZE;
	    f.write(&dbblock,sizeof (DB_BLOCK));
	}

	f.seek(0L,f.seek_set);
	f.write(&dbheader,sizeof (DB_HEADER));
	f.close();

	return (0);
}/*do_datfree()*/


/* ------------------------------------------------------------------------- */
byte do_idxfree (word num_blocks)
{
	RAW_FILE      f;
	FILE_OFS      free;
	register word i;

	if (num_blocks < 1 || num_blocks > 32768U) {
	   fprintf(stderr,"! Range of valid no. blocks to add is 1 to 32768\n");
	   return (1);
	}

	printf("+ IDXFREE %s %u\n", work_base->file, num_blocks);

	sprintf(buffer,"%s%s.IDX", work_base->path, work_base->file);
	if (!idx_file.open(&idxcache,buffer)) {
	   fprintf(stderr,"! Can't open/create index file or different format version!\n");
	   return (1);
	}
	idx_file.close();

	f.open(buffer, f.o_rdwr);
	f.seek(0L,f.seek_set);
	f.read(&bpheader,sizeof (BP_HEADER));
	free = bpheader.free;
	bpheader.free = f.length();

	memset(&bpblock,0,sizeof (BP_BLOCK));
	bpblock.lower = bpheader.free;

	for (i = 1; i <= num_blocks; i++) {
	    printf("  %u/%u\r", i,num_blocks);
	    f.seek(bpblock.lower,f.seek_set);
	    if (i == num_blocks)
	       bpblock.lower = free;
	    else
	       bpblock.lower += BP_BLOCKSIZE;
	    f.write(&bpblock,sizeof (BP_BLOCK));
	}

	f.seek(0L,f.seek_set);
	f.write(&bpheader,sizeof (BP_HEADER));
	f.close();

	return (0);
}/*do_idxfree()*/


/* ------------------------------------------------------------------------- */
byte do_import (char *index_name)
{
	struct _index *work_index;
	BP_FLAGS       bp_flags;
	DB_FLAGS       db_flags;
	FILE	      *fp;
	FILE_OFS       ofs;
	char	      *name;
	word	       n, d, total_n, total_d;
	long	       textlen;
	register char *p, *q;
	register word  i, j, k, l, m;

	if (index_name) {
	   for (j = 0; j < work_base->num_indexes; j++)
	       if (!stricmp(index_name,work_base->index[j].name)) break;
	   if (j == work_base->num_indexes) {
	      fprintf(stderr,"! Index name '%s' not known for base '%s'\n",
			     index_name, work_base->file);
	      return (1);
	   }
	   name = index_name;
	}
	else
	   name = work_base->file;

	printf("+ IMPORT %s %s\n",
	       work_base->file, index_name ? index_name : "");

	sprintf(buffer,"%s%s.DAT", work_base->path, work_base->file);
	db_flags = DB_NONE;
	if (work_base->counter) db_flags |= DB_COUNTER;
	if (!dat.open(&datcache,buffer,db_flags)) {
	   fprintf(stderr,"! Can't open/create database file or different format version!\n");
	   return (1);
	}

	sprintf(buffer,"%s%s.IDX", work_base->path, work_base->file);
	if (!idx_file.open(&idxcache,buffer)) { // Open/create our idx file
	   fprintf(stderr,"! Can't open/create index file or different format version!\n");
	   return (1);
	}
	for (j = 0; j < work_base->num_indexes; j++) {	// Init all the indexes
	    work_index = &(work_base->index[j]);
	    bp_flags = BP_NONE;
	    if (work_index->dupkeys) bp_flags |= BP_DUPKEYS;
	    if (work_index->nocase)  bp_flags |= BP_NOCASE;
	    work_index->idx->init(&idx_file, work_index->name,
				 work_index->num_fields, bp_flags);
	}

	printf("+ Scanning for import files %-.8s.000 to %-.8s.099\n",
	       name, name);
	total_n = total_d = 0;
	for (i = 0; i <= 99; i++) {
	    printf("  File %03u\r", i);
	    sprintf(buffer,"%-.8s.%03u", name, i);
	    if (!(fp = fopen(buffer,"rt"))) continue;
	    printf("+ Reading %s\n", buffer);

	    n = d = 0;
	    rec.clear();
	    textlen = 0L;
	    while (fgets(buffer,MAX_BUFFER,fp)) {
		  p = strtok(buffer," :\r\n\032");
		  if (!p || !*p) {
		     if (textlen) {
			fread(text,1,(word) textlen,fp);
			fgets(buffer,MAX_BUFFER,fp);
		     }
		     rec.set_text(text,textlen);
#if 0
printf("\n");
for (p = rec.first(); p; p = rec.next())
    printf("%s: %s\n", p, rec.get());
if (textlen) {
   printf("DB_TEXTLEN: %ld\n",textlen);
   printf("\n");
   printf(text);
}
printf("\n");
#endif

		     if (work_base->shared) {
			if (!(p = rec.get("tag"))) continue;
			for (j = 0; j < work_base->num_indexes; j++)
			    if (!stricmp(p,work_base->index[j].name)) break;
			if (j >= work_base->num_indexes) continue;
			work_index = &(work_base->index[j]);

			ofs = dat.put(&rec);

			for (j = 0; true; j++) {
			    for (k = 0; k < work_index->num_fields; k++) {
				p = rec.get(work_index->field[k]);
				if (!k && j) {		// First key field only
				   for (l = 0; l < j; l++) {
				       p = rec.next(work_index->field[0]);
				       if (!p) goto shared_break;
				   }
				}
				if (p && *p == '\"') {
				   strcpy(buffer,p);
				   parse((byte *) buffer, (byte **) av);
				   p = av[0];
				}
				strcpy(entry.key[k], p ? p : "");
			    }
			    if (!*entry.key[0]) {	// First field empty
			       dat.del(ofs);
			       goto shared_break;
			    }
			    entry.datarec = ofs;
			    if (work_index->idx->add(&entry) != BP_OK) {
			       dat.del(ofs);
			       if (!j) d++;
			    }
			    else {
			       if (!j) n++;
			    }
			}
shared_break:		;
		     }
		     else {  /* exclusive base */
			ofs = dat.put(&rec);

			for (k = 0; k < work_base->num_indexes; k++) {
			    for (j = 0; true; j++) {
				work_index = &(work_base->index[k]);
				for (l = 0; l < work_index->num_fields; l++) {
				    p = rec.get(work_index->field[l]);
				    if (!l && j) {	// First key field only
				       for (m = 0; m < j; m++) {
					   p = rec.next(work_index->field[0]);
					   if (!p) goto exclusive_break;
				       }
				    }
				    if (p && *p == '\"') {
				       strcpy(buffer,p);
				       parse((byte *) buffer, (byte **) av);
				       p = av[0];
				    }
				    strcpy(entry.key[l], p ? p : "");
				}
				if (!*entry.key[0])	// First field empty
				   continue;
				entry.datarec = ofs;
				if (work_index->idx->add(&entry) != BP_OK) {
				   dat.del(ofs);
				   if (!j && !k) d++;
				   goto exclusive_break;
				}
				if (!j && !k) n++;
			    }
exclusive_break:	    ;
			}
		     }

		     printf("  Recs=%u  Dups=%u\r", n, d);
		     rec.clear();
		     textlen = 0L;
		     continue;
		  }

		  q = strtok(NULL,"\r\n\032");
		  if (!q) continue;
		  while (*q == ' ') q++;
		  if (!*q) continue;
		  if (!strcmp(p,"DB_TEXTLEN"))
		     textlen = atol(q);
		  else
		     rec.set(q,p);
	    }

	    total_n += n;
	    total_d += d;
	    printf("\n");
	    fclose(fp);
	}

	printf("* Total of %u records imported (%u duplicate entries skipped)\n",
	       total_n, total_d);

	dat.close();
	idx_file.close();

	return (0);
}/*do_import()*/


/* ------------------------------------------------------------------------- */
byte do_export (char *index_name)
{
	struct _index *work_index;
	BP_FLAGS       bp_flags;
	DB_FLAGS       db_flags;
	BP_RES	       res;
	FILE	      *fp;
	char	      *name;
	register char *p;
	register word  i, j;

	if (index_name) {
	   for (j = 0; j < work_base->num_indexes; j++)
	       if (!stricmp(index_name,work_base->index[j].name)) break;
	   if (j == work_base->num_indexes) {
	      fprintf(stderr,"! Index name '%s' not known for base '%s'\n",
			     index_name, work_base->file);
	      return (1);
	   }
	   name = index_name;
	}
	else
	   name = work_base->file;

	printf("+ EXPORT %s %s\n",
	       work_base->file, index_name ? index_name : "");

	sprintf(buffer,"%s%s.DAT", work_base->path, work_base->file);
	db_flags = DB_NONE;
	if (work_base->counter) db_flags |= DB_COUNTER;
	if (!dat.open(&datcache,buffer,db_flags)) {
	   fprintf(stderr,"! Can't open/create database file or different format version!\n");
	   return (1);
	}

	sprintf(buffer,"%s%s.IDX", work_base->path, work_base->file);
	if (!idx_file.open(&idxcache,buffer)) { // Open/create our idx file
	   fprintf(stderr,"! Can't open/create index file or different format version!\n");
	   return (1);
	}
	for (j = 0; j < work_base->num_indexes; j++) {	// Init all the indexes
	    work_index = &(work_base->index[j]);
	    bp_flags = BP_NONE;
	    if (work_index->dupkeys) bp_flags |= BP_DUPKEYS;
	    if (work_index->nocase)  bp_flags |= BP_NOCASE;
	    work_index->idx->init(&idx_file, work_index->name,
				 work_index->num_fields, bp_flags);
	}

	if (index_name) {
	   for (j = 0; j < work_base->num_indexes; j++)
	       if (!stricmp(index_name,work_base->index[j].name)) break;
	   work_index = &(work_base->index[j]);
	}
	else
	   work_index = &(work_base->index[0]);

	sprintf(buffer,"%-.8s.000", name);
	unique_name(buffer);
	printf("+ Writing to export file %s\n", buffer);
	if (!(fp = fopen(buffer,"w+t"))) {
	   fprintf(stderr,"! Can't create export file\n");
	   return (1);
	}

	res = work_index->idx->first(&entry);
	for (i = 0; res == BP_OK; ) {
	    printf("  %u\r", ++i);
	    rec.set_text(text,MAX_TEXT);
	    dat.get(&rec,entry.datarec);

#if 0
for (p = rec.first(); p; p = rec.next())
    printf("%s: %s\n", p, rec.get());
if (rec.get_textlen()) {
   printf("DB_TEXTLEN: %ld\n",rec.get_textlen());
   printf("\n");
   printf(text);
}
printf("\n");
#endif

	    for (p = rec.first(); p; p = rec.next())
		fprintf(fp,"%s: %s\n", p, rec.get());
	    if (rec.get_textlen()) {
	       fprintf(fp,"DB_TEXTLEN: %ld\n",rec.get_textlen());
	       fprintf(fp,"\n");
	       fwrite(text,1,(word) rec.get_textlen(),fp);
	    }
	    fprintf(fp,"\n");

	    res = work_index->idx->next(&entry);
	}

	printf("* Total of %u records\n", i);

	fclose(fp);
	dat.close();
	idx_file.close();

	return (0);
}/*do_export()*/


/* ------------------------------------------------------------------------- */
void main (int argc, char *argv[])
{
	register byte i;

	signal(SIGINT,SIG_IGN); 			   /* disable Ctrl-C */

	fprintf(stderr,PRGNAME" ("PRJNAME"); Version "VERSION", Created on "__DATE__" at "__TIME__"\n");
	fprintf(stderr,"Design & COPYRIGHT (C) 1991 by LENTZ SOFTWARE-DEVELOPMENT; ALL RIGHTS RESERVED\n");
	fprintf(stderr,"File format versions: Database=%d, Index=%d\n\n", DB_VERSION, BP_VERSION);

	if (argc < 3) goto usage;

	read_conf();

	for (i = 0; i < num_bases; i++)
	    if (!stricmp(argv[2],base[i].file)) break;
	if (i >= num_bases) {
	   fprintf(stderr,"! Specified base '%s' not known in configuration\n",argv[2]);
	   exit (1);
	}
	work_base = &base[i];

	if	(!stricmp(argv[1],"REINDEX") && argc == 3)
	   i = do_reindex();
	else if (!stricmp(argv[1],"REBUILD") && argc == 3)
	   i = do_rebuild();
	else if (!stricmp(argv[1],"DATFREE") && argc == 4)
	   i = do_datfree((word) atol(argv[3]));
	else if (!stricmp(argv[1],"IDXFREE") && argc == 4)
	   i = do_idxfree((word) atol(argv[3]));
	else if (!stricmp(argv[1],"IMPORT")) {
	   if (work_base->shared) {
	      if (argc != 4) goto usage;
	      i = do_import(argv[3]);
	   }
	   else {
	      if (argc != 3) goto usage;
	      i = do_import(NULL);
	   }
	}
	else if (!stricmp(argv[1],"EXPORT")) {
	   if (work_base->shared) {
	      if (argc != 4) goto usage;
	      i = do_export(argv[3]);
	   }
	   else {
	      if (argc != 3) goto usage;
	      i = do_export(NULL);
	   }
	}
	else
	   goto usage;

	table_file.close();
	if (ffirst(PRGNAME".TBL")) unlink(PRGNAME".TBL");

	if (!i) printf("+ Done!        \n");
	exit (i);


usage:	fprintf(stderr,"Usage: "PRGNAME" <command> <base> [<blocks>|<index>]\n\n");
	fprintf(stderr,"Options: REINDEX <base>           Rebuild indexes from database\n");
	fprintf(stderr,"         REBUILD <base>           Rebuild database (by copying), and indexes\n");
	fprintf(stderr,"         DATFREE <base> <blocks>  Add n free %u byte blocks to database file\n", DB_BLOCKSIZE);
	fprintf(stderr,"         IDXFREE <base> <blocks>  Add n free %u byte blocks to index file\n", BP_BLOCKSIZE);
	fprintf(stderr,"         IMPORT  <base> [<index>] Import textfiles into database\n");
	fprintf(stderr,"         EXPORT  <base> [<index>] Export database to textfile\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"NOTE: BEFORE using this program, make ABSOLUTELY SURE there are no other\n");
	fprintf(stderr,"      applications active which also use the specified database/index files!\n");
	exit (0);
}/*main()*/


/* end of dbutils.cpp */
