#include <exec/libraries.h>

extern Library *SysBase, *DOSBase, *LocaleBase, *TimerBase;

extern "C"
{
	float floorf(float x);
}