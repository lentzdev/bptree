/*
	B+Tree Indexing routines - Low-level file I/O & caching functions
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
#include <string.h>
#include <dos.h>
#include "file_io.h"


/* ------------------------------------------------------------------------- */
boolean share_active (void)	     /* Return true if DOS' SHARE.EXE active */
{
	static boolean share_checked = false;	     /* Flag if checked once */
	static boolean share_flag;  /* True if share active, otherwise false */

	if (!share_checked) {		   /* Only check once, then use flag */
	   share_flag = false;
#if DOS_SHARE
	   union REGS regs;

	   regs.x.ax = 0x1000;				/* DOS Multiplexer   */
	   int86(0x2f,&regs,&regs);			/* INT 2Fh sub 1000h */
	   if (regs.h.al == 0xff)			/* AL=FFh: installed */
	      share_flag = true;
#endif
	   share_checked = true;
	}

	return (share_flag);
}/*share_active()*/


/* ------------------------------------------------------------------------- */
void XOR_FILE :: encode (register byte *buf, register word len)
{
	register byte lastc = 0xee, curc;

	while (len > 0) {
	      curc = *buf;
	      *buf++ ^= lastc;
	      lastc = curc;
	      len--;
	}
}


void XOR_FILE :: decode (register byte *buf, register word len)
{
	register byte lastc = 0xee;

	while (len > 0) {
	      *buf ^= lastc;
	      lastc = *buf++;
	      len--;
	}
};


/* ------------------------------------------------------------------------- */
static FILE_CACHE *cache_protect_first;


static void cache_protect_atexit (void)
{
	while (cache_protect_first) {
	      cache_protect_first->clear();
	      cache_protect_first = cache_protect_first->cache_protect_next;
	}
}


static void cache_protect_new (FILE_CACHE *cp)
{
	static boolean cache_protect_didinit = false;

	if (!cache_protect_didinit) {
	   cache_protect_first = NULL;
	   atexit(cache_protect_atexit);
	   cache_protect_didinit = true;
	}

	cp->cache_protect_prev = NULL;
	cp->cache_protect_next = cache_protect_first;
	cache_protect_first->cache_protect_prev = cp;
	cache_protect_first = cp;
}


static void cache_protect_delete (FILE_CACHE *cp)
{
	if (cp->cache_protect_prev)
	   cp->cache_protect_prev->cache_protect_next = cp->cache_protect_next;
	else /* first in chain */
	   cache_protect_first = cp->cache_protect_next;

	if (cp->cache_protect_next)
	   cp->cache_protect_next->cache_protect_prev = cp->cache_protect_prev;
}


/* ------------------------------------------------------------------------- */
FILE_CACHE :: FILE_CACHE (word num_blocks)
{
	pool = new CACHE_BLOCK[num_blocks];
	if (!pool) alloc_fail();
	poolsize = num_blocks;
	use_counter = 0;
	cache_protect_new(this);
}


FILE_CACHE :: ~FILE_CACHE (void)
{
	clear();
	cache_protect_delete(this);
	delete pool;
}


/* ------------------------------------------------------------------------- */
void FILE_CACHE :: clear (CACHE_BLOCK *block)
{
	if (block->f) {
	   flush(block);
	   delete block->data;
	   block->f = NULL;
	}
}


void FILE_CACHE :: clear (void)
{
	register word i;

	for (i = 0; i < poolsize; i++)
	    clear(&pool[i]);
}


void FILE_CACHE :: clear (RAW_FILE *f)
{
	register word i;

	for (i = 0; i < poolsize; i++)
	    if (pool[i].f == f) clear(&pool[i]);
}


void FILE_CACHE :: clear (RAW_FILE *f, FILE_OFS filepos)
{
	register word i;

	for (i = 0; i < poolsize; i++)
	    if (pool[i].f == f && pool[i].filepos == filepos) clear(&pool[i]);
}


void FILE_CACHE :: flush (CACHE_BLOCK *block)
{
	if (block->f && block->dirty) {
	   block->f->seek(block->filepos,block->f->seek_set);
	   block->f->write(block->data,block->length);
	   block->dirty = false;
	}
}


void FILE_CACHE :: flush (void)
{
	register word i;

	for (i = 0; i < poolsize; i++)
	    flush(&pool[i]);
}


void FILE_CACHE :: flush (RAW_FILE *f)
{
	register word i;

	for (i = 0; i < poolsize; i++)
	    if (pool[i].f == f) flush(&pool[i]);
}


void FILE_CACHE :: flush (RAW_FILE *f, FILE_OFS filepos)
{
	register word i;

	for (i = 0; i < poolsize; i++)
	    if (pool[i].f == f && pool[i].filepos == filepos) flush(&pool[i]);
}


FILE_OFS FILE_CACHE :: length (RAW_FILE *f)
{	// Because of dirty buffers at/past end of file; calc right filelength
	register word i;
	FILE_OFS length = f->length();			// End of file on disk

	for (i = 0; i < poolsize; i++) {		// Loop through pool
	    if ((pool[i].f == f) && pool[i].dirty &&
		(pool[i].filepos >= length))		// Past end of file
	       length = pool[i].filepos + pool[i].length;	// New length
	}

	return (length);				// Ret correct length
}


CACHE_BLOCK * FILE_CACHE :: find (RAW_FILE *f, FILE_OFS filepos)
{
	register word i;

	for (i = 0; i < poolsize; i++) {
	    if (pool[i].f == f && pool[i].filepos == filepos) {
	       if (pool[i].last_used != use_counter) {
		  use_counter++;
		  pool[i].last_used = use_counter;
	       }
	       return (&pool[i]);
	    }
	}
	return (NULL);
}


void FILE_CACHE :: make (RAW_FILE *f, void *buf, word len, FILE_OFS filepos, boolean writing)
{
	register word i, j;

	for (i = 0; i < poolsize; i++)			// Look for empty block
	    if (!pool[i].f) break;			// Yup, got one!
	if (i >= poolsize) {				// No empty block found
	   i = 0;					// To least recently ..
	   for (j = 1; j < poolsize; j++)		// .. used block...
	       if (pool[j].last_used < pool[i].last_used) i = j;
	   clear(&pool[i]);				// Got the bugger
	}

	if (!writing) { 				// We're reading
	   f->seek(filepos,f->seek_set);		// Seek in file
	   f->read(buf,len);				// Get data from disk
	}

	pool[i].data = new byte[len];			// Try allocate block
	if (pool[i].data) {				// Succeeded!
	   pool[i].dirty     = writing ? true : false;
	   use_counter++;
	   pool[i].last_used = use_counter;
	   pool[i].f	     = f;
	   pool[i].filepos   = filepos;
	   pool[i].length    = len;
	   memmove(pool[i].data,buf,len);
	}
	else if (writing) {				// No memory & writing
	   f->seek(filepos,f->seek_set);		// So get it do disk!
	   f->write(buf,len);
	}
}


void FILE_CACHE :: read (RAW_FILE *f, void *buf, word len, FILE_OFS filepos)
{
	CACHE_BLOCK *block;

	if ((block = find(f,filepos)) != NULL)
	   memmove(buf,block->data,len);
	else
	   make(f,buf,len,filepos,false);
}


void FILE_CACHE :: write (RAW_FILE *f, void *buf, word len, FILE_OFS filepos)
{
	CACHE_BLOCK *block;

	if ((block = find(f,filepos)) != NULL) {
	   block->dirty = true;
	   memmove(block->data,buf,len);
	}
	else
	   make(f,buf,len,filepos,true);
}


/* end of file_io.cpp */
