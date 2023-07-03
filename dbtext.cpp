/* ------------------------------------------------------------------------- */
/* Pigeon Post (WAITS)	-  Database functions with free-format ASCII records */
/* Design & COPYRIGHT (C) 1990,91 by A.G. Lentz & LENTZ SOFTWARE-DEVELOPMENT */
/* ------------------------------------------------------------------------- */
#include <string.h>
#include "dbtext.h"

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif


/* ------------------------------------------------------------------------- */
void DB_TEXT :: work_flush (DB_WORK *work)
{
	work->file_ofs = get_free();
	put_block(&(work->block),work->file_ofs);

	if (work->prev_ofs != OFS_NONE) {
	   get_block(&(work->block),work->prev_ofs);
	   work->block.next = work->file_ofs;
	   put_block(&(work->block),work->prev_ofs);
	}
	else
	   work->start_ofs = work->file_ofs;

	work->prev_ofs	 = work->file_ofs;
	memset(&(work->block), 0, sizeof (DB_BLOCK));
	work->block.next = OFS_NONE;
	work->ptr	 = work->block.data;
	work->count	 = DB_BLOCKSPACE;
}


/* ------------------------------------------------------------------------- */
DB_RECORD :: DB_RECORD (void)
{
	num_fields = cur_field = 0;
	text_len = 0;
}


DB_RECORD :: ~DB_RECORD (void)
{
	clear();
}


void DB_RECORD :: clear (void)
{
	register byte i;

	for (i = 0; i < num_fields; i++) {
	    free(fields[i].tag);
	    free(fields[i].val);
	}
	num_fields = cur_field = 0;
	text_len = 0;
}


char * DB_RECORD :: get (const char *tag)
{
	if (tag) {
	   register byte i;

	   for (i = 0; i < num_fields; i++)
	       if (!stricmp(tag,fields[i].tag)) break;
	   if (i < num_fields)
	      cur_field = i;
	   else
	      return (NULL);
	}
	else if (cur_field >= num_fields)
	   return (NULL);

	return (fields[cur_field].val);
}/*get()*/


boolean DB_RECORD :: set (const char *val, const char *tag, const char *oldval)
{
	if (tag) {
	   register char *p;

	   p = get(tag);

	   if (oldval) {
	      while (p && strcmp(oldval,p)) {
		    p = next();
		    if (!p || stricmp(tag,p))
		       return (false);
		    p = get();
	      }
	      if (!p) return (false);
	   }
	   else {
	      if (num_fields >= DB_MAXFIELDS)
		 return (false);
	      if (p) {
		 do p = next();
		 while (p && !stricmp(tag,p));
		 if (p)
		    memmove(&fields[cur_field + 1], &fields[cur_field],
			    (num_fields - cur_field) * sizeof (_db_field));
		 else
		    cur_field++;
	      }
	      else 
		 cur_field = num_fields;
	      num_fields++;
	      fields[cur_field].tag = NULL;
	   }
	}
	else if (cur_field >= num_fields)
	   return (false);

	if (fields[cur_field].tag) {
	   free(fields[cur_field].tag);
	   free(fields[cur_field].val);
	}
	fields[cur_field].tag = strdup(tag);
	fields[cur_field].val = strdup(val);

	if (!fields[cur_field].tag || !fields[cur_field].val)
	   alloc_fail();

	return (true);
}/*set()*/


boolean DB_RECORD :: del (const char *tag, const char *oldval)
{
	if (tag) {
	   register char *p;

	   p = get(tag);

	   if (oldval) {
	      while (p && strcmp(oldval,p)) {
		    p = next();
		    if (!p || stricmp(tag,p))
		       return (false);
		    p = get();
	      }
	   }
	   if (!p) return (false);
	}
	else if (cur_field >= num_fields)
	   return (false);

	if (fields[cur_field].tag) {
	   free(fields[cur_field].tag);
	   free(fields[cur_field].val);
	}

	memmove(&fields[cur_field], &fields[cur_field + 1],
		(--num_fields - cur_field) * sizeof (_db_field));

	return (true);
}/*del()*/


char * DB_RECORD :: first (char *tag)
{
	if (!num_fields)
	   return (NULL);
	cur_field = 0;
	if (!tag)
	   return (fields[cur_field].tag);
	else
	   return (!stricmp(tag,fields[cur_field].tag) ? get() : NULL);
}/*first()*/


char * DB_RECORD :: last (char *tag)
{
	if (!num_fields)
	   return (NULL);
	cur_field = num_fields - 1;
	if (!tag)
	   return (fields[cur_field].tag);
	else
	   return (!stricmp(tag,fields[cur_field].tag) ? get() : NULL);
}/*last()*/


char * DB_RECORD :: next (char *tag)
{
	if (!num_fields || (cur_field >= (num_fields - 1)))
	   return (NULL);
	cur_field++;
	if (!tag)
	   return (fields[cur_field].tag);
	else
	   return (!stricmp(tag,fields[cur_field].tag) ? get() : NULL);
}/*next()*/


char * DB_RECORD :: prev (char *tag)
{
	if (!num_fields || !cur_field)
	   return (NULL);
	cur_field--;
	if (!tag)
	   return (fields[cur_field].tag);
	else
	   return (!stricmp(tag,fields[cur_field].tag) ? get() : NULL);
}/*prev()*/


/* ------------------------------------------------------------------------- */
void DB_TEXT :: db_lock (void)
{
	if (share_active()) {
	   dword oldsharecnt = header->sharecnt;// Remember old share cnt

	   while (!f.lock(0L,1L));		// Lock first byte of file
	   get_header();			// Get header from disk
	   if (header->sharecnt != oldsharecnt) // Did someone write something?
	      cache->clear(&f); 		// Yes, so clear complete cache
	   did_write = false;
	}

	changed_header = false;
}


void DB_TEXT :: db_unlock (void)
{
	if (share_active()) {			// Only when file sharing on
	   if (changed_header || did_write) {
	      cache->flush(&f); 		// Write all blocks to disk
	      if (did_write) header->sharecnt++;
	      put_header();
	      cache->clear(&f,0L);		// Clear hdr block from cache
	   }
	   f.unlock(0L,1L);			// Unlock first byte of file
	}
	else if (changed_header)		// No file sharing active
	   put_header();			// From memory to cache
}


/* ------------------------------------------------------------------------- */
boolean DB_TEXT :: open (FILE_CACHE *cp, const char *pathname,
			 DB_FLAGS db_flags)
{
	cache = cp;
	if (!(header = new DB_HEADER)) alloc_fail();

	did_write = changed_header = false;

	if (f.sopen(pathname, f.o_rdwr, f.sh_denynone)) { // Dat already exists
	   while (!f.lock(0L,1L));
	   get_header();
	   if (header->version != DB_VERSION) {
	      db_unlock();
	      close();
	      return (false);
	   }
	   if (db_flags != DB_DUMMY && db_flags != header->flags) {
	      header->flags = db_flags;
	      changed_header = true;
	   }
	   db_unlock();
	   return (true);
	}
	else if (!f.sopen(pathname, f.o_rdwr|f.o_creat,
			  f.sh_denynone, f.s_iread|f.s_iwrite))
	   return (false);			// Can't create - return error

	// Created new database file - initialize and write the header
	while (!f.lock(0L,1L));

	memset(header,0,sizeof (DB_HEADER));
	header->version  = DB_VERSION;			// File format version
	header->free	 = OFS_NONE;			// No empty blocks
	header->sharecnt = 0UL; 			// Init share counter
	if (db_flags == DB_DUMMY) db_flags = DB_NONE;	// Fix default flags
	header->flags	 = db_flags;			// Put flags in place
	header->counter  = 0UL; 			// Ini sequence counter

	changed_header = true;
	db_unlock();

	return (true);					// Return success
}/*open()*/


void DB_TEXT :: close (void)
{
	if (header) {
	   cache->clear(&f);
	   f.close();
	   delete header;
	   header = NULL;
	}
}/*close()*/


void DB_TEXT :: get (DB_RECORD *record, FILE_OFS offset)
{
	DB_BLOCK *block = new DB_BLOCK;
	long	  textlen;
	register char *s, *p, *q;
	register int i;

	if (!block) alloc_fail();
	db_lock();

	textlen = record->get_textlen();
	record->clear();			// Sets internal text_len to 0
	record->set_textlen(textlen);

	textlen = 0;
	s = record->get_textptr();
	block->next = offset;
	i = 0;

	while (1) {
	      if (i <= 0) {
		 get_block(block,block->next);
		 p = block->data;
		 i = DB_BLOCKSPACE;
	      }
	      i--;

	      if (!(*s++ = *p++)) {
		 s = record->get_textptr();
		 if (!*s) break;
		 q = strchr(s,':');
		 *q++ = '\0';
		 if (!strcmp(s,"DB_TEXTLEN"))
		    textlen = atol(q);
		 else
		    record->set(q,s);
	      }
	}

	if (textlen) {
	   if (textlen > record->get_textlen()) 	// Not enough room
	      textlen = record->get_textlen();
	   if (textlen == record->get_textlen())	// Exactly to buf end?
	      textlen--;				// Substract one for \0

	   record->set_textlen(textlen);		// This'll be text len
	   s = record->get_textptr();
	   s[(word) textlen] = '\0';			// Set \0 past end

	   if (i) {			// Minimum textbuf avail MUST be 1K !!
	      memmove(s,p,i);
	      s += i;
	      textlen -= i;
	   }

	   while (textlen && block->next != OFS_NONE) { // Read while room avl
		 get_block(block,block->next);
		 i = (int) min(textlen,DB_BLOCKSPACE);
		 memmove(s,block->data,i);
		 s += i;
		 textlen -= i;
	   }
	}
	else						// No textfield in rec
	   record->set_textlen(0);			// Say it in user's rec

	db_unlock();
	delete block;
}/*get()*/


FILE_OFS DB_TEXT :: put (DB_RECORD *record, FILE_OFS offset)
{
	DB_WORK  *db_work = new DB_WORK;
	char	  buf[30];
	long	  textlen;
	register char *s;

	if (!db_work) alloc_fail();

	if (offset != OFS_NONE)
	   del(offset);

	db_lock();

	work_init(db_work);

	if ((header->flags & DB_COUNTER) && !record->get("DB_COUNTER")) {
	   header->counter++;
	   changed_header = true;
	   sprintf(buf,"%010lu",header->counter);
	   record->set(buf,"DB_COUNTER");
	}

	for (s = record->first(); s; s = record->next()) {
	    while (*s) work_update(db_work,*s++);
	    work_update(db_work,':');
	    s = record->get();
	    while (*s) work_update(db_work,*s++);
	    work_update(db_work,'\0');
	}

	textlen = record->get_textlen();
	if (textlen) {
	   sprintf(buf,"DB_TEXTLEN:%ld",textlen);
	   s = buf;
	   while (*s) work_update(db_work,*s++);
	   work_update(db_work,'\0');
	}

	work_update(db_work,'\0');

	if (textlen) {
	   for (s = record->get_textptr(); textlen > 0; textlen--)
	       work_update(db_work,*s++);
	}

	offset = work_finish(db_work);

	db_unlock();
	delete db_work;
	return (offset);
}/*put()*/


void DB_TEXT :: del (FILE_OFS offset)
{
	DB_BLOCK  *block  = new DB_BLOCK;
	FILE_OFS   old_free;

	if (!block) alloc_fail();
	db_lock();

	old_free = header->free;		// Save start of old free list
	header->free = offset;			// Begin of new free list
	changed_header = true;

	while (1) {				// Find last block of record
	      get_block(block,offset);		// Read in each block
	      if (block->next == OFS_NONE)	// Last one yet?  Then break
		 break;
	      offset = block->next;		// No, get filepos of next
	}

	block->next = old_free; 		// Link old list to end of new
	put_block(block,offset);		// Rewrite block with new link

	db_unlock();				// Rewrites header as well
	delete block;
}/*del*/


/* ------------------------------------------------------------------------- */
void DB_TEXT :: get_header (void)
{
	cache->read(&f,header,sizeof (DB_HEADER),0L);
}/*get_header()*/


void DB_TEXT :: put_header (void)
{
	cache->write(&f,header,sizeof (DB_HEADER),0L);
}/*put_header()*/


void DB_TEXT :: get_block (DB_BLOCK *block, FILE_OFS offset)
{
	cache->read(&f,block,sizeof (DB_BLOCK),offset);
}/*get_block()*/


void DB_TEXT :: put_block (DB_BLOCK *block, FILE_OFS offset)
{
	cache->write(&f,block,sizeof (DB_BLOCK),offset);
	did_write = true;
}/*put_block()*/


/* ------------------------------------------------------------------------- */
FILE_OFS DB_TEXT :: get_free (void)
{
	FILE_OFS   free;

	if (header->free != OFS_NONE) { 		// Any free blocks?
	   DB_BLOCK *block = new DB_BLOCK;

	   if (!block) alloc_fail();

	   free = header->free; 			// Yes, store offset
	   get_block(block,header->free);		// Get it as well
	   header->free = block->next;			// Save new first block
	   changed_header = true;

	   delete block;
	}
	else						// No free blocks
	   free = cache->length(&f);			// Get correct file len

	return (free);					// File ofs free block
}/*get_free()*/


void DB_TEXT :: set_free (FILE_OFS offset)
{
	DB_BLOCK  *block  = new DB_BLOCK;

	if (!block) alloc_fail();

	block->next = header->free;			// Link into free chain
	put_block(block,offset);			// Write free block
	header->free = offset;				// Ofs of our free blk
	changed_header = true;

	delete block;
}/*get_free()*/


/* end of dbtext.cpp */
