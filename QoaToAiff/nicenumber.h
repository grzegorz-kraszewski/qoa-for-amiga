#include "main.h"

class NiceInt
{
	STRPTR buffer;

	public:

	NiceInt();
	~NiceInt();
	STRPTR GetString(LONG x);
};


class NiceTime
{
	STRPTR buffer;

	public:

	NiceTime();
	~NiceTime();
	STRPTR GetString(float x);
};
