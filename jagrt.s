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

	move.w	#$1865,MEMCON1				; set cart up to be 32 bits
	nop						; wait for it to settle down
	nop
	nop
;
; figure out which type of NVRAM cart is plugged in
;

; first, try asking flash rom for product identification

	move.w	#$00aa,d4
	move.w	#$0055,d5
	move.w	#$0090,d6
	lea	$800000+(4*$5555),a4
	lea	$800000+(4*$2aaa),a5
	move.w	sr,d7
	or.w	#$0700,sr				; interrupts off
;;	move.w	#$00AA,$800000+(4*$5555)		; send "product identification" command
;;	move.w	#$0055,$800000+(4*$2aaa)
;;	move.w	#$0090,$800000+(4*$5555)
	move.w	d4,(a4)
	move.w	d5,(a5)
	move.w	d6,(a4)
	move.w	d7,sr					; interrupts back on again
	bsr	pause10

	move.l	$800000,d2	; read manufacturer code from the device
	swap	d2		; get data into low word
	and.w	#$00FF,d2	; mask out all but the byte we read

	move.l	$800004,d3	; read device id
	swap	d3		; get data into low word
	and.w	#$00FF,d3	; mask out all but the byte we read
;
; reset the ROM
;
	move.w	#$00aa,d4
	move.w	#$0055,d5
	move.w	#$00f0,d6
	lea	$800000+(4*$5555),a4
	lea	$800000+(4*$2aaa),a5

	move.w	sr,d7
	or.w	#$0700,sr				; interrupts off
;;	move.w	#$00AA,$800000+(4*$5555)		; send reset command
;;	move.w	#$0055,$800000+(4*$2aaa)
;;	move.w	#$00F0,$800000+(4*$5555)
	move.w	d4,(a4)
	move.w	d5,(a5)
	move.w	d6,(a4)
	move.w	d7,sr					; interrupts back on again
	bsr	pause10

	cmp.b	#$01,d2		; AMD manufacturer ID == 01
	bne.b	.notAMD
	cmp.b	#$20,d3		; check for device == AM29F010
	bne	unknown_device

	move.l	#nvmamd,a0
	move.l	#nvmamd_end,d0
	bra	loadbios

.notAMD:
	cmp.b	#$1f,d2		; AMTEL manufacturer ID == $1f
	bne.b	.notATMEL
	cmp.b	#$d5,d3		; check for device == AT29C010
	bne	unknown_device

	move.l	#nvmat,a0
	move.l	#nvmat_end,d0
	bra	loadbios

.notATMEL:
;
; next, check for ROMULATOR
;
	move.w	$800000+(4*$2aaa),d0
	cmp.w	#$0055,d0
	bne	unknown_device
	move.l	#nvmrom,a0
	move.l	#nvmrom_end,d0
	bra	loadbios
unknown_device:
	move.l	#nvmnone,a0
	move.l	#nvmnone_end,d0

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
; ROMULATOR BIOS
nvmrom:
	.incbin	"nvmrom.bin"
nvmrom_end:

; AMD BIOS
	.long
nvmamd:
	.incbin	"nvmamd.bin"
nvmamd_end:

; ATMEL BIOS
	.long
nvmat:
	.incbin	"nvmat.bin"
nvmat_end:

; UNKNOWN DEVICE BIOS
	.long
nvmnone:
	dc.l	'_NVM'
	moveq.l	#-1,d0
	rts
nvmnone_end:
