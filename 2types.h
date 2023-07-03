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
#ifndef __2TYPES_DEF_
#define __2TYPES_DEF_


typedef unsigned char byte;
typedef unsigned int  word;
typedef unsigned long dword;

enum boolean { false, true };
#ifndef __cplusplus
typedef int boolean;
#endif

typedef long FILE_OFS;				/* Offset in a disk file     */
#ifdef __cplusplus
const FILE_OFS OFS_NONE = -1L;			/* Unused file offset ptr    */
#else
#define OFS_NONE (-1L)
#endif


#endif/*__2TYPES_DEF_*/


/* end of 2types.h */
