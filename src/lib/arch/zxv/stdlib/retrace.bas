' ----------------------------------------------------------------
' This file is released under the MIT License
'
' ZX-V version of retrace.bas: waits for the next frame (VBLANK)
' ----------------------------------------------------------------

#ifndef waitretrace
#define waitretrace 'REM Retrace \
	asm \
		zxb_ccall zxb_lib_vblank \
	end asm
#endif
