' ----------------------------------------------------------------
' This file is released under the MIT License
'
' Copyleft (k) 2024
' Contributed by Britlion
' ----------------------------------------------------------------

#pragma once

#pragma push(case_insensitive)
#pragma case_insensitive = True


FUNCTION fSin(num as FIXED) as FIXED
    DIM quad as byte
    DIM est1,dif as uByte

    'This change made now that MOD works with FIXED types.
    'This is much faster than the repeated subtraction method for large angles (much > 360)
    'while having some tiny rounding errors that should not significantly affect our results.
    'Note that the result may be positive or negative still, and for SIN(360) might come out
    'fractionally above 360 (which would cause issued) so the below code still is required.

    IF num >= 360 THEN
      num = num MOD 360
    ELSEIF num < 0 THEN
      num = 360 - ABS(num) MOD 360
    END IF

    IF num>180 then
      quad=-1
      num=num-180
    ELSE
      quad=1
    END IF

    IF num>90 then num=180-num

    num=num/2
    dif=num : rem Cast to byte loses decimal
    num=num-dif : rem so this is just the decimal bit


    est1=PEEK (@sinetable+dif)
    dif=PEEK (@sinetable+dif+1)-est1 : REM this is just the difference to the next up number.

    num=est1+(num*dif): REM base +interpolate to the next value.

    return (num/255)*quad


    sinetable:
    asm
    .byte 0, 9, 18, 27, 35, 44, 53, 62, 70, 79, 87, 96, 104, 112, 120, 127, 135, 143, 150, 157, 164, 171, 177, 183, 190, 195, 201, 206, 211, 216, 221, 225, 229, 233, 236, 240, 243, 245, 247, 249, 251, 253, 254, 254, 255, 255
    .balign 4
    end asm
END FUNCTION

FUNCTION fCos(num as FIXED) as FIXED
    return fSin(90-num)
END FUNCTION

FUNCTION fTan(num as FIXED) as FIXED
    return fSin(num)/fSin(90-num)
END FUNCTION

REM Fast floating Point Square Root Function
REM Adapted and modified for Boriel's ZX BASIC
REM By Britlion

FUNCTION fSqrt(radicand as FLOAT) as FLOAT
    RETURN SQR(radicand)
END FUNCTION

#pragma pop(case_insensitive)
