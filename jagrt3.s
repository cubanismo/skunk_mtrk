TEST	=	0
;
; Jaguar C library run time startup code
;
; Copyright 1994,1995 Atari Corporation.
; All rights reserved.
;
; Portions Copyright 2022, James Jones
; Released under the CC0-1.0 license.
;
;
; Functions defined here:
; void abort(void):
;	does an illegal instruction so rdbjag gets control
;
; Functions called:
; main(int argc, char **argv, char **envp): obvious
; _init_alloc(void): initialize memory allocator
; VIDinit(void): initialize video
;
; Variables:
; OLPstore: storage for packed object list
; at most 80 phrases worth of objects are
; supported

	.include	"jaguar.inc"

	.extern	_VIDinit
	.extern	_VIDsync
	.extern	__init_alloc
	.extern	__init_JOY
	.extern	_JOYget,_JOYedge
	.extern	_main
	.extern	SetOLP
	.extern _BSS_E

RBASE	.equ $800000				; ROM Base Address
HPIDATA	.equ RBASE
HPIADDR	.equ $C00000

	.text
start::
; assume a Skunkboard is plugged in
;
	move.l	#nvmskunk,a0
	move.l	#nvmskunk_end,d0

	lea	$2400,a1
;
; copy the BIOS over
.copy:
	move.l	(a0)+,(a1)+
	cmp.l	a0,d0
	bge.b	.copy

;
; Since we'll be operating in bank 2 mode, we need to initialize the BIOS in
; bank 2 so the Skunkboard will still boot if the Jaguar is reset. Normally the
; Skunkboard BIOS itself takes care of this, but it launched us in bank 1, so
; it must be handled manually here.
;
	jsr	InitSkunkB2BIOS


	move.l	#$00070007,G_END		;don't need to swap for GPU
	move.w	#$FFFF,VI			;temporarily disable video interrupts
	move.w	#$0100,JOYSTICK			;set bit 8 to cancel mute

;; don't mess with stack, we need to rts
	move.l	#$17fffc,sp			;set stack to top of our usable memory (we can use $100000-$180000)

	suba.l	a6,a6				; initialize frame pointer for debugger

	move.w	#$6C1,-(sp)			; assume 16 bit CRY mode
	jsr	_VIDinit			; initialize the video
	addq.w	#2,sp

	jsr	__init_alloc			; initialize the memory allocator
	jsr	__init_JOY			; initialize the joypad library

	move.l	#0,__timestamp
	move.l	#-1,PIT0			; initialize the timing library

	move.w	#0,-(sp)
	jsr	_main
	addq.l	#2,sp
;
; clean up the video
;
	move.l	#stopobj,_OList			; make the object list empty
	jsr	_VIDsync			; wait for VBI (so it will be copied over)
	move.w	#$FFFF,VI			; disable interrupts
	move.l	#0,$2300.w			; write a stop object to $2300
	move.l	#4,$2304.w
	move.l	#$2300,d0			; restore OLP to $2300
	jsr	SetOLP

skipmanager:
; end of program
	illegal				; return to caller

___main::
	rts

_abort::
	link	a6,#0
	illegal
	unlk	a6
	rts

; Copy the Skunkboard BIOS to the beginning of bank 2 if needed.
;
; Use the memory after _BSS_E as scratch space. This is safe at least
; until __init_alloc is called, as the allocator module uses the same
; region to fulfill malloc() requests.
;
; Exits with bank 2 active.
InitSkunkB2BIOS:
	movem.l	d0-d1/a0-a2,-(sp)
	move.l	#HPIADDR, a2

	move.w	#$4BA1, (a2)		; Select bank 1
	; a little extra check here to make sure there is a BIOS present!
	move.l	RBASE,d0		; check the first dword contains data
	cmp.l	#$ffffffff,d0		; erased
	bne	.donesb			; there is SOMETHING there

.copybios:	; Copy the BIOS over. Note this overwrites 8k RAM at _BSS_E
	move.w	#$1234, BG		; purple background, in case it hangs

	move.w	#$4000, (a2)		; Enter Flash read/write mode
	move.w	#$4BA0, (a2)		; now from bank 0
		
	move.l	#RBASE, a0		; source BIOS data
	move.l	#((_BSS_E+3)&(~3)), a1	; destination in RAM
	move.l	#$3ff, d0		; 4k / 4 - 1
.cplp2:
	move.l	(a0)+,(a1)+
	dbra	d0,.cplp2		; copy the block

	; Now write the data to bank 2
	move.w  #$4BA1, (a2)		; Access BAnk 1!

	; do the write - to guarantee it will work, we should use the
	; slow flash write from _BSS_E to $800000, 4k
	move.l	#((_BSS_E+3)&(~3)), a0	; source
	move.l	#RBASE, a1		; dest
	move.l	#$7ff, d0		; 4k/2-1

.wrword:
	;36A=9098 / 1C94=C501 / 36A=8088 / Adr0=Data
	move.w	#$9098, $80036a
	move.w	#$c501, $801c94
	move.w	#$8088, $80036a	; program word
	move.w	(a0)+, d1
	move.w  d1,(a1)			; Payload

.waitpgm21:
	cmp.w	(a1), d1		; wait for it to be equal
	bne 	.waitpgm21
	addq	#2,	a1

	dbra	d0, .wrword

	move.w	#$4001, (a2)		; set flash read-only mode
.donesb:
	movem.l	(sp)+,d0-d1/a0-a2	; Restore regs
	rts

	.data
; the acutal BIOSes come here
;
; SKUNKBOARD BIOS
nvmskunk:
	.incbin	"nvmskunk.bin"
nvmskunk_end:

	.bss
_OLPstore::
	ds.l	160
_OList::
	ds.l	1
__timestamp::
	ds.l	1

	.data
stopobj:
	.dc.l	0
	.dc.l	4
