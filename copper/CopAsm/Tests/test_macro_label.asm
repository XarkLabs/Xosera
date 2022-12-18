; vim: set noet ts=8 sw=8
;
.macro		labeltest	arg
		dw		\arg
foo\@:		dw		\arg+\arg	; \@ expands to <macroname>_<invocation #>
.endm

		labeltest	1
		labeltest	2
