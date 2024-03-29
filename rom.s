LOADADDR	=	$c0000
VI		=	$f0004e
;
; simple ROM bootup stub: all it does is copies the main program
; down to where it wants to be
;
; first, though, it checks to see if we're booting from the CD-ROM
; boot rom; if not, it disables video (so that [sic]
;
; Even before that though, branch over a marker the Skunkboard BIOS
; uses to detect this ROM and auto-boot it when in CD boot mode.

	bra.s	.realboot

	.long
	.dc.l	'SKNK'				; Identifiers
	.dc.l	'MTRK'
	.dc.l	$00802000			; Boot address
	.dc.l	$00010100			; Version: 1.1.0

.realboot:
	move.l	$2400.w,d0			; check for boot rom magic
	cmp.l	#$12345678,d0
	beq.b	.cdboot
;
; not running from CD-ROM: initialize video & stack
;
	move.w	#$FFFF,VI
	move.l	#$1ffff8,sp

.cdboot:
	lea	LOADADDR,a1
	move.l	#nvcart,a0
	move.l	#nvcart_end,d0
.copy:
	move.l	(a0)+,(a1)+
	cmp.l	a0,d0
	bge.b	.copy
	jmp	LOADADDR

nvcart:
	.incbin	"manager.bin"
nvcart_end:
