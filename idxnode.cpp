/*
	B+Tree NodeList Indexing routines - just a test exercise
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
#include <stdlib.h>
#include <conio.h>
#include <process.h>
#include <string.h>
#include <ctype.h>
#include "bpindex.h"


static FILE_CACHE  cache(30);
static BP_FILE	   idx_file;
static BP_INDEX    zone_idx, net_idx, node_idx;
static BP_ENTRY    entry;
static char	   buffer[256];
static const char *idxfname = "IDXNODE.IDX";
static FILE	  *fp;
static char	  *name, *city, *sysop, *phone, *flags;


void getnode(void)
{
	register char *p;

	fseek(fp,entry.datarec,SEEK_SET);
	fgets(buffer,255,fp);
	for (p = strchr(buffer,'_'); p; p = strchr(p + 1,'_')) *p = ' ';
	name  = strtok(strchr(strchr(buffer,',') + 1,',') + 1,",");
	city  = strtok(NULL,",");
	sysop = strtok(NULL,",");
	phone = strtok(NULL,",");
	flags = strtok(NULL,"\n");
}


void main (int argc, char *argv[])
{
	FILE_OFS  filepos;
	int	  zone, net, node;
	word	  zones, regions, nets, hubs, holds, nodes;
	BP_RES	  res;
	boolean   create = false;
	long	  starttime, endtime;
	int	  i;

	directvideo = 0;

	if (argc != 2) {
	   cprintf("Usage: IDXNODE <nodelist>\r\n");
	   cprintf("Index is automatically created if non-existent\r\n");
	   exit (1);
	}

	if ((fp = fopen(argv[1],"rt")) == NULL) {
	   cprintf("! Can't open nodelist file %s\r\n",argv[1]);
	   exit (1);
	}

	if (access(idxfname,0))
	   create = true;

	if (!idx_file.open(&cache,idxfname)) {
	   cprintf("! Can't open/create index file %s or different format version\r\n",idxfname);
	   exit (1);
	}
	if (!zone_idx.init(&idx_file,"zone",1,BP_NONE)) {
	   cprintf("! Can't init index 'zone' within file %s\r\n",idxfname);
	   exit (1);
	}
	if (!net_idx.init(&idx_file,"net",2,BP_NONE)) {
	   cprintf("! Can't init index 'net' within file %s\r\n",idxfname);
	   exit (1);
	}
	if (!node_idx.init(&idx_file,"node",3,BP_NONE)) {
	   cprintf("! Can't init index 'node' within file %s\r\n",idxfname);
	   exit (1);
	}

	if (create) {
	   cprintf("* Creating index '%s' from nodelist '%s'\r\n",idxfname,argv[1]);
	   filepos = 0L;
	   zone = net = node = 0;
	   zones = regions = nets = hubs = holds = nodes = 0;
	   starttime = time(NULL);
	   while (fgets(buffer,255,fp)) {
		 entry.datarec = filepos;
		 filepos = ftell(fp);
		 if (buffer[0] == ';') continue;
		 if	 (buffer[0] == ',') {
		    node = atoi(&buffer[1]);
		 }
		 else if (!strncmp(buffer,"Hold",4)) {
		    node = atoi(&buffer[5]);
		    holds++;
		 }
		 else if (!strncmp(buffer,"Hub",3)) {
		    node = atoi(&buffer[4]);
		    hubs++;
		 }
		 else if (!strncmp(buffer,"Host",4)) {
		    net = atoi(&buffer[5]); node = 0;
		    nets++;
		 }
		 else if (!strncmp(buffer,"Region",6)) {
		    net = atoi(&buffer[7]); node = 0;
		    regions++;
		 }
		 else if (!strncmp(buffer,"Zone",4)) {
		    zone = net = atoi(&buffer[5]); node = 0;
		    zones++;
		    cprintf("Zone: %d   \r\n",zone);
		 }
		 /* Leftover: DOWN and KENL, neither can be sent any mail to */
		 cprintf("%d/%d        \r",net,node);
		 itoa(zone,entry.key[0],10);
		 itoa(net,entry.key[1],10);
		 itoa(node,entry.key[2],10);
		 if (!node) {
		    if (zone == net)
		       zone_idx.add(&entry);
		    net_idx.add(&entry);
		 }
		 nodes++;
		 node_idx.add(&entry);
	   }
	   endtime = time(NULL);
	   cprintf("+ Complete nodelist indexed in %ld:%02ld minutes\r\n",
		  (endtime - starttime) / 60, (endtime - starttime) % 60);
	   cprintf("  Zones  : %u\r\n  Regions: %u\r\n  Nets   : %u\r\n"
		   "  Hubs   : %u\r\n  Holds  : %u\r\n  Nodes  : %u\r\n",
		   zones, regions, nets, hubs, holds, nodes);
	}

	cprintf("= Operating on %s/%s\r\n",idxfname,argv[1]);

	zone = net = node = 0;
	while (1) {
	      itoa(zone,entry.key[0],10);
	      itoa(net,entry.key[1],10);
	      itoa(node,entry.key[2],10);
	      if (node_idx.find(&entry) == BP_OK) {
		 getnode();
		 cprintf("> Selection: %s, %s (%d:%d/%d)\r\n",
			 name,city,zone,net,node);
		 cprintf("  sysop=%s, phone=%s, flags=%s\r\n",
			 sysop,phone,flags);
	      }
	      else {
		 cprintf("> Selection: No listed node (%d:%d/%d)\r\n",
			 zone,net,node);
	      }
	      cprintf("  [Q]uit, Enter (partial) nodenumber, # for list: ");
	      fgets(buffer,128,stdin);
	      if (toupper(*buffer) == 'Q')
		 break;
	      else if (sscanf(buffer,"%d:%d/%d",&i,&i,&i) == 3)
		 sscanf(buffer,"%d:%d/%d",&zone,&net,&node);
	      else if (sscanf(buffer,"%d/%d",&i,&i) == 2)
		 sscanf(buffer,"%d/%d",&net,&node);
	      else if (strstr(buffer,"#:")) {			// Zone
		 zone = net = node = 0;
		 cprintf("= List of zones\r\n");
		 res = zone_idx.first(&entry);
		 while (res == BP_OK) {
		       getnode();
		       cprintf("  %-5s  %s, %s (ZC=%s)\r\n",
			       entry.key[0],name,city,sysop);
		       res = zone_idx.next(&entry);
		 }
	      }
	      else if (strstr(buffer,":#")) {			// Net
		 sscanf(buffer,"%d:#",&zone);
		 net = zone; node = 0;
		 goto listnets;
	      }
	      else if (*buffer == '/') {			// Net
		 net = zone;
		 node = 0;
listnets:	 cprintf("= List of nets in zone %d\r\n",zone);
		 res = net_idx.first(&entry);
		 while (res == BP_OK && atoi(entry.key[0]) != zone)
		       res = net_idx.next(&entry);
		 while (res == BP_OK && atoi(entry.key[0]) == zone) {
		       getnode();
		       cprintf("  %-5s  %s, %s (*C=%s)\r\n",
			       entry.key[1],name,city,sysop);
		       res = net_idx.next(&entry);
		 }
	      }
	      else if (strstr(buffer,"/#")) {			// Node
		 if (strchr(buffer,':'))
		    sscanf(buffer,"%d:%d/#",&zone,&net);
		 else
		    sscanf(buffer,"%d/#",&net);
		 node = 0;
		 goto listnodes;
	      }
	      else if (sscanf(buffer,"%d:%d",&i,&i) == 2) {
		 sscanf(buffer,"%d:%d",&zone,&net);
		 node = 0;
	      }
	      else if (*buffer == '#') {			// Node
		 node = 0;
listnodes:	 cprintf("= List of nodes in net %d:%d\r\n",zone,net);
		 itoa(zone,entry.key[0],10);
		 itoa(net,entry.key[1],10);
		 itoa(0,entry.key[2],10);
		 res = node_idx.find(&entry);
		 while (res == BP_OK && atoi(entry.key[0]) == zone &&
			atoi(entry.key[1]) == net) {
		       getnode();
		       cprintf("  %-5s  %s, %s (sysop=%s)\r\n",
			       entry.key[2],name,city,sysop);
		       res = node_idx.next(&entry);
		 }
	      }
	      else {
		 i = atoi(buffer);
		 if	 (!zone) { zone = i; net = node = 0; }
		 else if (!net)  { net	= i; node = 0; }
		 else		 node = i;
	      }
	}

	idx_file.close();
	fclose(fp);
	exit (0);
}


/* end of idxnode.cpp */
