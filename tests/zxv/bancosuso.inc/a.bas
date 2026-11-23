' Banco "a" (zona 0): llama a una función del banco "b", en la misma zona.
SUB llamaB()
    PRINT "a llama a b"
    deB()
    PRINT "no llega"
END SUB
