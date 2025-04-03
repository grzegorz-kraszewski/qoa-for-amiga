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
	ULONG eClock;

	TimerDevice();
	~TimerDevice();
	Library* base() { return libbase; }
};
