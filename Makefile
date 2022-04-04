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

COMMONOBJS = arrowfnt.o alloc.o joyinp.o video.o  olist.o font.o bltrect.o \
	bltutil.o line.o sprintf.o util.o memcpy.o memset.o atol.o qsort.o \
	joypad.o strcmp.o strtol.o ctype.o

MANAGOBJS = manager.o
STANDOBJS = stand.o
MANAGFONTS = -ii lubs10.jft _medfnt -ii lubs12.jft _bigfnt -ii emlogo.img _emlogo

OBJS = $(COMMONOBJS) $(MANAGOBJS) $(STANDOBJS) \
	jagrt.o jagrt2.o jagrt3.o nvmamd.o nvmrom.o nvmat.o nvm.o romulat.o \
	rom.o

GENERATED += nvmamd.abs nvmat.abs nvmrom.abs manager.abs stand.abs \
	nvmamd.sym nvmat.sym nvmrom.sym manager.sym stand.sym

PROGS = rom.abs nvmsim.rom

nvmsim.rom: jagrt2.o $(MANAGOBJS) $(COMMONOBJS)
	$(LINK) -s -a 802000 x 5000 -o nvmsim.abs $^ $(MANAGFONTS)
	$(FIXROM) nvmsim.abs
	mv nvmsim.abs nvmsim.rom

rom.abs: rom.s manager.abs
	$(ASM) -fb rom.s
	$(LINK) -s -a 802000 x 5000 -o rom.abs rom.o
	$(FIXROM) rom.abs

stand.abs: jagrt3.o $(STANDOBJS) $(COMMONOBJS)
	$(LINK) -s -a 5000 x 20000 -o stand.abs $^ $(MANAGFONTS)
	filefix stand.abs
	rm -f stand.tx stand.dta stand.db
	$(FIXROM) stand.abs

manager.abs: jagrt.o $(MANAGOBJS) $(COMMONOBJS)
	$(LINK) -s -a c0000 x d4000 -o manager.abs $^ $(MANAGFONTS)
	filefix manager.abs
	rm -f manager.tx manager.dta manager.db
	$(FIXROM) manager.abs


nvmat.abs: nvmat.o
	$(LINK) -l -a 2400 9e0000 xt -o nvmat.abs nvmat.o
	filefix nvmat.abs
	rm -f nvmat.tx nvmat.dta nvmat.db
	$(FIXROM) nvmat.abs

nvmamd.abs: nvmamd.o
	$(LINK) -l -a 2400 9e0000 xt -o nvmamd.abs nvmamd.o
	filefix nvmamd.abs
	rm -f nvmamd.tx nvmamd.dta nvmamd.db
	$(FIXROM) nvmamd.abs

nvmrom.abs: nvmrom.o
	$(LINK) -l -a 2400 9e0000 xt -o nvmrom.abs nvmrom.o
	filefix nvmrom.abs
	rm -f nvmrom.tx nvmrom.dta nvmrom.db
	$(FIXROM) nvmrom.abs

nvm.o: nvm.c nvm.h
jagrt.o: jagrt.s nvmamd.abs nvmrom.abs nvmat.abs
jagrt2.o: jagrt2.s nvmamd.abs nvmrom.abs nvmat.abs

nvm.inc: nvm.h makeinc.c
	gcc -o makeinc -Wall -O makeinc.c
	./makeinc

nvmat.o: nvmat.s asmnvm.s nvm.inc
	$(ASM) -fb nvmat.s

nvmamd.o: nvmamd.s asmnvm.s nvm.inc
	$(ASM) -fb nvmamd.s

nvmrom.o: nvmrom.s asmnvm.s nvm.inc
	$(ASM) -fb nvmrom.s

romulat.o: romulat.s
	$(ASM) -fb romulat.s

include $(JAGSDK)/tools/build/jagrules.mk
