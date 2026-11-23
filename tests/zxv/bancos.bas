REM Bancos de código (ZX-V D109, zxbc --code-bank): las funciones de cada
REM fichero de bancos.inc se cargan del propio .zxv al llamarlas.
DIM contador AS INTEGER = 0
#include "bancos.inc/tres.bas"
#include "bancos.inc/uno.bas"
#include "bancos.inc/dos.bas"

PRINT suma(2, 3)
PRINT factorial(10)
PRINT mitad(7)
PRINT saludo("mundo")
PRINT doble(21)
PRINT resta(10, 4)
PRINT suma(1, 1)
cuenta()
cuenta()
PRINT contador
llamaTres()
PRINT lista()
apunta(5)
apuntaDos()
apunta(10)
PRINT marcador; " "; ultimo
apuntaDos()
PRINT marcador; " "; ultimo
PRINT "fin"
