#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef unsigned int uint;
typedef unsigned char byte;
typedef struct MD5state
{
    uint len;
    uint state[4];
}MD5state;
enum
{
    S11=    7,
    S12=    12,
    S13=    17,
    S14=    22,

    S21=    5,
    S22=    9,
    S23=    14,
    S24=    20,

    S31=    4,
    S32=    11,
    S33=    16,
    S34=    23,

    S41=    6,
    S42=    10,
    S43=    15,
    S44=    21
};

typedef struct Table
{
    uint    sin;    /* integer part of 4294967296 times abs(sin(i)) */
    byte    x;  /* index into data block */
    byte    rot;    /* amount to rotate left by */
}Table;

void encode(byte*, uint*, uint);
void decode(uint*, byte*, uint);
MD5state* md5(byte*, uint, byte*, MD5state*, MD5state*);
char* sum( char*);//call這個