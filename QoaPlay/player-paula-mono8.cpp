#include "player-paula-mono8.h"
#include "main.h"

#include <proto/exec.h>

#define PLAYBACK_BUFFER_SIZE 5120  // one decoded QOA frame


// This subclass allocates two buffers in chip RAM

PlayerPaulaMono8::PlayerPaulaMono8(LONG frequency) : PlayerPaula(frequency)
{
	if (ready)
	{
		ready = FALSE;
		buf0 = (BYTE*)AllocMem(PLAYBACK_BUFFER_SIZE * 2, MEMF_CHIP);

		if (buf0)
		{
			ready = TRUE;
			buf1 = buf0 + PLAYBACK_BUFFER_SIZE;
			D("PlayerPaulaMono8[$%08lx]: buf0 = $%08lx, buf1 = $%08lx.\n", this, buf0, buf1);
		}
	}
}


PlayerPaulaMono8::~PlayerPaulaMono8()
{
	if (buf0) FreeMem(buf0, PLAYBACK_BUFFER_SIZE * 2);
	D("PlayerPaulaMono8[$%08lx]: chip RAM buffers at $%08lx freed.\n", this, buf0);
}


BYTE* PlayerPaulaMono8::GetBuffer()
{
	// if buf0 free return it
	// else if buf1 free return it
	// else Wait() for returning IORequests (L+R) and return their buffer
	// if Wait() returned on CTRL+C return NULL
}


BOOL PlayerPaulaMono8::BufferFilled(LONG samples)
{
	// fill IoRequests for buffer passed lately with GetBuffer()
	// CMD_WRITE them
}
