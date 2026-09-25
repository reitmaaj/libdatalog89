#ifndef DL89_SYMBOLS_H
#define DL89_SYMBOLS_H

/* Common symbolic fixture: fixed numeric identifiers with no special
 * meaning to libdl89. */

enum
{
    C_A = 101,
    C_B = 102,
    C_C = 103,
    C_D = 104,
    C_E = 105
};

enum
{
    R_PARENT = 1,
    R_ANCESTOR = 2,
    R_EDGE = 3,
    R_PATH = 4,
    R_SAME = 5,
    R_LEFT = 6,
    R_RIGHT = 7,
    R_JOINED = 8,
    R_P = 9,
    R_Q = 10,
    R_R = 11,
    R_FLAG = 12
};

enum
{
    V_X = 1,
    V_Y = 2,
    V_Z = 3
};

#define DL89_ULMAX ((unsigned long)-1)

#endif
