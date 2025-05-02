#include "locale.h"
#include "nicenumber.h"

#include <proto/exec.h>
#include <libraries/locale.h>


static void ProcPutChar(void)
{
	asm("move.b d0,(a3)+");
}


static void VFmtPut(STRPTR dest, STRPTR fmt, LONG *args)
{
	RawDoFmt(fmt, args, ProcPutChar, dest);
}


static void FmtPut(STRPTR dest, STRPTR fmt, ...)
{
	LONG *_args = ((LONG*)&fmt) + 1;
	VFmtPut(dest, fmt, _args);
}


static void FsProcCountChars()
{
	asm("addq.l #1,16(a0)");
}


static void FsProcPutChars()
{
	asm("move.w a1,d0; movea.l 16(a0),a1; move.b d0,(a1)+; move.l a1,16(a0)");
}


static LONG VLocFmtSize(Locale *locale, STRPTR str, ULONG *data)
{
	Hook counthook;

	counthook.h_Entry = (HOOKFUNC)FsProcCountChars;
	counthook.h_Data = 0;                               // the character counter

	FormatString(locale, str, data, &counthook);
	return (LONG)counthook.h_Data;
}


static LONG LocFmtSize(Locale *locale, STRPTR str, ...)
{
	ULONG *data = ((ULONG*)&str) + 1;
	return VLocFmtSize(locale, str, data);
}


static STRPTR VLocFmtPut(Locale *locale, STRPTR str, STRPTR dest, ULONG *data)
{
	Hook puthook;

	puthook.h_Entry = (HOOKFUNC)FsProcPutChars;
	puthook.h_Data = dest;

	FormatString(locale, str, data, &puthook);
	return (STRPTR)puthook.h_Data - 1;
}


static STRPTR LocFmtPut(Locale *locale, STRPTR str, STRPTR dest, ...)
{
	ULONG *data = ((ULONG*)&dest) + 1;
	return VLocFmtPut(locale, str, dest, data);
}


NiceInt::NiceInt()
{
	LONG bufsize = 12;   // no digit grouping, takes '-' and terminator into account

	if (Loc) bufsize = LocFmtSize(Loc, "%lD", -1234567890);
	buffer = new char[bufsize];
}


NiceInt::~NiceInt()
{
	if (buffer) delete buffer;
}


char* NiceInt::GetString(LONG x)
{
	if (!buffer) return "";
	if (Loc) LocFmtPut(Loc, "%lD", buffer, x);
	else FmtPut(buffer, "%ld", x);
	return buffer;
}


// The class assumes that integer number of seconds is less than 2^32, which is always true for
// duration of QOA file.

NiceTime::NiceTime()
{
	LONG bufsize = 14; // no digit grouping, 10 chars for ULONG, 1 for decimal point, 2 for fractional part

	if (Loc) bufsize = LocFmtSize(Loc, "%lD%s%02ld", 1234567890, Loc->loc_DecimalPoint, 99);
	buffer = new char[bufsize];
}


NiceTime::~NiceTime()
{
	if (buffer) delete buffer;
}


STRPTR NiceTime::GetString(float x)
{
	LONG seconds, hundredths;

	if (!buffer) return "";
	seconds = (LONG)x;
	hundredths = (LONG)(fract(x) * 100.0f);
	if (Loc) LocFmtPut(Loc, "%lD%s%02ld", buffer, seconds, Loc->loc_DecimalPoint, hundredths);
	else FmtPut(buffer, "%ld.%02ld", seconds, hundredths);
	return buffer;
}
