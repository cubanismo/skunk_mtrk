	.include 'jaguar.inc'
;
; A very simple font, containing two arrows: an arrow pointing up (mapped to 'A')
; and one pointing down (mapped to 'B')
;
	.globl	_arrowfnt
_arrowfnt:
	dc.b	1,0		; bitmapped font, monospaced
	dc.b	8,8		; width,height
	dc.b	'A','B'		; startcharacter, endcharacter
	dc.w	WID16|PIXEL1
;		  A    B
	dc.b	$08, $1c
	dc.b	$1c, $1c
	dc.b	$3e, $1c
	dc.b	$7f, $1c
	dc.b	$1c, $7f
	dc.b	$1c, $3e
	dc.b	$1c, $1c
	dc.b	$1c, $08
