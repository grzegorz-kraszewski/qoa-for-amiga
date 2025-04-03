#include "main.h"
#include "timing.h"

TimerDevice::TimerDevice()
{
	UQUAD dummy;

	ready = FALSE;
	if (!(port = CreateMsgPort())) return;
	if (!(req = (TimeRequest*)CreateIORequest(port, sizeof(TimeRequest)))) return;
	if (OpenDevice("timer.device", UNIT_VBLANK, (IORequest*)req, 0) != 0) return;
	TimerBase = &req->tr_node.io_Device->dd_Library;
	eClock = ReadEClock((EClockVal*)&dummy);
	ready = TRUE;
}

TimerDevice::~TimerDevice()
{
	if (ready) CloseDevice((IORequest*)req);
	if (req) DeleteIORequest(req);
	if (port) DeleteMsgPort(port);
}