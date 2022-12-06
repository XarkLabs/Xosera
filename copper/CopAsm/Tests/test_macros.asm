; macro tests
.macro		twoargsdef	a=def1,b=def2
			dw			\a,\b
			dw	\0
.endm
.macro		twoargs		a, b
			dw			\a,\b
			dw	\0
.endm

; no macro arguments
.macro		countargs
			dw	\0
.endm

; no macro arguments
.macro		noargs
			dw	0x41
			dw	0x42
			dw	0x43
.endm
; one macro argument
.macro		onearg		a
			dw	\a
			dw	\a+1
			dw	\a+2
.endm
; one macro argument with default value
.macro		oneargdef	a=def1
			dw	\a
			dw	\a+1
			dw	\a+2
.endm

; simple macro that invokes another macro
.macro		nested		a=1
			oneargdef	\a
.endm
; more complicated recursion test (invoking itself, but not infinitely)
.macro		recurse		a=def1
			.if			(\a) != 0
			dw			\a
			recurse		(\a)-1
			.else
			oneargdef	\a
			.endif
.endm
; recursion test using variable (more efficient)
.macro		recursev	a=def1
var:		=			\a
			.if			(var) != 0
			dw			var
var:		=			var-1
			recursev	var
var:		.undef
			.else
			oneargdef	var
			.endif
.endm

.macro		altnoargs	
			dw			0x44
			dw			0x45
			dw			0x46
.endm

def1		=			1
def2		=			2
symA		=			100
symB		=			200

			dw			1,2
			twoargs		1,2
			twoargsdef		symA , 
			twoargsdef		, symB
			twoargsdef

			countargs	1,2,3,4,5,6,7,8,9,10,"foo"
			twoargs		1,2


			noargs
			NOARGS
			nOaRgS
			altnoargs	1 + 1,2

			onearg				; WARNING expected
			ONEARG		2
			oNeArG		3
			
			oneargdef
			ONEARGDEF	2
			oNeArGdEf	3
			
			twoargs		3,4
			twoargs		3, 4 
			twoargs		3 ,4 
			twoargs		3 , 4 
			twoargsdef	5,7
			twoargsdef	5, 7
			twoargsdef	5 ,7
			twoargsdef	5 , 7

			nested		3
		.if 1
			recurse		3

			recursev	3
		.endif



