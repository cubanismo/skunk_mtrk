; NVM_BIOS
; Jaguar CD-ROM Non-volatile backup memory BIOS
; Copyright 1994,1995 Atari Corporation.
; All Rights Reserved
; Version 1.01
;
; AT29C010 version
;
; This is the 2K module that gets loaded from
; the cartridge ROM into Jaguar RAM. The main
; dispatcher is found at the NVM_BIOS entry
; point.
;

	.include	"asmnvm.s"
_NVRAM	=	$800000
SECTORSIZE   = 128
LOG2_SECTORSIZE = 7

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
;	d7 = number of times to retry flush
;	a0 = source address
;	a1 = destination address
;	a2 = NVRAM base address
; Preserves all registers
;
FLUSH:
	movem.l	d0-d7/a0-a2, -(sp)
	cmp.w	#-1, cache_sector(a5)		; check for cache empty
	beq	L2				; if cache is empty, return
	moveq	#3,d7				; initialize master retry count
	lea	_NVRAM, a2

RETRYFLUSH:
	moveq.l	#0,d4				; make sure high word of d4 is 0
	move.w	cache_sector(a5), d4		; d4 = starting address within NVRAM
	moveq	#LOG2_SECTORSIZE+2, d0		; multiply by 4 because each byte is stored in a long word
	asl.l	d0, d4
	move.l	scratchbuf(a5), a0		; a0 = source address
	lea	0(a2,d4.l),a1			; a1 = dest address

	move.w	#SECTORSIZE-1, d5		; d5 = sector size/counter for writing
;
; send the write command
;
	move.w	sr,d2				; save status register
	or.w	#$0700,sr			; turn interrupts off (timing is critical here
	move.l	#$00AA0000, _NVRAM+(4*($5555))
	move.l	#$00550000, _NVRAM+(4*($2aaa))
	move.l	#$00A00000, _NVRAM+(4*($5555))		; write enable
L6:
	move.b	(a0)+, d0			; d0 = the byte to write
	not.w	d0				; invert bytes, so that erased sectors ($FF) read as 0's
	swap	d0				; put d0 in low byte of high word
	move.l	d0,(a1)+			; write it to the device
	dbra	d5,L6

	move.w	d2,sr				; restore status register
;
; wait for the write to complete
; provide a long timeout...
;
	subq.l	#4,a1				; back up to last address
	move.l	#$000fffff,d6			; timeout counter
.loop:
	move.l	(a1),d1
	eor.l	d0,d1				; see about matching bits
	btst.l	#(7+16),d1			; did bit 7 match what we wrote?
	beq.b	.done				; they match: we're done
	subq.l	#1,d6				; otherwise, check for timeout
	bpl.b	.loop				; no time out.. keep looping
;
; a failed write: see if we should retry
;
	subq.w	#1,d7				; decrement master retry count
	bpl	RETRYFLUSH			; if positive, retry the operation
	bra	FAILWRITE			; yes: fail (we really should check again)
.done:

	; now do a verify of the sector
	moveq.l	#0,d4				; make sure high word of d4 is 0
	move.w	cache_sector(a5), d4		; d4 = starting address within NVRAM
	moveq	#LOG2_SECTORSIZE+2, d0		; multiply by 4 because each byte is stored in a long word
	asl.l	d0, d4
	move.l	scratchbuf(a5), a0		; a0 = source address
	lea	0(a2,d4.l),a1			; a2 = NVRAM base, so a1 = dest address
	move.w	#SECTORSIZE-1, d5		; d5 = sector size/counter for reading

	moveq.l	#0,d0
.verifyloop:
	move.l	(a1)+,d1			; read data from ROM
	swap	d1				; get it in the low word
	not	d1				; we wrote it inverted
	move.b	(a0)+,d0			; read data from cache
	cmp.b	d0,d1				; are they equal?
	bne	FAILVERIFY			; no: fail the verify
	subq.w	#1,d5
	bpl.b	.verifyloop

Lreturn:					; return
	move.w	#-1, cache_sector(a5)			; mark cache as clear
L2:
	movem.l	(sp)+, d0-d7/a0-a2
	rts
;
; put these labels here so we can break for them
;
FAILWRITE:
	bra.b	Lreturn
FAILERASE:
	bra.b	Lreturn
FAILVERIFY:
	; see if the master retry counter is still positive
	subq.w	#1,d7
	bpl	RETRYFLUSH	; retry count is positive, so retry the whole flush
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
	move.l	a0,d1
	add.l	d1,d1				; the actual byte offset in ROM is multiplied by 4
	add.l	d1,d1
	lea	_NVRAM,a1
	move.l	0(a1,d1.l),d0			; the byte we want is the low byte of the high word
	swap	d0
	not.w	d0				; invert bytes, so cleared sectors read as 0
	and.l	#$000000ff,d0			; mask off other stuff
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
	movem.l	d2-d3/a2,-(sp)
	move.l	scratchbuf(a5),a2
	move.l	a0,d1
	moveq.l	#LOG2_SECTORSIZE,d2
	lsr.l	d2,d1				; set d1 = sector number
	move.l	a0,d2
	and.l	#(SECTORSIZE-1),d2		; set d2 = sector offset
	cmp.w	cache_sector(a5),d1		; is this the currently cached sector?
	beq.b	.iscached
	movem.l	d0-d1/a0/a2,-(sp)			; push the scratch registers we care about
	bsr	FLUSH				; flush the cache (preserves a0)
	moveq.l	#LOG2_SECTORSIZE,d0
	lsl.l	d0,d1				; make d1 = base of its sector
	move.l	d1,a0				; start reading from the sector base
	; now fill the cache
	move.w	#SECTORSIZE-1,d3
.fill:
	bsr	Getbyte
	move.b	d0,(a2)+
	addq.l	#1,a0
	dbra	d3,.fill

	movem.l	(sp)+,d0-d1/a0/a2		; restore scratch registers
	move.w	d1,cache_sector(a5)		; set the cache sector
.iscached:
	move.b	d0,(a2,d2.l)
	movem.l	(sp)+,d2-d3/a2
	rts

	.end
