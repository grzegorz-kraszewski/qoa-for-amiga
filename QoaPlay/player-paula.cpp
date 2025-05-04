#include "main.h"
#include "errors.h"
#include "player-paula.h"

#include <proto/exec.h>
#include <exec/errors.h>
#include <exec/execbase.h>

#define PAULA_LEFT_CHANNELS 6     // 0110
#define PAULA_RIGHT_CHANNELS 9    // 1001


PlayerPaula::PlayerPaula(LONG frequency)
{
	LONG err;
	ULONG masterclock = 3546895;   // PAL

	ready = TRUE;
	port = NULL;
	devopen = FALSE;
	for (WORD i = 0; i < 4; i++) reqs[i] = NULL;
	if (((ExecBase*)SysBase)->VBlankFrequency == 60) masterclock =  3579545;   // NTSC
	period = divu16(masterclock, frequency);
	D("PlayerPaula[%08lx]: masterclock = %lu, using period %lu.\n", this, masterclock, period);

	if (port = CreateMsgPort())
	{
		for (WORD i = 3; i >= 0; i--)
		{
			if (!(reqs[i] = (IOAudio*)CreateIORequest(port, sizeof(IOAudio))))
			{
				ready = FALSE;
				break;
			}

			D("PlayerPaula[$%08lx]: reqs[%ld] created at $%08lx.\n", this, i, reqs[i]);
		}

		if (ready)
		{
			InitReq0();
			err = OpenDevice("audio.device", 0, (IORequest*)reqs[0], 0);
			D("PlayerPaula[$%08lx]: OpenDevice result %ld, channel mask $%lx.\n", this, err,
				reqs[0]->ioa_Request.io_Unit);
			
			if (!err)
			{
				devopen = TRUE;
				left = (UBYTE)reqs[0]->ioa_Request.io_Unit & PAULA_LEFT_CHANNELS;
				right = (UBYTE)reqs[0]->ioa_Request.io_Unit & PAULA_RIGHT_CHANNELS;
				D("PlayerPaula[$%08lx]: using $%ld as left channel, $%ld as right channel.\n",
					this, left, right);
				InitReqClones();
			}
			else
			{
				ready = FALSE;
				AudioProblem(err);
			}
		}
	}
}


PlayerPaula::~PlayerPaula()
{
	if (devopen)
	{
		CloseDevice((IORequest*)reqs[0]);
		D("PlayerPaula[$%08lx]: device closed.\n", this);
	}

	for (WORD i = 3; i >= 0; i--)
	{
		if (reqs[i])
		{
			DeleteIORequest((IORequest*)reqs[i]);
			D("PlayerPaula[$%08lx]: request $%08lx deleted.\n", this, reqs[i]);
		}
	}

	if (port)
	{
		DeleteMsgPort(port);
		D("PlayerPaula[$%08lx]: MsgPort $%08lx deleted.\n", this, port);
	}
}


// All possible combinations of one L and one R Paula channel.

static UBYTE ChannelMatrix[4] = { 3, 5, 10, 12 };


// Initializes IORequest for device opening (channel allocation is done at opening)

void PlayerPaula::InitReq0()
{
	reqs[0]->ioa_AllocKey = 0;
	reqs[0]->ioa_Data = ChannelMatrix;
	reqs[0]->ioa_Length = sizeof(ChannelMatrix);
	reqs[0]->ioa_Request.io_Message.mn_Node.ln_Pri = 120;
}

// IoRequests 0 and 2 are for stereo L
// IoRequests 1 and 3 are for stereo R

void PlayerPaula::InitReqClones()
{
	reqs[1]->ioa_AllocKey = reqs[0]->ioa_AllocKey;
	reqs[2]->ioa_AllocKey = reqs[0]->ioa_AllocKey;
	reqs[3]->ioa_AllocKey = reqs[0]->ioa_AllocKey;
	reqs[0]->ioa_Request.io_Unit = (Unit*)left;
	reqs[1]->ioa_Request.io_Unit = (Unit*)right;
	reqs[2]->ioa_Request.io_Unit = (Unit*)left;
	reqs[3]->ioa_Request.io_Unit = (Unit*)right;

	for (WORD i = 3; i >= 0; i--)
	{
		reqs[i]->ioa_Cycles = 1;
		reqs[i]->ioa_Period = period;
		reqs[i]->ioa_Volume = 64;
	}
}

void PlayerPaula::AudioProblem(LONG err)
{
	if (err == IOERR_OPENFAIL) Problem(E_APP_AUDIO_DEVICE_FAILED);
	else if (err = ADIOERR_ALLOCFAILED) Problem(E_APP_AUDIO_NO_CHANNELS);
}
