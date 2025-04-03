#include <proto/exec.h>
#include <proto/timer.h>
#include <exec/io.h>
#include <devices/timer.h>

#include "main.h"


class TimerDevice
{
	private:

	MsgPort *port;
	TimeRequest *req;
	Library *libbase;

	public:

	BOOL ready;
	static ULONG eClock;

	TimerDevice();
	~TimerDevice();
	Library* base() { return libbase; }
};


class StopWatch
{
	UQUAD last;

	public:

	UQUAD total;

	StopWatch() { total = 0; }
	void start() { ReadEClock((EClockVal*)&last); }
	void stop();
};
