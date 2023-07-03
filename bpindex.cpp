/* ------------------------------------------------------------------------- */
/* Pigeon Post (WAITS)	-  B+Tree Indexing routines			     */
/* Design & COPYRIGHT (C) 1990,91 by A.G. Lentz & LENTZ SOFTWARE-DEVELOPMENT */
/* ------------------------------------------------------------------------- */
#include "bpindex.h"


/* ------------------------------------------------------------------------- */
void BP_FILE :: bp_lock (void)
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


void BP_FILE :: bp_unlock (void)
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
boolean BP_FILE :: open (FILE_CACHE *cp, const char *pathname)
{
	register int i;

	cache = cp;
	if (!(header = new BP_HEADER)) alloc_fail();

	did_write = changed_header = false;

	if (f.sopen(pathname, f.o_rdwr, f.sh_denynone)) { // idx already exists
	   while (!f.lock(0L,1L));
	   get_header();
	   bp_unlock();
	   if (header->version != BP_VERSION) {
	      close();
	      return (false);
	   }
	   return (true);
	}
	else if (!f.sopen(pathname, f.o_rdwr|f.o_creat,
			  f.sh_denynone, f.s_iread|f.s_iwrite))
	   return (false);			// Can't create - return error

	// Created new index file - initialize and write the header
	while (!f.lock(0L,1L));

	memset(header,0,sizeof (BP_HEADER));
	header->version  = BP_VERSION;			// File format version
	header->free	 = OFS_NONE;			// No empty blocks
	header->sharecnt = 0UL; 			// Init share counter
	for (i = 0; i < BP_MAXINDEX; i++)
	    header->index[i].root = OFS_NONE;		// Empty all indexes

	changed_header = true;
	bp_unlock();

	return (true);					// Return success
}/*open()*/


void BP_FILE :: close (void)
{
	if (header) {
	   cache->clear(&f);
	   f.close();
	   delete header;
	   header = NULL;
	}
}/*close()*/


/* ------------------------------------------------------------------------- */
FILE_OFS BP_FILE :: get_free (void)
{
	FILE_OFS free;

	if (header->free != OFS_NONE) { 		// Any free blocks?
	   BP_BLOCK *block = new BP_BLOCK;

	   if (!block) alloc_fail();

	   free = header->free; 			// Yes, store offset
	   get_block(block,header->free);		// Get it as well
	   header->free = block->lower; 		// Save new first block
	   changed_header = true;

	   delete block;
	}
	else						// No free blocks
	   free = cache->length(&f);			// Get correct file len

	return (free);					// File ofs free block
}/*get_free()*/


void BP_FILE :: set_free (FILE_OFS offset)
{
	BP_BLOCK *block = new BP_BLOCK;

	if (!block) alloc_fail();

	block->lower = header->free;			// Link into free chain
	put_block(block,offset);			// Write free block
	header->free = offset;				// Ofs of our free blk
	changed_header = true;

	delete block;
}/*get_free()*/


/* ------------------------------------------------------------------------- */
void BP_INDEX :: bp_lock (void)
{
	file->bp_lock();
	do_rescan = false;
	if (share_active()) {
	   if (index->sharecnt != sharecnt)
	      do_rescan = true;
	   did_write = false;
	}
	valid_info = false;
}/*bp_lock()*/


void BP_INDEX :: bp_unlock (void)
{
	if (share_active()) {
	   if (did_write) {
	      index->sharecnt++;
	      file->flag_changed_header();
	   }
	   sharecnt = index->sharecnt;
	}
	file->bp_unlock();
}/*bp_unlock()*/


/* ------------------------------------------------------------------------- */
boolean BP_INDEX :: init (BP_FILE *bpfp, const char *nametag,
			  word num_keys, BP_FLAGS bp_flags)
{
	BP_ENTRY entry;
	int	 i, firstfree;

	file = bpfp;

	file->bp_lock();

	for (firstfree = -1, i = 0; i < BP_MAXINDEX; i++) {	// Find index
	    index = file->get_index(i);
	    if (index->root != OFS_NONE) {			// In use?
	       if (!stricmp(index->nametag,nametag))
		  break;
	    }
	    else if (firstfree < 0)		// Not in use, mark first free
	       firstfree = i;
	}

	if (i < BP_MAXINDEX) {					// Index exists
	   if (bp_flags != BP_DUMMY && bp_flags != index->flags) {
	      index->flags = bp_flags;
	      file->flag_changed_header();
	   }
	   if (num_keys != index->keys) {
	      index->keys = num_keys;
	      file->flag_changed_header();
	   }
	}
	else if (firstfree >= 0) {				// Create index
	   BP_BLOCK *root = new BP_BLOCK;

	   if (!root) alloc_fail();

	   index = file->get_index(firstfree);
	   strcpy(index->nametag,nametag);		// Set right nametag
	   if (bp_flags == BP_DUMMY) bp_flags = BP_NONE;// Fix default flags
	   index->flags    = bp_flags;			// Put flags in place
	   index->keys	   = num_keys;			// Put no.keys in place
	   index->root	   = file->get_free();		// Create a root
	   index->sharecnt = 0UL;			// Init share counter
	   file->flag_changed_header();

	   memset(root,0,sizeof (BP_BLOCK));		// Clear root block
	   root->next  = 0;				// No keys in use now
	   root->lower = OFS_NONE;			// No lower levels
	   put_block(root,index->root); 		// Write empty root

	   delete root;
	}
	else {						// Not found, none free
	   file->bp_unlock();
	   return (false);				// Return failure !-(
	}

	cmpfunc = (index->flags & BP_NOCASE) ? memicmp : memcmp;

	bp_unlock();
	first(&entry);
	return (true);
}/*init()*/


BP_RES BP_INDEX :: current (BP_ENTRY *entry)
{
	if (!valid_info)
	   return (BP_FAIL);

	systouser(entry);
	return (BP_OK);
}/*current()*/


BP_RES BP_INDEX :: first (BP_ENTRY *entry)
{
	BP_BLOCK *block;

	if (!(block = new BP_BLOCK)) alloc_fail();
	bp_lock();
	get_block(block,index->root);

	if (!block->next) {				// Index entirely empty
	   bp_unlock();
	   delete block;
	   return (BP_EOIDX);
	}

	level = 0;
	block_ofs[0] = index->root;
	while (block->lower != OFS_NONE) {		// Lower level exists
	      entry_ofs[level] = -1;
	      level++;
	      block_ofs[level] = block->lower;
	      get_block(block,block->lower);		// Go there
	}

	entry_ofs[level] = 0;
	copy_entry(sys_entry,entry_ptr(block,0));
	systouser(entry);

	bp_unlock();
	delete block;
	return (BP_OK);
}/*first()*/


BP_RES BP_INDEX :: last (BP_ENTRY *entry)
{
	BP_BLOCK     *block = new BP_BLOCK;
	BP_SYS_ENTRY *ep;

	if (!block) alloc_fail();
	bp_lock();
	get_block(block,index->root);

	if (!block->next) {				// Index entirely empty
	   bp_unlock();
	   delete block;
	   return (BP_EOIDX);
	}

	level = 0;
	block_ofs[0] = index->root;
	while (entry_ofs[level] = last_entry(block),
	       ep = entry_ptr(block,entry_ofs[level]), ep->higher != OFS_NONE) {
	      level++;
	      block_ofs[level] = ep->higher;
	      get_block(block,ep->higher);		// Goto lower level
	}

	copy_entry(sys_entry,ep);
	systouser(entry);

	bp_unlock();
	delete block;
	return (BP_OK);
}/*last()*/


BP_RES BP_INDEX :: next (BP_ENTRY *entry)
{
	BP_BLOCK *block;

	if (!valid_info)
	   return (BP_FAIL);

	if (!(block = new BP_BLOCK)) alloc_fail();
	bp_lock();

	if (do_rescan) {
	   if (tree_exact(block,sys_entry) != BP_OK) {
	      bp_unlock();
	      delete block;
	      return (BP_FAIL);
	   }
	}
	else
	   get_block(block,block_ofs[level]);

	if (tree_next(block) != BP_OK) {
	   bp_unlock();
	   delete block;
	   last(entry);
	   return (BP_EOIDX);
	}

	copy_entry(sys_entry,entry_ptr(block,entry_ofs[level]));
	systouser(entry);

	bp_unlock();
	delete block;
	return (BP_OK);
}/*next()*/


BP_RES BP_INDEX :: prev (BP_ENTRY *entry)
{
	BP_BLOCK *block;
	FILE_OFS  file_ofs;

	if (!valid_info)
	   return (BP_FAIL);

	if (!(block = new BP_BLOCK)) alloc_fail();
	bp_lock();

	if (do_rescan) {
	   if (tree_exact(block,sys_entry) != BP_OK) {
	      bp_unlock();
	      delete block;
	      return (BP_FAIL);
	   }
	}
	else
	   get_block(block,block_ofs[level]);

	entry_ofs[level] = prev_entry(block,entry_ofs[level]);
	if (entry_ofs[level] < 0)
	   file_ofs = block->lower;
	else
	   file_ofs = entry_ptr(block,entry_ofs[level])->higher;

	while (file_ofs != OFS_NONE) {
	      get_block(block,file_ofs);
	      level++;
	      block_ofs[level] = file_ofs;
	      entry_ofs[level] = last_entry(block);
	      file_ofs = entry_ptr(block,entry_ofs[level])->higher;
	}

	if (entry_ofs[level] < 0) {
	   do {
	      if (!level) {
		 bp_unlock();
		 delete block;
		 first(entry);
		 return (BP_EOIDX);
	      }
	      level--;
	   } while (entry_ofs[level] < 0);
	   get_block(block,block_ofs[level]);
	}

	copy_entry(sys_entry,entry_ptr(block,entry_ofs[level]));
	systouser(entry);

	bp_unlock();
	delete block;
	return (BP_OK);
}/*prev()*/


BP_RES BP_INDEX :: find (BP_ENTRY *entry)
{
	BP_BLOCK *block;
	BP_RES	  res;

	if (!(block = new BP_BLOCK)) alloc_fail();
	bp_lock();
	usertosys(entry);

	res = tree_scan(block,sys_entry,true);
	if (res == BP_OK) {
	   copy_entry(sys_entry,entry_ptr(block,entry_ofs[level]));
	   systouser(entry);
	}

	bp_unlock();
	delete block;
	return (res);
}/*find()*/


BP_RES BP_INDEX :: locate (BP_ENTRY *entry)
{
	BP_BLOCK *block;
	BP_RES	  res;

	if (!(block = new BP_BLOCK)) alloc_fail();
	bp_lock();
	usertosys(entry);

	res = tree_scan(block,sys_entry,true);
	if (res == BP_FAIL)
	   res = tree_next(block);
	if (res == BP_OK) {
	   copy_entry(sys_entry,entry_ptr(block,entry_ofs[level]));
	   systouser(entry);
	}

	bp_unlock();
	delete block;
	return (res);
}/*locate()*/


BP_RES BP_INDEX :: exact (BP_ENTRY *entry)
{
	BP_BLOCK *block;
	BP_RES	  res;

	if (!(block = new BP_BLOCK)) alloc_fail();
	bp_lock();
	usertosys(entry);

	res = tree_exact(block,sys_entry);
	if (res == BP_OK) {
	   copy_entry(sys_entry,entry_ptr(block,entry_ofs[level]));
	   systouser(entry);
	}

	bp_unlock();
	delete block;
	return (res);
}/*exact()*/


BP_RES BP_INDEX :: add (BP_ENTRY *entry)
{
	BP_BLOCK *block;
	BP_RES	  res;

	if (!(block = new BP_BLOCK)) alloc_fail();
	bp_lock();
	usertosys(entry);

	res = tree_scan(block,sys_entry,false);
	if (res == BP_OK && !(index->flags & BP_DUPKEYS)) {
	   bp_unlock();
	   delete block;
	   return (BP_FAIL);
	}

	sys_entry->higher = OFS_NONE;
	tree_add(block,sys_entry);

	valid_info = true;
	bp_unlock();
	delete block;
	return (BP_OK);
}/*add()*/


BP_RES BP_INDEX :: del (BP_ENTRY *entry)
{
	BP_BLOCK *block;
	BP_RES	  res;
	FILE_OFS  file_ofs;
	boolean   combine;

	if (!(block = new BP_BLOCK)) alloc_fail();
	bp_lock();
	usertosys(entry);

	res = tree_exact(block,sys_entry);
	if (res != BP_OK) {				// No exact match found
	   bp_unlock();
	   delete block;
	   return (res);
	}

	if ((file_ofs = entry_ptr(block,entry_ofs[level])->higher) != OFS_NONE) {
	   BP_LEVEL	save_level = level, leaf_level;
	   BP_SYS_ENTRY work_entry;

	   do {
	      level++;
	      block_ofs[level] = file_ofs;
	      get_block(block,file_ofs);
	      entry_ofs[level] = -1;
	   } while ((file_ofs = block->lower) != OFS_NONE);

	   entry_ofs[level] = 0;
	   copy_entry(&work_entry,entry_ptr(block,entry_ofs[level]));
	   leaf_level = level;
	   level = save_level;
	   tree_replace(block,&work_entry);
	   level = leaf_level;
	}

	do {
	   get_block(block,block_ofs[level]);
	   tree_delete(block);
	   if (!level && !block->next) {		// Root block, empty
	      if (block->lower != OFS_NONE) {		// Different root block
		 file_ofs = index->root;		// Pos of now empty root
		 index->root = block->lower;		// We changed hdr!
		 file->set_free(file_ofs);		// Writes header too
	      }
	      break;
	   }

	   combine = ((block->next < (BP_BLOCKSPACE / 2)) && (level > 0)) ?
		     true : false;
	   if (combine)
	      combine = tree_combine(block);
	} while (combine);

	bp_unlock();
	delete block;
	return (BP_OK);
}/*del()*/


/* ------------------------------------------------------------------------- */
BP_RES BP_INDEX :: tree_scan (BP_BLOCK *block,
			      BP_SYS_ENTRY *entry, boolean find)
{
	BP_ENTOFS  cur, next, save_cur;
	FILE_OFS   file_ofs;
	int	   way;
	BP_LEVEL   save_level;
	boolean    saved = false;

	level = -1;
	file_ofs = index->root;

	while (file_ofs != OFS_NONE) {
	      level++;
	      block_ofs[level] = file_ofs;
	      get_block(block,file_ofs);

	      cur = -1;
	      next = next_entry(block,cur);
	      way = -1;

	      while (next < block->next) {
		    way = cmp_entry(entry,entry_ptr(block,next),cmpfunc);
		    if (way <= 0) break;
		    cur = next;
		    next = next_entry(block,cur);
	      }

	      if (!way) {
		 cur = next;

		 if (!(index->flags & BP_DUPKEYS)) {
		    entry_ofs[level] = cur;
		    break;
		 }
		 else if (find) {
		    save_level = level;
		    save_cur   = cur;
		    saved      = true;
		 }
	      }

	      file_ofs = (cur < 0) ? block->lower : entry_ptr(block,cur)->higher;
	      entry_ofs[level] = cur;
	}

	if (saved) {
	   if (way) {
	      level = save_level;
	      get_block(block,block_ofs[save_level]);
	      way = 0;
	   }
	   entry_ofs[save_level] = save_cur;
	}

	return (!way ? BP_OK : BP_FAIL);
}/*tree_scan()*/


BP_RES BP_INDEX :: tree_exact (BP_BLOCK *block, BP_SYS_ENTRY *entry)
{
	BP_SYS_ENTRY *ep;
	BP_RES	      res;

	res = tree_scan(block,entry,true);
	if (res != BP_OK || !(index->flags & BP_DUPKEYS))
	   return (res);

	do {
	   ep = entry_ptr(block,entry_ofs[level]);
	   if (entry->datarec == ep->datarec)
	      return (BP_OK);
	} while (tree_next(block) != BP_EOIDX &&
		 !cmp_entry(entry,ep,cmpfunc));

	return (BP_FAIL);
}/*tree_exact()*/


BP_RES BP_INDEX :: tree_next (BP_BLOCK *block)
{
	FILE_OFS file_ofs;

	if (entry_ofs[level] < 0)
	   file_ofs = block->lower;
	else {
	   if (entry_ofs[level] == block->next)
	      file_ofs = OFS_NONE;
	   else
	      file_ofs = entry_ptr(block,entry_ofs[level])->higher;
	}

	while (file_ofs != OFS_NONE) {
	      get_block(block,file_ofs);
	      level++;
	      block_ofs[level] = file_ofs;
	      entry_ofs[level] = -1;
	      file_ofs = block->lower;
	}

	while ((entry_ofs[level] =
		next_entry(block,entry_ofs[level])) == block->next) {
	      if (!level)
		 return (BP_EOIDX);
	      level--;
	      get_block(block,block_ofs[level]);
	}

	return (BP_OK);
}/*tree_next()*/


void BP_INDEX :: tree_add (BP_BLOCK *block, BP_SYS_ENTRY *entry)
{
	BP_SYS_ENTRY insert_entry, passup_entry;
	boolean      find_again = false;

	copy_entry(&insert_entry,entry);

	while (1) {
	      entry_ofs[level] = next_entry(block,entry_ofs[level]);
	      if (block->next + entry_size(&insert_entry) <= BP_BLOCKSPACE) {
		 tree_insert(block,&insert_entry);
		 break;
	      }
	      else {
		 find_again = true;
		 tree_split(block,&insert_entry,&passup_entry);
		 copy_entry(&insert_entry,&passup_entry);
		 level--;
		 if (level < 0) {
		    BP_LEVEL i;

		    for (i = BP_MAXLEVELS; i > 0; ) {
			i--;
			block_ofs[i] = block_ofs[i - 1];
			entry_ofs[i] = entry_ofs[i - 1];
		    }

		    block->lower = index->root; 	// Link in old root
		    copy_entry(entry_ptr(block,0),&insert_entry);// Insert entry
		    block->next = entry_size(&insert_entry);// Only 1 in new root
		    index->root = file->get_free();	// Make new root block
		    file->flag_changed_header();
		    put_block(block,index->root);	// Write the new root
		    level = 0;
		    block_ofs[level] = index->root;
		    entry_ofs[level] = 0;
		    break;
		 }
		 get_block(block,block_ofs[level]);
	      }
	}

	if (find_again)
	   tree_scan(block,entry,false);
}/*tree_add()*/


void BP_INDEX :: tree_split (BP_BLOCK *block,
			     BP_SYS_ENTRY *insert_entry, BP_SYS_ENTRY *passup_entry)
{
	BP_BLOCK      *new_block;
	BP_ENTOFS      half, insert_pos, size;
	BP_SYS_ENTRY  *ep;

	if (!(new_block = new BP_BLOCK)) alloc_fail();

	insert_pos = entry_ofs[level];
	half	   = prev_entry(block,block->next / 2 + sizeof (FILE_OFS));
	entry_ofs[level] = half;
	if (half == insert_pos)
	   copy_entry(passup_entry,insert_entry);
	else {
	   ep = entry_ptr(block,half);
	   copy_entry(passup_entry,ep);
	   size = entry_size(passup_entry);
	   memmove(ep,entry_ptr(block,half + size),block->next - (half + size));
	   block->next -= size;
	}

	new_block->next = block->next - half;
	memmove(entry_ptr(new_block,0),entry_ptr(block,half),new_block->next);
	block->next = half;
	new_block->lower = passup_entry->higher;
	passup_entry->higher = file->get_free();

	if (insert_pos < half) {
	   entry_ofs[level] = insert_pos;
	   tree_insert(block,insert_entry);
	   put_block(new_block,passup_entry->higher);
	}
	else {
	   put_block(block,block_ofs[level]);
	   if (insert_pos > half) {
	      insert_pos -= entry_size(passup_entry);
	      block_ofs[level] = passup_entry->higher;
	      entry_ofs[level] = insert_pos - half;
	      tree_insert(new_block,insert_entry);
	   }
	   else
	      put_block(new_block,passup_entry->higher);
	}

	delete new_block;
}/*tree_split()*/


boolean BP_INDEX :: tree_combine (BP_BLOCK *block)
{
	FILE_OFS      save_ofs, file_ofs;
	BP_BLOCK     *work_block = new BP_BLOCK;
	BP_ENTOFS     size, save_cur, cur, save_size;
	BP_SYS_ENTRY  save_entry;
	boolean       combine = false;

	if (!work_block) alloc_fail();

	save_ofs = block_ofs[level],
	size	 = block->next;
	level--;
	save_cur = entry_ofs[level];

	get_block(block,block_ofs[level]);
	if ((cur = entry_ofs[level] =
	     next_entry(block,save_cur)) < block->next) {	// Comb. right

	   if ((entry_size(entry_ptr(block,cur)) + size) < BP_BLOCKSPACE) {
	      copy_entry(&save_entry,entry_ptr(block,cur));
	      file_ofs = entry_ptr(block,cur)->higher;
	      level++;
	      block_ofs[level] = file_ofs;
	      get_block(work_block,file_ofs);
	      block_ofs[level] = save_ofs;
	      get_block(block,save_ofs);
	      save_size = entry_size(&save_entry);
	      if (((block->next + work_block->next + save_size) >= BP_BLOCKSPACE) &&
		  (work_block->next <= block->next + save_size)) {
		 delete work_block;
		 return (combine);
	      }

	      save_entry.higher = work_block->lower;
	      entry_ofs[level] = block->next;
	      tree_insert(block,&save_entry);
	      if ((block->next + work_block->next) < BP_BLOCKSPACE) {
		 memmove(entry_ptr(block,block->next),
			 entry_ptr(work_block,0),
			 work_block->next);
		 block->next += work_block->next;
		 put_block(block,save_ofs);
		 file->set_free(file_ofs);
		 level--;
		 combine = true;
	      }
	      else {
		 BP_SYS_ENTRY *ep = entry_ptr(work_block,0);

		 copy_entry(&save_entry,ep);
		 save_size = entry_size(&save_entry);
		 memmove(ep,entry_ptr(work_block,save_size),
			 work_block->next - save_size);
		 work_block->next -= save_size;
		 work_block->lower = save_entry.higher;
		 put_block(work_block,file_ofs);
		 level--;
		 tree_replace(block,&save_entry);
	      }
	   }
	}
	else {							// Comb. left
	   if ((entry_size(entry_ptr(block,entry_ofs[level])) +
		size) < BP_BLOCKSPACE) {

	      copy_entry(&save_entry,entry_ptr(block,save_cur));
	      entry_ofs[level] = prev_entry(block,save_cur);
	      if (entry_ofs[level] < 0)
		 file_ofs = block->lower;
	      else
		 file_ofs = entry_ptr(block,entry_ofs[level])->higher;
	      level++;
	      block_ofs[level] = file_ofs;
	      get_block(work_block,file_ofs);
	      cur = entry_ofs[level] = last_entry(work_block);
	      block_ofs[level] = save_ofs;
	      get_block(block,save_ofs);
	      save_size = entry_size(&save_entry);
	      if (((block->next + work_block->next + save_size) >= BP_BLOCKSPACE) &&
		  (work_block->next <= (block->next + save_size))) {
		 delete work_block;
		 return (combine);
	      }

	      entry_ofs[level] = 0;
	      save_entry.higher = block->lower;
	      tree_insert(block,&save_entry);
	      if ((block->next + work_block->next) < BP_BLOCKSPACE) {
		 memmove(entry_ptr(work_block,work_block->next),
			 entry_ptr(block,0),
			 block->next);
		 work_block->next += block->next;
		 put_block(work_block,file_ofs);
		 file->set_free(save_ofs);
		 level--;
		 entry_ofs[level] = save_cur;
		 combine = true;
	      }
	      else {
		 block->lower = entry_ptr(work_block,cur)->higher;
		 copy_entry(&save_entry,entry_ptr(work_block,cur));
		 work_block->next = cur;
		 put_block(work_block,file_ofs);
		 level--;
		 entry_ofs[level] = save_cur;
		 tree_replace(block,&save_entry);
	      }
	   }
	}

	delete work_block;
	return (combine);
}/*tree_combine()*/


void BP_INDEX :: tree_replace (BP_BLOCK *block, BP_SYS_ENTRY *entry)
{
	get_block(block,block_ofs[level]);
	entry->higher = entry_ptr(block,entry_ofs[level])->higher;
	tree_delete(block);
	entry_ofs[level] = prev_entry(block,entry_ofs[level]);
	tree_add(block,entry);
}/*tree_replace()*/


void BP_INDEX :: tree_insert (BP_BLOCK *block, BP_SYS_ENTRY *entry)
{
	BP_ENTOFS      offset, size;
	BP_SYS_ENTRY  *ep;

	offset = entry_ofs[level];
	ep = entry_ptr(block,offset);
	size = entry_size(entry);
	memmove(entry_ptr(block,offset + size), ep, block->next - offset);
	copy_entry(ep,entry);
	block->next += size;

	put_block(block,block_ofs[level]);
}/*tree_insert()*/


void BP_INDEX :: tree_delete (BP_BLOCK *block)
{
	BP_ENTOFS     offset, size;
	BP_SYS_ENTRY *ep;

	offset = entry_ofs[level];
	ep = entry_ptr(block,offset);
	size = entry_size(ep);
	memmove(ep, entry_ptr(block,offset + size),
		    block->next - (offset + size));
	block->next -= size;

	put_block(block,block_ofs[level]);
}/*tree_delete()*/


/* ------------------------------------------------------------------------- */
BP_ENTOFS BP_INDEX :: prev_entry (BP_BLOCK *block, BP_ENTOFS offset)
{
	BP_ENTOFS previous = -1, current = 0;

	while (current < offset) {
	      previous = current;
	      current += entry_size(entry_ptr(block,current));
	}

	return (previous);
}/*prev_entry()*/


BP_ENTOFS BP_INDEX :: next_entry (BP_BLOCK *block, BP_ENTOFS offset)
{
	if (offset < 0)
	   offset = 0;
	else if (offset < block->next)
	   offset += entry_size(entry_ptr(block,offset));

	return (offset);
}/*next_entry()*/


BP_ENTOFS BP_INDEX :: last_entry (BP_BLOCK *block)
{
	return (prev_entry(block,block->next));
}/*last_entry()*/


/* ------------------------------------------------------------------------- */
void BP_INDEX :: usertosys (BP_ENTRY *user_entry)
{
	register byte *p;
	register byte len, i, keysize;

	sys_entry->datarec = user_entry->datarec;
	keysize = 0;
	p = key_start(sys_entry);
	for (i = 0; i < index->keys; i++) {
	    len = strlen(user_entry->key[i]) + 1;
	    memmove(p,user_entry->key[i],len);
	    keysize += len;
	    p += len;
	}
	set_keysize(sys_entry,keysize);
}/*usertosys()*/


void BP_INDEX :: systouser (BP_ENTRY *user_entry)
{
	register byte *p;
	register byte len, i;

	user_entry->datarec = sys_entry->datarec;
	p = key_start(sys_entry);
	for (i = 0; i < index->keys; i++) {
	    len = strlen((char *) p) + 1;
	    memmove(user_entry->key[i],p,len);
	    p += len;
	}

	valid_info = true;
}/*systouser()*/


/* end of bpindex.cpp */
