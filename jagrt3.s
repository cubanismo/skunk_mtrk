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
; Put Skunkboard in bank 2, so NVRAM won't interfere with the rom boot code
	move.w	#$4BA1, $C00000

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
