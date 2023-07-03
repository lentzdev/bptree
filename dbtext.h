/* ------------------------------------------------------------------------- */
/* Pigeon Post (WAITS)	-  Database functions with free-format ASCII records */
/* Design & COPYRIGHT (C) 1990,91 by A.G. Lentz & LENTZ SOFTWARE-DEVELOPMENT */
/* ------------------------------------------------------------------------- */
#ifndef __DBTEXT_DEF_
#define __DBTEXT_DEF_
#include <stdlib.h>
#include "file_io.h"


typedef int DB_FLAGS;	// ----------------------- Flags for open function
enum { DB_DUMMY   = -1,
       DB_NONE	  =  0, 			// No flags
       DB_COUNTER =  1, 			// Use counter facility
};

struct	_db_header {	// ----------------------- Data file database header
	word	 version;			// File format version number
	FILE_OFS free;				// Offset of first free block
	dword	 sharecnt;			// Used when file-sharing
	DB_FLAGS flags; 			// Option flags (see above)
	dword	 counter;			// Used for DB_COUNTER field
};

struct	_db_block {	// ----------------------- Useful part of text block
	FILE_OFS next;				// Offset of next block/endlink
};

enum { DB_VERSION    = 2,			// File format version number
       DB_BLOCKSIZE  = 256,			// Size of a database block
       DB_BLOCKSPACE = DB_BLOCKSIZE - sizeof (_db_block),// Key space in blocks
       DB_MAXFIELDS  = 255
};


/* ------------------------------------------------------------------------- */
struct	DB_HEADER : public _db_header {  // D'base file base,same size as BLOCK
	char   fill[DB_BLOCKSIZE - sizeof (_db_header)];
};

struct	DB_BLOCK : public _db_block {			// Database file block
	char data[DB_BLOCKSPACE];			// Contents
};


struct	DB_WORK {
	FILE_OFS  start_ofs, file_ofs, prev_ofs;
	DB_BLOCK  block;
	char	 *ptr;
	int	  count;
};


/* ------------------------------------------------------------------------- */
class DB_RECORD {					// Database record
	struct _db_field {
		char *tag;				// Name tag of field
		char *val;				// Content of field
	} fields[DB_MAXFIELDS]; 			// Record's fields
	byte  num_fields,				// No. fields in use
	      cur_field;				// Current field ptr
	char *text_ptr; 				// Free text (user mem)
	long  text_len; 				// Length of text

	void alloc_fail (void) { exit (255); }

public:  DB_RECORD (void);				// Constructor
	~DB_RECORD (void);				// Destructor

	void clear (void);				// Clear all fields+txt

	char	*get (const char *tag = NULL);		// Get value, NULL=cur
	boolean  set (const char *val, const char *tag = NULL,
		      const char *oldval = NULL);	// Add/replace field
	boolean  del (const char *tag = NULL,		// Del field, NULL=cur
		      const char *oldval = NULL);

	char	*first (char *tag = NULL);		// Cur = first
	char	*last  (char *tag = NULL);		// Cur = last
	char	*next  (char *tag = NULL);		// Cur = next
	char	*prev  (char *tag = NULL);		// Cur = prev

	char *get_textptr (void) { return (text_ptr); } // Get ptr to user text
	long  get_textlen (void) { return (text_len); } // Get len of user text
	void  set_text	  (char *text, long len = 0L)	// Set usertext ptr/len
		{ text_ptr = text; text_len = len; }
	void set_textlen (long len) { text_len = len; }
};


/* ------------------------------------------------------------------------- */
class DB_TEXT {
	RAW_FILE    f;					// Database file class
	FILE_CACHE *cache;
	DB_HEADER  *header;
	boolean     changed_header,
		    did_write;

	void work_init (DB_WORK *work)
	{
		work->prev_ofs	 = OFS_NONE;
		memset(&(work->block), 0, sizeof (DB_BLOCK));
		work->block.next = OFS_NONE;
		work->ptr	 = work->block.data;
		work->count	 = DB_BLOCKSPACE;
	}
	void work_update (DB_WORK *work, char c)
	{
		*work->ptr++ = c;
		work->count--;
		if (work->count <= 0)
		   work_flush(work);
	}
	void work_flush (DB_WORK *work);
	FILE_OFS work_finish (DB_WORK *work)
	{
		if (work->count < DB_BLOCKSPACE)
		   work_flush(work);
		return (work->start_ofs);
	}

	void db_lock	(void);
	void db_unlock	(void);
	void alloc_fail (void) { exit (255); }

	void get_header (void);
	void put_header (void);
	void get_block	(DB_BLOCK *block, FILE_OFS offset);
	void put_block	(DB_BLOCK *block, FILE_OFS offset);

	FILE_OFS get_free (void);
	void	 set_free (FILE_OFS offset);

public:  DB_TEXT (void) { header = NULL; }			// Constructor
	~DB_TEXT (void) { close(); }				// Destructor
	boolean open  (FILE_CACHE *cp, const char *pathname,  // Open (+create)
		       DB_FLAGS db_flags = DB_DUMMY);
	void	close (void);				// Close database file

	void	 get (DB_RECORD *record, FILE_OFS offset);	// Get record
	FILE_OFS put (DB_RECORD *record, FILE_OFS offset = OFS_NONE); // Putrec
	void	 del (FILE_OFS offset); 			// Del record
};


#endif/*__DBTEXT_DEF_*/


/* end of dbtext.h */
