#include <exec/libraries.h>

#define QOAPLAY_VERSION "0.1"
#define QOAPLAY_DATE "01.05.2025"

#ifdef DEBUG
#define D(args...) Printf(args)
#else
#define D(args...)
#endif

#define divu16(a,b) ({ \
UWORD _r, _b = (b); \
ULONG _a = (a); \
asm("DIVU.W %2,%0": "=d" (_r): "0" (_a), "dmi" (_b): "cc"); \
_r;})

extern Library *SysBase, *DOSBase, *LocaleBase, *UtilityBase;

BOOL Problem(LONG error);
