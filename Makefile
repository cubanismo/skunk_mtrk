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
CFLAGS = -O2 -Wall -mshort -ffreestanding -fomit-frame-pointer -I$(INCLUDE)
FIXROM = ./fixrom

COMMONOBJS = arrowfnt.o alloc.o joyinp.o video.o  olist.o font.o bltrect.o \
	bltutil.o line.o sprintf.o util.o memcpy.o memset.o atol.o qsort.o \
	joypad.o strcmp.o strtol.o ctype.o

MANAGOBJS = manager.o
STANDOBJS = jagrt3.o stand.o
MANAGFONTS = -ii lubs10.jft _medfnt -ii lubs12.jft _bigfnt -ii emlogo.img _emlogo

OBJS = $(COMMONOBJS) $(MANAGOBJS) $(STANDOBJS) \
	jagrt.o jagrt2.o nvmamd.o nvmrom.o nvmat.o nvmskunk.o nvm.o romulat.o \
	rom.o

GENERATED += nvmamd.abs nvmat.abs nvmrom.abs nvmskunk.abs manager.abs \
	nvmamd.sym nvmat.sym nvmrom.sym nvmskunk.sym manager.sym stand.sym \
	nvmamd.bin nvmat.bin nvmrom.bin nvmskunk.bin manager.bin

PROGS = rom.rom nvmsim.rom stand.cof

nvmsim.rom: jagrt2.o $(MANAGOBJS) $(COMMONOBJS)
	$(LINK) -s -a 802000 x 5000 -o nvmsim.abs $^ $(MANAGFONTS)
	$(FIXROM) nvmsim.abs
	mv nvmsim.bin nvmsim.rom

rom.rom: rom.s manager.bin
	$(ASM) -fb rom.s
	$(LINK) -s -a 802000 x 5000 -o rom.abs rom.o
	$(FIXROM) rom.abs
	mv rom.bin rom.rom

stand.cof: $(STANDOBJS) $(COMMONOBJS)
	$(LINK) -l -e -a 5000 x 20000 -o stand.cof $^ $(MANAGFONTS)

manager.bin: jagrt.o $(MANAGOBJS) $(COMMONOBJS)
	$(LINK) -s -a c0000 x d4000 -o manager.abs $^ $(MANAGFONTS)
	filefix manager.abs
	rm -f manager.tx manager.dta manager.db
	$(FIXROM) manager.abs


nvmat.bin: nvmat.o
	$(LINK) -l -a 2400 9e0000 xt -o nvmat.abs nvmat.o
	filefix nvmat.abs
	rm -f nvmat.tx nvmat.dta nvmat.db
	$(FIXROM) nvmat.abs

nvmamd.bin: nvmamd.o
	$(LINK) -l -a 2400 9e0000 xt -o nvmamd.abs nvmamd.o
	filefix nvmamd.abs
	rm -f nvmamd.tx nvmamd.dta nvmamd.db
	$(FIXROM) nvmamd.abs

nvmrom.bin: nvmrom.o
	$(LINK) -l -a 2400 9e0000 xt -o nvmrom.abs nvmrom.o
	filefix nvmrom.abs
	rm -f nvmrom.tx nvmrom.dta nvmrom.db
	$(FIXROM) nvmrom.abs

nvmskunk.bin: nvmskunk.o
	$(LINK) -l -a 2400 9e0000 xt -o nvmskunk.abs $^
	filefix nvmskunk.abs
	rm -f nvmskunk.tx nvmskunk.dta nvmskunk.db
	$(FIXROM) nvmskunk.abs

nvm.o: nvm.c nvm.h
jagrt.o: jagrt.s nvmskunk.bin
jagrt2.o: jagrt2.s nvmamd.bin nvmrom.bin nvmat.bin
jagrt3.o: jagrt.s nvmskunk.bin

nvm.inc: nvm.h makeinc.c
	gcc -o makeinc -Wall -O makeinc.c
	./makeinc

nvmat.o: nvmat.s asmnvm.s nvm.inc
	$(ASM) -fb nvmat.s

nvmamd.o: nvmamd.s asmnvm.s nvm.inc
	$(ASM) -fb nvmamd.s

nvmrom.o: nvmrom.s asmnvm.s nvm.inc
	$(ASM) -fb nvmrom.s

nvmskunk.o: nvmskunk.s asmnvm.s nvm.inc
	$(ASM) -fb nvmskunk.s

romulat.o: romulat.s
	$(ASM) -fb romulat.s

include $(JAGSDK)/tools/build/jagrules.mk
