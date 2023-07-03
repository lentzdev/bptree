.RESPONSE_LINK: tlink link
MODEL	= l
CFLAGS	= -w+ -m$(MODEL) -O -G -Z -f- -d
#CFLAGS  = -w+ -m$(MODEL) -O -G -Z -f- -d -a+
#CFLAGS  = -w+ -m$(MODEL) -O -G -Z -f- -d -N
COPTS	= -DDOS_SHARE=0 -D__PROTO__
LFLAGS	= /x/c/d
LIBDIR	= \bc\lib
STARTUP = $(LIBDIR)\c0$(MODEL).obj
LIBS	= $(LIBDIR)\c$(MODEL).lib


DBUTILS  = dbutils.obj bpindex.obj dbtext.obj file_io.obj
SHOWTREE = showtree.obj bpindex.obj file_io.obj
IDXNODE  = idxnode.obj bpindex.obj file_io.obj
TEST	 = test.obj bpindex.obj file_io.obj
BPTEST	 = bptest.obj bpindex.obj dbtext.obj file_io.obj

COBJS	= $(DBUTILS) $(SHOWTREE) $(IDXNODE) $(TEST) $(BPTEST)


all:	dbutils.exe showtree.exe idxnode.exe test.exe bptest.exe


dbutils.exe: $(DBUTILS)
	     tlink $(LFLAGS) $(STARTUP) $(DBUTILS),$*.exe,nul,$(LIBS)

showtree.exe: $(SHOWTREE)
	     tlink $(LFLAGS) $(STARTUP) $(SHOWTREE),$*.exe,nul,$(LIBS)

idxnode.exe: $(IDXNODE)
	     tlink $(LFLAGS) $(STARTUP) $(IDXNODE),$*.exe,nul,$(LIBS)

test.exe:    $(TEST)
	     tlink $(LFLAGS) $(STARTUP) $(TEST),$*.exe,nul,$(LIBS)

bptest.exe:  $(BPTEST)
	     tlink $(LFLAGS) $(STARTUP) $(BPTEST),$*.exe,nul,$(LIBS)


$(COBJS):    $*.cpp
	     bcc -c -P $(CFLAGS) $(COPTS) $*.cpp

$(COBJS):     2types.h
dbutils.obj:  bpindex.h dbtext.h file_io.h
showtree.obj: bpindex.h file_io.h
idxnode.obj:  bpindex.h file_io.h
test.obj:     bpindex.h file_io.h
bptest.obj:   bpindex.h dbtext.h file_io.h
bpindex.obj:  bpindex.h file_io.h
dbtext.obj:   dbtext.h file_io.h
file_io.obj:  file_io.h
