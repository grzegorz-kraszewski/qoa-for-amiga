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
	EClockVal last;

	public:

	EClockVal total;

	StopWatch() { total.ev_hi = 0; total.ev_lo = 0; }
	void start() { ReadEClock(&last); }
	void stop();
};
