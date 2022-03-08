#
# To make the NVM Bios simulator, hit "make nvmsim.abs"
# To load it, do "aread nvmsim.abs"
# To change the address at which the "non-volatile RAM"
# sits, change the link command for nvmsim.abs; the
# non-volatile RAM goes where the data segment for
# nvmsim.abs goes (currently it's at the end of
# the ROMULATOR space)
#
# WARNING WARNING WARNING:
# don't change any link addresses without consulting
# `usage.txt' and Dave Staugas!
#

include $(JAGSDK)/tools/build/jagdefs.mk

INCLUDE = ./include
#CFLAGS = -O2 -Wall -mshort -fomit-frame-pointer -mpcrel -I$(INCLUDE)
CFLAGS = -O2 -Wall -mshort -fomit-frame-pointer -I$(INCLUDE)
FIXROM = ./fixrom

CARTOBJS = cartrom.o
#MANAGOBJS = manager.o arrowfnt.o alloc.o joyinp.o video.o logo.o
MANAGOBJS = manager.o arrowfnt.o alloc.o joyinp.o video.o olist.o font.o bltrect.o bltutil.o line.o\
	sprintf.o util.o memcpy.o memset.o atol.o qsort.o joypad.o strcmp.o strtol.o ctype.o
STANDOBJS = stand.o arrowfnt.o alloc.o joyinp.o video.o olist.o font.o bltrect.o bltutil.o line.o\
	sprintf.o util.o memcpy.o memset.o atol.o qsort.o joypad.o strcmp.o strtol.o ctype.o
MANAGFONTS = -ii lubs10.jft _medfnt -ii lubs12.jft _bigfnt -ii emlogo.img _emlogo
TESTOBJS = test.o romnvm.o

all: rom.abs nvmsim.rom

nvmsim.rom: jagrt2.o $(MANAGOBJS)
	aln -s -a 802000 x 5000 -o nvmsim.abs jagrt2.o $(MANAGOBJS) $(MANAGFONTS)
	$(FIXROM) nvmsim.abs
	mv nvmsim.abs nvmsim.rom

rom.abs: rom.s manager.abs
	mac -fb rom.s
	aln -s -a 802000 x 5000 -o rom.abs rom.o
	$(FIXROM) rom.abs

stand.abs: jagrt3.o $(STANDOBJS)
	aln -s -a 5000 x 20000 -o stand.abs jagrt3.o $(STANDOBJS) $(MANAGFONTS)
	filefix stand.abs
	rm -f stand.tx stand.dta stand.db
	$(FIXROM) stand.abs

manager.abs: jagrt.o $(MANAGOBJS) $(STANDOBJS)
	aln -s -a c0000 x d4000 -o manager.abs jagrt.o $(STANDOBJS) $(MANAGFONTS)
	filefix manager.abs
	rm -f manager.tx manager.dta manager.db
	$(FIXROM) manager.abs

test.abs: $(MANAGOBJS)
	aln -s -a 802000 x 5000 -o test.abs jagrt.o $(MANAGOBJS) $(MANAGFONTS)


nvmat.abs: nvmat.o
	aln -l -a 2400 9e0000 xt -o nvmat.abs nvmat.o
	filefix nvmat.abs
	rm -f nvmat.tx nvmat.dta nvmat.db
	$(FIXROM) nvmat.abs

nvmamd.abs: nvmamd.o
	aln -l -a 2400 9e0000 xt -o nvmamd.abs nvmamd.o
	filefix nvmamd.abs
	rm -f nvmamd.tx nvmamd.dta nvmamd.db
	$(FIXROM) nvmamd.abs

nvmrom.abs: nvmrom.o
	aln -l -a 2400 9e0000 xt -o nvmrom.abs nvmrom.o
	filefix nvmrom.abs
	rm -f nvmrom.tx nvmrom.dta nvmrom.db
	$(FIXROM) nvmrom.abs

testsim.abs: testsim.o
	aln -s -a 802000 x 5000 -o testsim.abs testsim.o

test: $(TESTOBJS)
	gcc -o test -mshort $(TESTOBJS)

test.o: test.c nvm.h
nvm.o: nvm.c nvm.h
jagrt.o: jagrt.s nvmamd.abs nvmrom.abs nvmat.abs
jagrt2.o: jagrt2.s nvmamd.abs nvmrom.abs nvmat.abs

nvm.inc: nvm.h makeinc.c
	gcc -o makeinc -Wall -O makeinc.c
	./makeinc

nvmat.o: nvmat.s asmnvm.s nvm.inc
	mac -fb nvmat.s

nvmamd.o: nvmamd.s asmnvm.s nvm.inc
	mac -fb nvmamd.s

nvmrom.o: nvmrom.s asmnvm.s nvm.inc
	mac -fb nvmrom.s

romulat.o: romulat.s
	mac -fb romulat.s

include $(JAGSDK)/tools/build/jagrules.mk
