TEST	=	0
;
; Jaguar C library run time startup code
;
; Copyright 1994,1995 Atari Corporation.
; All rights reserved.
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
	move.l	$2400.w,d0				; save the old magic number for the cart
	move.l	d0,cart_magic
	cmp.l	#$12345678,d0
	beq.b	.10

	move.l	#$00070007,G_END		;don't need to swap for GPU
	move.w	#$FFFF,VI			;temporarily disable video interrupts
	move.w	#$0100,JOYSTICK			;set bit 8 to cancel mute

	move.l	#$17fffc,sp			;set stack to top of our usable memory (we can use $100000-$180000)

	suba.l	a6,a6				; initialize frame pointer for debugger

	move.w	#$6C1,-(sp)			; assume 16 bit CRY mode
	jsr	_VIDinit			; initialize the video
	addq.w	#2,sp

	jsr	_VIDsync			; wait for video sync

.10:

;
; The only type of NVRAM supported is the Skunkboard
;

	move.l	#nvmskunk,a0
	move.l	#nvmskunk_end,d0

loadbios:

	lea	$2400,a1
;
; copy the BIOS over
.copy:
	move.l	(a0)+,(a1)+
	cmp.l	a0,d0
	bge.b	.copy
;
; check for CD boot rom
;
	move.l	cart_magic,d0			; CD boot rom puts $12345678 into $2400
	cmp.l	#$12345678,d0			; if this isn't found, always run the manager
	bne.b	domanager			; program
;
; now look for the option button on controller 1
;
	.extern	__PAD1
	jsr	__PAD1
	btst.l	#OPTION,d0			; is the option button down?
	beq	skipmanager			; no -- skip the program manager
;
; now fall through to the program manager
;
domanager:
	move.l	#$00070007,G_END		;don't need to swap for GPU
	move.w	#$FFFF,VI			;temporarily disable video interrupts
	move.w	#$0100,JOYSTICK			;set bit 8 to cancel mute

;; don't mess with stack, we need to rts
;;;	move.l	#$17fffc,sp			;set stack to top of our usable memory (we can use $100000-$180000)

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
;
; check for CD boot rom again
; if it isn't there, don't return, re-run the program instead
;
	move.l	cart_magic,d0
	cmp.l	#$12345678,d0
	bne	domanager

skipmanager:
; end of program
	rts				; return to caller


	; pause 10 mS: on Jaguar I there are 13000 cycles/mS; let's be safe and use
	; Jaguar II's 20000 cycles/mS, so pause for 200,000 cycles
	; a divs instruction takes 122 cycles, so execute 1640 of them
pause10:
	movem.l	d0-d1,-(sp)
	move.w	#409,d0		; 4 divides per loop
	moveq	#1,d1
.delay:
	divs	d1,d1
	divs	d1,d1
	divs	d1,d1
	divs	d1,d1
	dbra	d0,.delay
	movem.l	(sp)+,d0-d1
	rts

___main::
	rts

_abort::
	link	a6,#0
	illegal
	unlk	a6
	rts

	.bss
_OLPstore::
	ds.l	160
_OList::
	ds.l	1
__timestamp::
	ds.l	1
cart_magic::
	ds.l	1			; old contents of $2400

	.data
stopobj:
	.dc.l	0
	.dc.l	4

;
; the acutal BIOSes come here
;
; SKUNKBOARD BIOS
nvmskunk:
	.incbin	"nvmskunk.bin"
nvmskunk_end:
