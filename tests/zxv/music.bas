REM zxv.bas: the music and the effects of the trackers (MUSICLOAD, MUSICPLAY,
REM SFXLOAD, SFX...). music.sd has a module of one tone, a bank of one effect
REM and a single effect (.afx); BEEP plays while they do.
#include <zxv.bas>

MUSICLOAD "tono"
PRINT "loaded "; MUSIC
MUSICPLAY
PRINT "play "; MUSIC
PAUSE 10
MUSICPAUSE
PRINT "pause "; MUSIC
MUSICCONTINUE
PRINT "continue "; MUSIC
SFXLOAD "pitido"
SFX 0
PRINT "sfx "; SFXPLAYING
BEEP 0.2, 0
SFX 0, 4
SFXSTOP
PRINT "sfx stop "; SFXPLAYING
MUSICSTOP
PRINT "stop "; MUSIC
SFXLOAD "uno.afx"
SFX 0
PRINT "afx "; SFXPLAYING
MUSICPLAY
PRINT "again "; MUSIC
REM Only one effect in the bank: B Integer out of range
SFX 1
