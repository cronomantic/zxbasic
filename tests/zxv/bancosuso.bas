REM Bancos de código: una función del banco a llama a una del banco b, de su
REM misma zona. b no se puede cargar encima de a, que está en uso: el
REM programa para con «Banco a en uso: no se puede cargar b en su zona».
#include "bancosuso.inc/b.bas"
#include "bancosuso.inc/a.bas"
PRINT "antes"
llamaB()
PRINT "después"
