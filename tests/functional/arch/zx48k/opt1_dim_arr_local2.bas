
FUNCTION test as UByte
   DIM a(5) as UByte => {0, 1, 2, 3, 4, 5}
   POKE 0, a(3)
END FUNCTION
test
