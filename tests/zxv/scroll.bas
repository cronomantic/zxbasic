REM scroll.bas and winscroll.bas
#include <scroll.bas>
#include <winscroll.bas>
#include <point.bas>
#include <screen.bas>
#include <attr.bas>

REM Pixels: inside the window (10..50, 90..110) and outside it
PLOT 10, 100: PLOT 50, 100: PLOT 60, 100: PLOT 31, 95: PLOT 32, 95
ScrollRight(10, 90, 50, 110)
PRINT point(10, 100); point(11, 100); point(50, 100); point(60, 100); " "; point(32, 95); point(33, 95)
ScrollLeft(10, 90, 50, 110)
ScrollLeft(10, 90, 50, 110)
PRINT point(10, 100); point(9, 100); point(30, 95); point(31, 95); point(32, 95); point(60, 100)
ScrollUp(10, 90, 50, 110)
PRINT point(30, 95); point(30, 96); point(10, 100); point(10, 101)
PLOT 20, 110
ScrollUp(10, 90, 50, 110)
PRINT point(20, 110); point(20, 111)
PLOT 40, 90
ScrollDown(10, 90, 50, 110)
PRINT point(40, 90); point(40, 89); point(30, 96); point(30, 97)

REM Cells
CLS
PRINT AT 3, 3; INK 2; "A"; INK 1; "B"; AT 6, 0; "C"
WinScrollRight(3, 3, 5, 1)
PRINT AT 12, 0; screen(3, 3); "|"; screen(3, 4); screen(3, 5); " "; attr(3, 4); " "; attr(3, 5); " "; attr(3, 3)
WinScrollLeft(3, 3, 5, 1)
WinScrollLeft(3, 3, 5, 1)
PRINT screen(3, 3); "|"; screen(3, 7); "|"
WinScrollUp(3, 0, 10, 4)
PRINT screen(5, 0); screen(2, 3); "|"; screen(6, 0); "|"
WinScrollDown(3, 0, 10, 4)
WinScrollDown(3, 0, 10, 4)
PRINT screen(6, 0); screen(5, 0); "|"; screen(3, 0); "|"
WinScrollRight(3, 3, 0, 1)
PRINT "fin"
