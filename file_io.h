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
#ifndef __FILE_IO_DEF_
#define __FILE_IO_DEF_
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include "2types.h"


#undef sopen
inline int  raw_open   (const char *path, int access, int mode)
	{ return (open(path, access, mode)); }
inline FILE_OFS raw_tell   (int handle)
	{ return (tell(handle)); }
inline int  raw_lock   (int handle, FILE_OFS offset, FILE_OFS length)
	{ return (lock(handle, offset, length)); }
inline int  raw_unlock (int handle, FILE_OFS offset, FILE_OFS length)
	{ return (unlock(handle, offset, length)); }


/* ------------------------------------------------------------------------- */
boolean share_active (void);  /* Return true if DOS' SHARE.EXE active (flag) */


/* ------------------------------------------------------------------------- */
class RAW_FILE {
protected:
	int	 fd;
	FILE_OFS seek_offset;
	int	 seek_fromwhere;

	void do_seek (void)
	{
		if (seek_fromwhere >= 0) {
		   lseek(fd, seek_offset, seek_fromwhere);
		   seek_fromwhere = -1;
		}
	}

public:  RAW_FILE (void)
	{
		fd = -1;
	}
	~RAW_FILE (void)
	{
		close();
	}

enum { o_rdonly    = 1,
       o_wronly    = 2,
       o_rdwr	   = 4,
};
enum { o_noinherit = 0x0080
};
enum { o_append    = 0x0800,
       o_creat	   = 0x0100,
       o_trunc	   = 0x0200,
       o_excl	   = 0x0400
};
enum { s_iwrite    = 0x0080,
       s_iread	   = 0x0100
};
enum { sh_denyrw   = 0x0010,
       sh_denywr   = 0x0020,
       sh_denyrd   = 0x0030,
       sh_denyno   = 0x0040,
       sh_denynone = sh_denyno
};
enum { seek_set    = 0,
       seek_cur    = 1,
       seek_end    = 2
};

	boolean open  (const char *path, int access, word mode = 0)
	{
		close();
		fd = raw_open(path, access | O_BINARY, mode);
		return ((fd != -1) ? true : false);
	}
	boolean sopen (const char *path, int access, int shflag, word mode = 0)
	{
		return (open(path,access | (share_active() ? shflag : 0), mode));
	}
	void	close (void)
	{
		if (fd >= 0) {
		   _close(fd);
		   fd = -1;
		}
		seek_fromwhere = -1;
	}
	int	read (void *buf, word len)
	{
		do_seek();
		register int res = _read(fd, buf, len);
		if (res > 0) decode((byte *) buf,len);
		return (res);
	}
	int	write (void *buf, word len)
	{
		do_seek();
		register int res;
		if (len > 0) encode((byte *) buf,len);
		res = _write(fd, buf, len);
		if (len > 0) decode((byte *) buf,len);
		return (res);
	}
	void	seek (FILE_OFS offset, int fromwhere)
	{
		if (fromwhere == seek_cur && fromwhere == seek_fromwhere)
		   seek_offset += offset;  // Cur relative - Cute optimization!
		else {
		   do_seek();
		   seek_offset = offset;
		   seek_fromwhere = fromwhere;
		}
	}
	FILE_OFS tell (void)
	{
		do_seek();
		return (raw_tell(fd));
	}
	FILE_OFS length (void)
	{
		return (filelength(fd));
	}
	boolean lock (FILE_OFS offset, FILE_OFS length)
	{
		if (share_active())
		   return (!raw_lock(fd,offset,length) ? true : false);
		else
		   return (true);
	}
	boolean unlock (FILE_OFS offset, FILE_OFS length)
	{
		if (share_active())
		   return (!raw_unlock(fd,offset,length) ? true : false);
		else
		   return (true);
	}

private:
	virtual void encode (byte *buf, word len) { buf = buf; len = len; };
	virtual void decode (byte *buf, word len) { buf = buf; len = len; };
};


/* ------------------------------------------------------------------------- */
class XOR_FILE : public RAW_FILE {
private:
	virtual void encode (register byte *buf, register word len);
	virtual void decode (register byte *buf, register word len);
};


/* ------------------------------------------------------------------------- */
class FILE_CACHE {
	word use_counter;
	word poolsize;
	struct CACHE_BLOCK {
		boolean   dirty;
		word	  last_used;
		RAW_FILE *f;
		FILE_OFS  filepos;
		word	  length;
		byte	 *data;

		 CACHE_BLOCK (void) { f = NULL; }
		~CACHE_BLOCK (void) { if (f) delete data; }
	} *pool;

	void alloc_fail (void) { exit (255); }

public:  FILE_CACHE (word num_blocks);
	~FILE_CACHE (void);
	FILE_CACHE *cache_protect_prev;
	FILE_CACHE *cache_protect_next;

	void	 clear	(void); 		// Flush, then clear all blocks
	void	 clear	(RAW_FILE *f);		// Ditto, but only spec file
	void	 clear	(RAW_FILE *f, FILE_OFS filepos); // Specific file&block
	void	 flush	(void); 		// Flush dirty blocks to disk
	void	 flush	(RAW_FILE *f);		// Ditto, but only spec file
	void	 flush	(RAW_FILE *f, FILE_OFS filepos); // Specific file&block
	FILE_OFS length (RAW_FILE *f);		// Smart ret correct filelength
	void read   (RAW_FILE *f, void *buf, word len, FILE_OFS filepos);
	void write  (RAW_FILE *f, void *buf, word len, FILE_OFS filepos);

private:
	void clear (CACHE_BLOCK *block);	// Flush, then clear spec.block
	void flush (CACHE_BLOCK *block);	// Flush specific block to disk
	CACHE_BLOCK *find (RAW_FILE *f, FILE_OFS filepos);
	void make  (RAW_FILE *f, void *buf, word len, FILE_OFS filepos, boolean writing);
};



#endif/*__FILE_IO_DEF_*/


/* end of file_io.h */
