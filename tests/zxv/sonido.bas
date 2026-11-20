REM zxv.bas: the .zxm and VGM modules and the recorded sounds (SAMPLELOAD,
REM SAMPLE...). sonido.sd has a .zxm with a sample track (seno), a VGM of an
REM AY, compressed (ay.vgz), one of the SCC alone (scc.vgm) and a WAV with a
REM loop (la.wav).
#include <zxv.bas>

MUSICLOAD "seno"
MUSICPLAY
PRINT "zxm "; MUSIC
PAUSE 10
MUSICLOAD "ay"
MUSICPLAY
PRINT "vgz "; MUSIC
SAMPLELOAD "la", 0
SAMPLE 0
PRINT "sample "; SAMPLEPLAYING
SAMPLE 0, 12, 32, 0, 3
SAMPLE 0, -12.5
SAMPLESTOP 3
PRINT "one stopped "; SAMPLEPLAYING
SAMPLESTOP
PRINT "all stopped "; SAMPLEPLAYING
MUSICLOAD "seno"
MUSICPLAY
SAMPLE 0
PRINT "both "; MUSIC; " "; SAMPLEPLAYING
MUSICSTOP
SAMPLESTOP
PRINT "stop "; MUSIC; " "; SAMPLEPLAYING
REM The SCC alone: R Loading error
MUSICLOAD "scc.vgm"
