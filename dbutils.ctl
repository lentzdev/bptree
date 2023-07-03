; Database & B+Tree Index Maintenance
; Design & COPYRIGHT (C) 1990,91 by A.G. Lentz & LENTZ SOFTWARE-DEVELOPMENT
;
;   This program is free software: you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, either version 3 of the License, or
;   (at your option) any later version.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program.  If not, see <https://www.gnu.org/licenses/>.
;
;------------------------------------------------------------------------------
;Base <PathName> NoCounter|Counter Shared|Exclusive
;      <PathName> Base path/filename database (.DAT) and index (.IDX) file.
;      NoCounter  Don't use record counter feature
;      Counter    Add unique sequence count in DB_COUNTER field if not yet in.
;      Shared     Database used for different record types, one index per type.
;      Exclusive  Database for one type of record, all indexes for each record.
;Index <Name> Unique|DupKeys Case|NoCase <Field> [<Field> ...]
;      Unique     No duplicate entries allowed in index.
;      DupKeys    Duplicate entries (all keys fields identical) allowed.
;      Case       Index entries are compared case sensitive.
;      NoCase     Index entries are compared without case sensitivity.
;      <Name>     Identifier of index, for shared bases also record identifier.
;      <Field>    Record field used for indexing.
;Maximum of 13 chars per indexname, 8 fields per index, 32 indexes per base.
;------------------------------------------------------------------------------
Base  BPTEST   NoCounter Exclusive
Index naam     Unique  NoCase  naam

Base  CONFIG   NoCounter Shared
Index General  Unique  NoCase  Task
Index Modem    Unique  NoCase  Type Comment
;Index Periods  Unique  NoCase  Period
;Index MsgAreas Unique  NoCase  MsgArea

Base  TELBOOK  NoCounter Exclusive
Index Phone    Unique  NoCase SystemPhone
Index Address  Unique  NoCase SystemAddress

Base  MESSAGES Counter Exclusive
Index MsgID    Unique  NoCase  MsgID
Index Number   Unique  NoCase  MsgArea DB_COUNTER Arrived
Index ReplyID  DupKeys NoCase  ReplyID
Index To       DupKeys NoCase  To      DB_COUNTER Received
;Internally, one extra special index could be used for duplicate detection:
;     DupBase  Unique  NoCase  MsgID
;The record pointer is filled with the value of the time 'Arrived' field
;Entries in this index would not be deleted when a msg is deleted, giving
;a longer time-span for detecting incoming duplicate messages...
;------------------------------------------------------------------------------
