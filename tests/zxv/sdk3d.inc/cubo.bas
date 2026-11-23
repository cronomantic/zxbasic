' Generado por sdk/tools/obj2c.py --bas a partir de cubo.obj: para
' zxv3d.bas (los vértices, x, y, z; por cara: lados, material y vértices).

DIM cubo_v(23) AS INTEGER => { _
    -4096, -4096, 4096, 4096, -4096, 4096, 4096, 4096, 4096, -4096, 4096, 4096, _
    -4096, -4096, -4096, 4096, -4096, -4096, 4096, 4096, -4096, -4096, 4096, -4096 }

DIM cubo_f(35) AS UINTEGER => { _
    4, 162, 1, 2, 3, 0, 4, 162, 7, 6, 5, 4, 4, 145, 3, 7, _
    4, 0, 4, 145, 5, 6, 2, 1, 4, 196, 2, 6, 7, 3, 4, 196, _
    4, 5, 1, 0 }

' cubo_model(n): el modelo n de MODEL3D, ADD3D...
SUB cubo_model(n AS UBYTE)
    MODEL3D n, @cubo_v(0), 8, @cubo_f(0), 6, 16
END SUB
