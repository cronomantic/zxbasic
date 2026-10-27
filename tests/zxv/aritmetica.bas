REM Integer, fixed and float arithmetic
DIM b AS BYTE = -100
DIM ub AS UBYTE = 200
DIM i AS INTEGER = -30000
DIM ui AS UINTEGER = 60000
DIM l AS LONG = -2000000000
DIM ul AS ULONG = 4000000000
DIM f AS FIXED = 1.5
DIM x AS FLOAT = 2.25

PRINT b + 1; " "; b - 100; " "; b * 2; " "; b / 3; " "; b MOD 7
PRINT ub + 100; " "; ub - 201; " "; ub * 2; " "; ub / 7; " "; ub MOD 7
PRINT i + 1; " "; i - 10000; " "; i / 7; " "; i MOD 7
PRINT ui + 10000; " "; ui / 7; " "; ui MOD 7; " "; ui * 2
PRINT l + 1; " "; l / 3; " "; l MOD 7; " "; l - 1000000000
PRINT ul + 1; " "; ul / 3; " "; ul MOD 7
PRINT f + 1; " "; f * f; " "; f / 4; " "; -f
PRINT x + 1; " "; x * x; " "; x / 3; " "; -x; " "; x ^ 2; " "; 2 ^ 10
PRINT ub << 1; " "; ub >> 2; " "; i >> 2; " "; ul >> 4; " "; l << 1
PRINT ub bAND 15; " "; ub bOR 1; " "; ub bXOR 255; " "; bNOT ub
PRINT 3 < 4; " "; 3 > 4; " "; -1 < 1; " "; ui > 100; " "; x >= 2.25; " "; x <> 2.25
PRINT (3 < 4) AND (5 > 6); " "; (3 < 4) OR (5 > 6); " "; NOT (3 < 4)
PRINT ABS(b); " "; ABS(i); " "; ABS(-x); " "; SGN(b); " "; SGN(x); " "; SGN(0)
PRINT INT(x); " "; INT(-x); " "; CAST(INTEGER, x * 10); " "; CAST(UBYTE, 300)
PRINT 1 / 3; " "; 1E10; " "; 0.05; " "; 123456789; " "; 1E-6
DIM y AS FLOAT = 1.5
PRINT x < y; x > y; x <= y; x >= y; x = y; x <> y; " "; y < x; y > x; y <= x; y >= x; y = x; y <> x
PRINT y >= 1.5; y <= 1.5; y > 1.5; y < 1.5
