; NVM_BIOS
; Jaguar CD-ROM Non-volatile backup memory BIOS
; Copyright 1994,1995 Atari Corporation.
; All Rights Reserved
; Version 1.01
;
; Skunkboard version
;
; This is the 2K module that gets loaded from
; the cartridge ROM into Jaguar RAM. The main
; dispatcher is found at the NVM_BIOS entry
; point.
;

	.include	"asmnvm.s"
	.include	"jaguar.inc"
_CART	=	$800000
; 2MB is start of application-accessible NVRAM
_NVRAM	=	(_CART+$200000)


; The NVRAM API only gives the NVRAM BIOS code a big enough scratch buffer to
; handle 16k sectors. The Skunkboard 2+ flash chip has 64k sectors (4 * 16k).
; However, the original NVRAM BIOS code also uses some funky logic to access the
; 8-bit NVRAM while in 32-bit ROM mode, and ends up multiplying every address by
; 4 and accessing each byte as a DWORD on the bus. These factors cancel each-
; other out. The 4x addressing logic from the original BIOS code is preserved,
; and the result is the sector sizes line up and 3/4 bytes just get wasted,
; meaning emulating the original 128k memory size uses 512k of flash memory on
; the Skunkboard.
SECTORSIZE   = 16384
LOG2_SECTORSIZE = 14

; setrom: Set ROMWIDTH = 16bit, ROMSPEED = 5 cycles
;
; NOTE: Try not to access ROM for ~3 instructions after this to allow the
;       value to "settle."
;
; Parameters: none
; Internal register usage:
; d0 = scratch
; d3.w = prior MEMCON1 value
; a4 = MEMCON1 address
;
; Registers clobbered:
; d0, d3, a4
setrom:
	lea	MEMCON1,a4
	move.w	(a4),d0				; Load current MEMCON1 value
	move.l	d0,d3				; Get a copy
	and.w	#$ffe1,d0			; Mask off (ROMSPEED|ROMWIDTH)
	or.w	#$1a,d0				; Set 16-bit 5 cycle width/speed
	move.w	d0,(a4)				; Set it.
	rts

; restorerom: Set ROMWIDTH = 16bit, ROMSPEED = 5 cycles
;
; NOTE: Try not to access ROM for ~3 instructions after this to allow the
;       value to "settle."
;
; Implicit Parameters:
; a4 = MEMCON1 address
; d3.w = desired MEMCON1 value
;
; Internal register usage: a4, d3 as defined above.
;
; Preserves all registers
.macro restorerom
	move.w	d3,(a4)				; Restore MEMCON1 from d3
.endm

;
;****************************************************************
;* Here are the routines that talk directly to the		*
;* hardware. These are probably the only things that will	*
;* have to change if the hardware does. There are 3		*
;* functions:							*
;*								*
;* Getbyte(long addr): get 1 byte from NVRAM; "addr" is the	*
;*	location of the byte (relative to the start of NVRAM	*
;* Putbyte(char byte, long addr): put 1 byte into NVRAM, at	*
;*	the given address (relative to start of NVRAM).		*
;* FLUSH(): flush any output buffers. Doing single character	*
;*	writes would be prohibitively expensive for most FLASH	*
;*	ROMs, so we buffer output until either a new ROM sector	*
;*	is encountered, or the system call finishes.		*
;****************************************************************
;
; FLUSH: flush the cached sector to NV RAM
; Parameters: none
; Internal register usage:
; a0 = source address
; a1 = dest. address
; a2 = ROM1 base address
; a3 = ROM1+(4*($5555))
; a4 = MEMCON1 address
; d3 = prior MEMCON1 value
; d4 = sector address << 2
; d5 = SECTORSIZE loop iterator
; d6 = master retry count for whole flush
; d7 = retry count for individual write operations
;
; Preserves all registers
;

;
; fixaddr: Ensure sub-64k address bits go to sub-64k address lines on flash
;
; Registered clobbered: d0, a1
;

fixaddr:
	move.l	d1,d0		; Make a copy
	move.l	d1,a1		; And another

	; d1 = ((addr & 0x180000) << 5) | ((addr & 0xc000) >> 5)
	and.l	#$18c000,d1	; preserve bits 20-19,15-14
	rol.w	#5,d1		; Move d1 bits 15-14 to 4-3
	swap	d1		; Move d1 bits 20-19,4-3 to 4-3,20-19
	ror.w	#5,d1		; Move d1 bits 4-3 to 15-14

	; d1 |= (addr & ~0x18c000)
	and.l	#$ffe73fff,d0	; d0 = (addr & ~0x18c000)
	or.l	d0,d1

	exg	d1,a1		; Preserve d1, return fixed address in a1
	rts

;
; utility function: sends the command prefix to the ROM
;
sendcmd:
	move.w	#$9098, (a3)			; special command
	move.w	#$C501, ($1C94-$36a)(a3)	; move.w #$C501, _CART+($1C94)
	rts
FLUSH:
	movem.l	d0-d7/a0-a4, -(sp)

	cmp.w	#-1, cache_sector(a5)		; check for cache empty
	beq	L2				; if cache is empty, return

	bsr	setrom				; Configure MEMCON1 for skunk
	moveq	#3,d6				; initialize master retry count
	move.l	#_NVRAM,d1
	lea	_CART+($36A),a3
	lea	$C00000,a2
	move.w	#$4000, (a2)			; Put skunk in read+write mode

RETRYFLUSH:
	subq.w	#1,d6				; retries exhausted yet?
	bmi	FAIL				; yes -- FAIL

	moveq.l	#0,d4				; make sure high word of d4 is 0
	move.w	cache_sector(a5), d4		; d4 = starting address within NVRAM
	moveq.l	#LOG2_SECTORSIZE+2, d0		; multiply by 4 because each byte is stored in a long word
	asl.l	d0, d4
	move.l	scratchbuf(a5), a0		; a0 = source address
	add.l	d4,d1				; d1 = dest address
	bsr	fixaddr				; a1 = fixed dest address

;
; send the "erase sector" command
;
	bsr	sendcmd
	move.w	#$8008, (a3)			; $0080

	bsr	sendcmd
	move.w	#$8480, (a1)			; $0030
	
;
; wait for sector erase to complete
; use the data polling algorithm
;
.loop:
	move.w	(a1), d0
	and.w	#8, d0				; finished yet?
	beq.b	.loop				; yes: we're done
	;btst	#(5+16),d0			; XXX Handle timeout error
	;bra	RETRYFLUSH			; XXX timeout error, give up
						; (technically we should check bit 7 again)
.done:

	move.w	#SECTORSIZE-1, d5		; d5 = sector size/counter for writing
L6:
	moveq	#3,d7				; initialize retry count for write
RETRYWRITE:
	move.b	(a0), d0			; d0 = the byte to write
	not.w	d0				; invert bytes, so that erased sectors ($FF) read as 0's

	; Skunk: Only going to write the low word of each dword. Keep the byte
	; we care about in that word.
	;swap	d0				; put d0 in low byte of high word

;
; write byte d0 to a1
;

;
; send the write command
;
	bsr	sendcmd
	move.w	#$8088, (a3)			; $00A0
	move.w	d0, (a1)
;
; now wait for it to finish
; use the data polling algorithm
;
.loop:
	move.w	(a1),d2
	cmp.w	d2, d0				; Do the values match?
	beq.b	.done				; yes: we're done
	and.w	#$1080, d2			; error indication?
	beq.b	.loop				; no: keep going
	move.w	(a1),d2				; Last chance
	cmp.w	d2, d0				; Do the values match?
	beq.b	.done				; yes: we're done
	subq	#1, d7				; error: check the retry count
	bpl	RETRYWRITE			; and retry if we should
	bra	FAILWRITE			; yes: fail (we really should check again)
.done:
	addq.l	#1,a0				; a0 = next src address
	subq.w	#1,d5
	blt.b	.verifystart
	addq.l	#4,d1				; d1 = next dst address
	bsr	fixaddr				; a1 = fixed dst address
	bra.b	L6

.verifystart:
	; now do a verify of the sector
	move.w	#SECTORSIZE-1, d5		; d5 = sector size/counter for reading

	moveq.l	#0,d0
.verifyloop:
	move.w	(a1),d2				; read data from ROM
	not	d2				; we wrote it inverted
	move.b	-(a0),d0			; read data from cache
	cmp.b	d0,d2				; are they equal?
	bne	RETRYFLUSH			; no: fail the verify
	subq.l	#4,d1				; Move to previous NVRAM addr
	bsr	fixaddr				; re-fix address
	subq.w	#1,d5
	bpl.b	.verifyloop

Lreturn:					; return
	move.w	#$4001, (a2)			; Put skunk in read-only mode
	restorerom				; Restore prior MEMCON1 value

	move.w	#-1, cache_sector(a5)			; mark cache as clear
L2:
	movem.l	(sp)+, d0-d7/a0-a4
	rts
;
; put these labels here so we can break for them
;
FAIL:
;	nop
FAILWRITE:
	bra.b	Lreturn

;
; Getbyte: fetch a byte from NV RAM
; Parameters:
;	a0.l	offset of the byte to fetch
; Internal register usage:
;	d1.l	sector number or offset
;	a1.l	buffer to use 
; ASSUMPTIONS: 256 < SECTORSIZE < 32768
; Registers destroyed: d0,d1,a1
; NOTE: a0 is preserved, so that it can be used
; in a loop
;
Getbyte:
	moveq.l	#LOG2_SECTORSIZE,d0		; makes sure high byte of d0.w is 0
	move.l	a0,d1
	lsr.l	d0,d1				; calculate the sector number
	cmp.w	cache_sector(a5),d1		; is this the currently cached sector?
	bne.b	.notcached			; if not, go read it directly
	move.l	a0,d1
	and.l	#SECTORSIZE-1,d1		; just use the offset into the buffer
	move.l	scratchbuf(a5),a1
	move.b	0(a1,d1.l),d0
	rts
.notcached:
	movem.l	d3/a4, -(sp)
	bsr	setrom				; Configure MEMCON1 for skunk

	move.l	a0,d1
	asl.l	#2,d1				; the actual byte offset in ROM is multiplied by 4
	add.l	#_NVRAM,d1			; d1 = actual dword offset
	bsr	fixaddr				; a1 = fixed dword offset
	move.w	(a1), d0
	not.w	d0				; invert bytes, so cleared sectors read as 0
	and.l	#$000000ff,d0			; mask off other stuff

	restorerom				; Restore prior MEMCON1 value
	movem.l	(sp)+, d3/a4
	rts

;
; Putbyte: put a byte into NV RAM
; Parameters:
;	d0.b	byte to put into NV RAM
;	a0.l	address in NV RAM
; Internal register usage:
;	d1.w	sector number
;	d2.w	sector offset
;	d3.l	loop counter
;	a1.l	source address
;	a2.l	scratch buffer address

; Registers destroyed: d1/a1
; NOTE: a0 is preserved, so that it can be
; used in a loop.
; ASSUMPTIONS: 256 < SECTORSIZE < 32768
;
Putbyte:
	movem.l	d2-d5/a2,-(sp)
	move.l	scratchbuf(a5),a2
	move.l	a0,d1
	moveq.l	#LOG2_SECTORSIZE,d4
	lsr.l	d4,d1				; set d1 = sector number
	move.l	a0,d2
	move.l	#(SECTORSIZE-1),d5
	and.l	d5,d2				; set d2 = sector offset
	cmp.w	cache_sector(a5),d1		; is this the currently cached sector?
	beq.b	.iscached
	movem.l	d0-d1/a0/a2,-(sp)		; push the scratch registers we care about
	bsr	FLUSH				; flush the cache (preserves a0)
	lsl.l	d4,d1				; make d1 = base of its sector
	move.l	d1,a0				; start reading from the sector base
	; now fill the cache
.fill:
	bsr	Getbyte
	move.b	d0,(a2)+
	addq.l	#1,a0
	dbra	d5,.fill

	movem.l	(sp)+,d0-d1/a0/a2		; restore scratch registers
	move.w	d1,cache_sector(a5)		; set the cache sector
.iscached:
	move.b	d0,(a2,d2.l)
	movem.l	(sp)+,d2-d5/a2
	rts

	.end
