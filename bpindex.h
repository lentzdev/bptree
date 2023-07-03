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
#ifndef __BPINDEX_DEF_
#define __BPINDEX_DEF_
#include <stdlib.h>
#include <string.h>
#include "file_io.h"

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif


typedef int BP_ENTOFS;				// Offset of entry in block
typedef int BP_LEVEL;				// Level variable

typedef int BP_FLAGS;	// ----------------------- Flags for open function
enum { BP_DUMMY   = -1,
       BP_NONE	  =  0, 			// No flags
       BP_DUPKEYS =  1, 			// Duplicate keys allowed
       BP_NOCASE  =  2, 			// Keys are case insensitive
};

struct	_bp_hdrbase {	// ----------------------- Part 1 of the index header
	word	 version;			// File format version number
	FILE_OFS free;				// Offset of first free block
	dword	 sharecnt;			// Used when file-sharing
};

struct	_bp_hdrindex {	// ----------------------- Part 2-n of the index header
	char	 nametag[14];			// Nametag of index
	BP_FLAGS flags; 			// Flags for this index (above)
	word	 keys;				// Number of fields for one key
	FILE_OFS root;				// Offset of root block in file
	dword	 sharecnt;			// Used when file-sharing
};

struct	_bp_block {	// ----------------------- Useful part of index block
	FILE_OFS  lower;			// Offset of lower/child block
	BP_ENTOFS next; 			// Key space used / next avail
};

enum { BP_VERSION      = 2,			// File format version number
       BP_MAXKEYSIZE   = 255,			// Maximum size of a key
       BP_MAXKEYFIELDS = 8,			// Maximum no. fields per key
       BP_BLOCKSIZE  = 1024,			// Size of index block record
       BP_BLOCKSPACE = BP_BLOCKSIZE - sizeof (_bp_block),// Key space in blocks
       BP_MAXLEVELS  = 8,			// Tree maximum n levels deep
       BP_MAXINDEX   = 32,			// Max no. indexes in one file
       BP_HDRFILL    = BP_BLOCKSIZE - (sizeof (_bp_hdrbase) +
				       (BP_MAXINDEX * sizeof (_bp_hdrindex)))
};

typedef byte BP_RES;
enum { BP_OK,					// Done ok, (exact) match found
       BP_FAIL, 				// Not found or no exact match
       BP_EOIDX 				// Key higher than any entry
};


/* ------------------------------------------------------------------------- */
struct	BP_HEADER : public _bp_hdrbase { // Index file base, same size as BLOCK
	struct _bp_hdrindex index[BP_MAXINDEX];
	char		    fill[BP_HDRFILL];
};

struct	BP_BLOCK : public _bp_block {			// Index file block
	char entries[BP_BLOCKSPACE];			// Entries & filler
};

struct	BP_SYS_ENTRY {	       /***SYSTEM STRUCT***/	// Index file entry
	FILE_OFS higher;				// Higher level child
	FILE_OFS datarec;				// Ref to data record
	byte	 key[1 + BP_MAXKEYSIZE];		// Key[0] & key space
};

struct	BP_ENTRY {	       /***USER STRUCT***/	// Index file entry
	FILE_OFS datarec;				// Ref to data record
	char	 key[BP_MAXKEYFIELDS][BP_MAXKEYSIZE];	// Array of key strings
};


/* ------------------------------------------------------------------------- */
class	BP_FILE {					// Index file class
	RAW_FILE    f;
	FILE_CACHE *cache;
	BP_HEADER  *header;				// Header of index file
	boolean     changed_header,
		    did_write;

	void alloc_fail (void) { exit (255); }

	void get_header (void) {
		cache->read(&f,header,sizeof (BP_HEADER),0L);
	}
	void put_header (void) {
		cache->write(&f,header,sizeof (BP_HEADER),0L);
	}

public:  BP_FILE (void) { header = NULL; }		// Constructor
	~BP_FILE (void) { close(); }			// Destructor

	boolean open  (FILE_CACHE *cp, const char *pathname); // Open (+create)
	void	close (void);				// Close index file

	void bp_lock   (void);
	void bp_unlock (void);

	void get_block (BP_BLOCK *block, FILE_OFS offset) {
		cache->read(&f,block,sizeof (BP_BLOCK),offset);
	}
	void put_block (BP_BLOCK *block, FILE_OFS offset) {
		cache->write(&f,block,sizeof (BP_BLOCK),offset);
		did_write = true;
	}

	FILE_OFS get_free (void);
	void	 set_free (FILE_OFS offset);

	_bp_hdrindex *get_index (word index) { return (&(header->index[index])); }
	void flag_changed_header(void) { changed_header = true; }
};


/* ------------------------------------------------------------------------- */
class	BP_INDEX {					// Logical index class
	BP_FILE       *file;				// Ptr to bp_file class
	_bp_hdrindex  *index;				// Pointer to index hdr
	int	     (*cmpfunc) (const void *s1, const void *s2, word n);
	boolean        did_write;
	boolean        valid_info,			// If info below valid
		       do_rescan;			// Find entry again
	dword	       sharecnt;			// When file-sharing
	BP_SYS_ENTRY  *sys_entry;			// Index entry
	FILE_OFS       block_ofs[BP_MAXLEVELS]; 	// Block file pos
	BP_ENTOFS      entry_ofs[BP_MAXLEVELS]; 	// Entry offset
	BP_LEVEL       level;				// Current work level

	void alloc_fail (void) { exit (255); }

	void bp_lock   (void);
	void bp_unlock (void);

	void get_block (BP_BLOCK *block, FILE_OFS offset) {
		file->get_block(block,offset);
	}
	void put_block (BP_BLOCK *block, FILE_OFS offset) {
		file->put_block(block,offset);
		did_write = true;
	}

	BP_RES	tree_scan    (BP_BLOCK *block,
			      BP_SYS_ENTRY *entry, boolean find);
	BP_RES	tree_exact   (BP_BLOCK *block, BP_SYS_ENTRY *entry);
	BP_RES	tree_next    (BP_BLOCK *block);
	void	tree_add     (BP_BLOCK *block, BP_SYS_ENTRY *entry);
	void	tree_split   (BP_BLOCK *block,
			      BP_SYS_ENTRY *insert_entry, BP_SYS_ENTRY *passup_entry);
	boolean tree_combine (BP_BLOCK *block);
	void	tree_replace (BP_BLOCK *block, BP_SYS_ENTRY *entry);
	void	tree_insert  (BP_BLOCK *block, BP_SYS_ENTRY *entry);
	void	tree_delete  (BP_BLOCK *block);

	BP_ENTOFS prev_entry (BP_BLOCK *block, BP_ENTOFS offset);
	BP_ENTOFS next_entry (BP_BLOCK *block, BP_ENTOFS offset);
	BP_ENTOFS last_entry (BP_BLOCK *block);

	BP_SYS_ENTRY *entry_ptr (BP_BLOCK *block, BP_ENTOFS offset) {
		return ((BP_SYS_ENTRY *) (block->entries + offset));
	}
	byte	key_size (BP_SYS_ENTRY *entry) {
		return (*entry->key);
	}
	void	set_keysize (BP_SYS_ENTRY *entry, byte keysize) {
		*entry->key = keysize;
	}
	byte   *key_start (BP_SYS_ENTRY *entry) {
		return (entry->key + 1);
	}
	BP_ENTOFS entry_size (BP_SYS_ENTRY *entry) {
		return ((2 * sizeof (FILE_OFS)) +
			sizeof (byte) + key_size(entry));
	}

	void copy_entry (BP_SYS_ENTRY *dest, BP_SYS_ENTRY *src) {
		memmove(dest,src,entry_size(src));
	}

	int  cmp_entry (BP_SYS_ENTRY *entry1, BP_SYS_ENTRY *entry2,
			int (*cmpfunc) (const void *s1, const void *s2, word n)) {
		register int res;
		res = cmpfunc(key_start(entry1),key_start(entry2),
			      min(key_size(entry1),key_size(entry2)));
		return (res ? res :
			((int) key_size(entry1)) - ((int) key_size(entry2)));
	}

	void usertosys (BP_ENTRY *user_entry);
	void systouser (BP_ENTRY *user_entry);

public:  BP_INDEX (void) {				// Constructor
		if (!(sys_entry = new BP_SYS_ENTRY)) alloc_fail();
	}
	~BP_INDEX (void) {				// Destructor
		delete sys_entry;
	}

	boolean init	(BP_FILE *bpfp, const char *nametag,  // Init (+create)
			 word num_keys, BP_FLAGS bp_flags = BP_DUMMY);
	boolean destroy (BP_FILE *bpfp, const char *nametag); // Del idx in file

	BP_RES current (BP_ENTRY *entry);	// Current entry in index
	BP_RES first   (BP_ENTRY *entry);	// First entry in index
	BP_RES last    (BP_ENTRY *entry);	// Last entry in index
	BP_RES next    (BP_ENTRY *entry);	// Next entry from here /first
	BP_RES prev    (BP_ENTRY *entry);	// Prev entry from here /first
	BP_RES find    (BP_ENTRY *entry);	// First matching entry
	BP_RES locate  (BP_ENTRY *entry);	// First matching/higher
	BP_RES exact   (BP_ENTRY *entry);	// Find exact entry
	BP_RES add     (BP_ENTRY *entry);	// Add a new entry
	BP_RES del     (BP_ENTRY *entry);	// Delete an entry
};


#endif/*__BPINDEX_DEF_*/


/* end of BPINDEX.h */
