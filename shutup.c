#include <exec/types.h>
void main(argc,argv)
int argc;
char *argv[];
{
    UBYTE *bytebase = (UBYTE *)(0x00e8004C);
    (*(bytebase)) = 0xFF;
}