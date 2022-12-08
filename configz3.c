/* ====================================================================== */
/* A very simple configuration utility for Zorro III boards. This code will
configure Zorro III cards that are placed after any Zorro II cards in
the A3000. All configuration is done based on 16 meg slots and no magic
for autoboot, etc. Eventually 2.0 will do this better. */
#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/configregs.h>
#include <libraries/configvars.h>
#include <libraries/expansionbase.h>
#include <stdio.h>
#include <ctype.h>
// #include <functions.h>
/* ====================================================================== */
/* Modified configuration information. */
/* Extensions to the TYPE field. */
#define E_Z3EXPBASE 0xff000000L
#define E_Z3EXPSTART 0x10000000L
#define E_Z3EXPFINISH 0x7fffffffL
#define E_Z3SLOTSIZE 0x01000000L
#define E_Z3ASIZEINC 0x00010000L
#define ERT_ZORROII ERT_NEWBOARD
#define ERT_ZORROIII 0x80
/* Extensions to the FLAGS field. */
#define ERFB_EXTENDED 5L
#define ERFF_EXTENDED (1L<<5)
static BoardSize[2][8] = {
    { 0x00800000,0x00010000,0x00020000,0x00040000,
    0x00080000,0x00100000,0x00200000,0x00400000 },
    { 0x01000000,0x02000000,0x04000000,0x08000000,
    0x10000000,0x20000000,0x40000000,0x00000000 }
};
#define ERFB_QUICKVALID 4L
#define ERFF_QUICKVALID (1L<<4)
#define ERF_SUBMASK 0x0fL
#define ERF_SUBSAME 0x00L
#define ERF_SUBAUTO 0x01L
#define ERF_SUBFIXED 0x02L
#define ERF_SUBRESERVE 0x0eL
static SubSize[16] = {
    0x00000000,0x00000000,0x00010000,0x00020000,
    0x00040000,0x00080000,0x00100000,0x00200000,
    0x00400000,0x00600000,0x00800000,0x00a00000,
    0x00c00000,0x00e00000,0x00000000,0x00000000
};
#define PRVB(x)if (verbose) { printf(x); }
static BOOL verbose = TRUE;
static BOOL anyone = FALSE;
struct ExpansionBase *ExpansionBase;
static ULONG Z3Space = 0x40000000L;   // Was 0x10000000L
/* ====================================================================== */
/* These functions are involved in finding a Zorro III board. */
/* This function reads the logical value stored at the given Zorro III
ROM location. This corrects for complements and the differing offsets
depending on location. */
UBYTE ReadZ3Reg(base,reg)
WORD *base;
WORD reg;
{
    ULONG *Z3base;
    UWORD result;
    if (base == (WORD *)E_EXPANSIONBASE) {
        base += (reg>>1);
        result = ((*base++)&0xf000)>>8;
        result |= ((*base)&0xf000)>>12; // Alex edit "|=" to look like below
    } else {
        Z3base = (ULONG *)(base+(reg>>1));
        result = ((*Z3base)&0xf0000000)>>24;
        result |= ((*(Z3base+0x40))&0xf0000000)>>28;
    }
    if (reg) result = ~result;
    return (UBYTE)result;
}
/* This function types the board in the system, returning the type code.
There are four possibilities -- no board, a Zorro II board, a Zorro III
board at the Zorro II configuration slot, and a Zorro III board at the
Zorro III configuration slot. */
#define BT_NONE 0
#define BT_Z2 1
#define BT_Z3_AT_Z2 2
#define BT_Z3_AT_Z3 3
BYTE TypeOfPIC() {
    UBYTE type;
    UWORD manf;
    type = ReadZ3Reg(E_EXPANSIONBASE,0x00);
    manf = ReadZ3Reg(E_EXPANSIONBASE,0x10)<<8 | ReadZ3Reg(E_EXPANSIONBASE,0x14);
    if (manf != 0x0000 && manf != 0xffff) {
        if ((type & ERT_TYPEMASK) == ERT_ZORROII) return BT_Z2;
        if ((type & ERT_TYPEMASK) == ERT_ZORROIII) return BT_Z3_AT_Z2;
    }
    type = ReadZ3Reg(E_Z3EXPBASE,0x00);
    manf = ReadZ3Reg(E_Z3EXPBASE,0x10)<<8 | ReadZ3Reg(E_Z3EXPBASE,0x14);
    if (manf != 0x0000 && manf != 0xffff)
    if ((type & ERT_TYPEMASK) == ERT_ZORROIII) return BT_Z3_AT_Z3;
    return BT_NONE;
}
/* This function fills the configuration ROM field of the given
ConfigDev, form the given address, based on the appropriate mapping
rules. */
void InitZ3ROM(base,cd)
WORD *base;
struct ConfigDev *cd;
{
    struct ExpansionRom *rom;
    rom = &cd->cd_Rom;
    rom->er_Type = ReadZ3Reg(base,0x00);
    rom->er_Product = ReadZ3Reg(base,0x04);
    rom->er_Flags = ReadZ3Reg(base,0x08);
    rom->er_Reserved03 = ReadZ3Reg(base,0x0c);
    rom->er_Manufacturer = ReadZ3Reg(base,0x10)<< 8 | ReadZ3Reg(base,0x14);
    rom->er_SerialNumber = ReadZ3Reg(base,0x18)<<24 | ReadZ3Reg(base,0x1c)<<16 |
    ReadZ3Reg(base,0x20)<< 8 | ReadZ3Reg(base,0x24);
    rom->er_InitDiagVec = ReadZ3Reg(base,0x28)<< 8 | ReadZ3Reg(base,0x2c);
    rom->er_Reserved0c = ReadZ3Reg(base,0x30);
    rom->er_Reserved0d = ReadZ3Reg(base,0x34);
    rom->er_Reserved0e = ReadZ3Reg(base,0x38);
    rom->er_Reserved0f = ReadZ3Reg(base,0x3c);
}
/* This function locates a Zorro III board. If it finds one in the
unconfigured state, it allocates a ConfigDev for it, fills in the
configuration data, and returns that ConfigDev. Otherwise it returns
NULL. It knows the basics of what to do should it encounter a
Zorro II board sitting in the way. */
struct ConfigDev *FindZ3Board() {
struct ConfigDev *cd;
while (TRUE) {
    if (!(cd = AllocConfigDev())) return NULL;
    switch (TypeOfPIC()) {
        case BT_NONE :
        FreeConfigDev(cd);
        return NULL;
        case BT_Z2 :
            PRVB("FOUND: Z2 Board, Configuring\n");
            if (!ReadExpansionRom(E_EXPANSIONBASE,cd))
            if (!ConfigBoard(E_EXPANSIONBASE,cd))
            AddConfigDev(cd);
            anyone = TRUE;
            break;
        case BT_Z3_AT_Z2 :
            PRVB("FOUND: Z3 Board (Z2 Space), Configuring\n");
            InitZ3ROM(E_EXPANSIONBASE,cd);
            cd->cd_BoardAddr = (APTR)E_EXPANSIONBASE;
            anyone = TRUE;
            return cd;
        case BT_Z3_AT_Z3 :
            PRVB("FOUND: Z3 Board (Z3 Space), Configuring\n");
            InitZ3ROM(E_Z3EXPBASE,cd);
            cd->cd_BoardAddr = (APTR)E_Z3EXPBASE;
            anyone = TRUE;
            return cd;
    }
}
return NULL;
}
/* ====================================================================== */
/* These functions are involved in configuring a Zorro III board. */
/* This function writes the configuration address stored in the given
ConfigDev to the board in the proper way. */
void WriteCfgAddr(base,cd)
UWORD *base;
struct ConfigDev *cd;
{
    UBYTE nybreg[4],bytereg[2],*bytebase;
    UWORD wordreg,i,*wordbase;
    wordreg = (((ULONG)cd->cd_BoardAddr)>>16);
    printf("DEBUG: base: $%x\n",base);
    printf("DEBUG: wordreg: $%x\n",wordreg);
    bytereg[0] = (UBYTE)(wordreg & 0x00ff);
    bytereg[1] = (UBYTE)(wordreg >> 8);
    nybreg[0] = ((bytereg[0] & 0x0f)<<4);
    nybreg[1] = ((bytereg[0] & 0xf0));
    nybreg[2] = ((bytereg[1] & 0x0f)<<4);
    nybreg[3] = ((bytereg[1] & 0xf0));
    bytebase = (UBYTE *)(base + 22);
    wordbase = (UWORD *)(base + 22);
    if (base == (UWORD *)E_EXPANSIONBASE) {
        printf("DEBUG: Doing Z2-style config write\n");
        printf("DEBUG: writing $%x\n",(bytebase+0x002));
        (*(bytebase+0x002)) = nybreg[2];
        printf("DEBUG: writing $%x\n",(bytebase+0x000));
        (*(bytebase+0x000)) = bytereg[1];
        printf("DEBUG: writing $%x\n",(bytebase+0x006));
        (*(bytebase+0x006)) = nybreg[1];
        printf("DEBUG: writing $%x\n",(bytebase+0x004));
        (*(bytebase+0x004)) = bytereg[0];
    } else {
        printf("DEBUG: Doing Z3-style config write\n");
        printf("DEBUG: writing $%x\n",(bytebase+0x104));
        (*(bytebase+0x104)) = nybreg[0];
        printf("DEBUG: writing $%x\n",(bytebase+0x004));
        (*(bytebase+0x004)) = bytereg[0];
        printf("DEBUG: writing $%x\n",(bytebase+0x100));
        (*(bytebase+0x100)) = nybreg[2];
        printf("DEBUG: writing $%x\n",(bytebase+0x000));
        (*(wordbase+0x000)) = wordreg;
    }
}
/* This function automatically sizes the configured board described by the
given ConfigDev. It doesn’t attempt to preserve the contents. */
void AutoSizeBoard(cd)
struct ConfigDev *cd;
{
    ULONG i,realmax,logicalsize = 0;
    realmax = ((ULONG)cd->cd_SlotSize) * E_Z3SLOTSIZE + (ULONG)cd->cd_BoardAddr;
    for (i = (ULONG)cd->cd_BoardAddr; i < realmax; i += E_Z3ASIZEINC)
    *((ULONG *)i) = 0;
    for (i = (ULONG)cd->cd_BoardAddr; i < realmax; i += E_Z3ASIZEINC) {
        if (*((ULONG *)i) != 0) break;
        *((ULONG *)i) = 0xaa5500ff;
        if (*((ULONG *)i) != 0xaa5500ff) break;
        logicalsize += E_Z3ASIZEINC;
    }
    cd->cd_BoardSize = (APTR)logicalsize;
}
/* This function configures a Zorro III board, based on the initialization
data in its ConfigDev structure. */
void ConfigZ3Board(cd)
struct ConfigDev *cd;
{
    APTR base = cd->cd_BoardAddr;
    UWORD sizecode,extended,subsize;
    ULONG physsize,logsize;
    char *memname;
    /* First examine the physical sizing of the board. */
    sizecode = cd->cd_Rom.er_Type & ERT_MEMSIZE;
    extended = ((cd->cd_Rom.er_Flags & ERFF_EXTENDED) != 0);
    physsize = BoardSize[extended][sizecode];
    cd->cd_BoardAddr = (APTR)Z3Space;
    cd->cd_BoardSize = (APTR)physsize;
    cd->cd_SlotAddr = (Z3Space-E_Z3EXPSTART)/E_Z3SLOTSIZE;
    cd->cd_SlotSize = ((physsize/E_Z3SLOTSIZE)>0)?(physsize/E_Z3SLOTSIZE):1;
    Z3Space += cd->cd_SlotSize * E_Z3SLOTSIZE;
    /* Next, process the sub-size, if any. */
    if (subsize = (cd->cd_Flags & ERF_SUBMASK))
    cd->cd_BoardSize = (APTR)SubSize[subsize];
    if (verbose) {
        printf(" BOARD STATS:\n");
        printf(" ADDRESS: $%lx\n",cd->cd_BoardAddr);
        if (cd->cd_BoardSize)
        printf(" SIZE: $%lx\n",cd->cd_BoardSize);
        else
        printf(" SIZE: AUTOMATIC => \n");
    }
    /* Now, configure the board. */
    WriteCfgAddr(base,cd);

    if (!cd->cd_BoardSize) {
        AutoSizeBoard(cd);
        printf("$%lx",cd->cd_BoardSize);
    }
    if (cd->cd_BoardSize && (cd->cd_Rom.er_Type & ERTF_MEMLIST)) {
        strcpy(memname = (char *)AllocMem(20L,MEMF_CLEAR),"Zorro III Memory");
        AddMemList(cd->cd_BoardSize,MEMF_FAST|MEMF_PUBLIC,10,cd->cd_BoardAddr,memname);
    }
    AddConfigDev(cd);
    exit(0); // Stop after first PIC
}
/* ====================================================================== */
/* This is the main program. */
void main(argc,argv)
int argc;
char *argv[];
{
    int i;
    struct ConfigDev *cd;
    if (!(ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library",0L))) {
        printf("Error: Can't open expansion.library\n");
        exit(10);
    }
    if (argc > 1)
    for (i = 1; i < argc; ++i) switch (toupper(argv[i][0])) {
        case 'Q': verbose = FALSE; break;
        case 'V': verbose = TRUE; break;
    }

    // printf("Making TF card go away...\n");
    // UBYTE *bytebase = (UBYTE *)(0x00e80048);
    // (*(bytebase)) = 0xFF;

    while (cd = FindZ3Board()) ConfigZ3Board(cd);
    if (!anyone) PRVB("No PICs left to configure\n");
    CloseLibrary((struct ExpansionBase *)ExpansionBase);
}