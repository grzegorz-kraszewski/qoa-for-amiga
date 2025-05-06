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

	// initialize AudioBlocks
	
	for (WORD i = 1; i >= 0; i--)
	{
		blocks[i].reqL = NULL;
		blocks[i].reqR = NULL;
		blocks[i].bufL = NULL;
		blocks[i].bufR = NULL;
		blocks[i].statusL = ABLOCK_UNUSED;
		blocks[i].statusR = ABLOCK_UNUSED;
	}

	if (((ExecBase*)SysBase)->VBlankFrequency == 60) masterclock =  3579545;   // NTSC
	period = divu16(masterclock, frequency);
	D("PlayerPaula[%08lx]: masterclock = %lu, using period %lu.\n", this, masterclock, period);

	if (port = CreateMsgPort())
	{
		for (WORD i = 1; i >= 0; i--)
		{
			blocks[i].reqL = (IOAudio*)CreateIORequest(port, sizeof(IOAudio));
			blocks[i].reqR = (IOAudio*)CreateIORequest(port, sizeof(IOAudio));

			if (blocks[i].reqL && blocks[i].reqR) continue;
			else
			{
				ready = FALSE;
				break;
			}

			D("PlayerPaula[$%08lx]: block[%ld], L iorequest created at $%08lx.\n",
				this, i, blocks[i].reqL);
			D("PlayerPaula[$%08lx]: block[%ld], R iorequest created at $%08lx.\n",
				this, i, blocks[i].reqR);
		}

		if (ready)
		{
			IOAudio *req0 = blocks[0].reqL;

			InitReq0(req0);
			err = OpenDevice("audio.device", 0, (IORequest*)req0, 0);

			D("PlayerPaula[$%08lx]: OpenDevice result %ld, channel mask $%lx.\n", this, err,
				req0->ioa_Request.io_Unit);

			if (!err)
			{
				devopen = TRUE;
				left = (UBYTE)req0->ioa_Request.io_Unit & PAULA_LEFT_CHANNELS;
				right = (UBYTE)req0->ioa_Request.io_Unit & PAULA_RIGHT_CHANNELS;

				D("PlayerPaula[$%08lx]: using $%ld as left channel, $%ld as right channel.\n",
					this, left, right);

				InitReqClone(req0, blocks[0].reqL, left);
				InitReqClone(req0, blocks[0].reqR, right);
				InitReqClone(req0, blocks[1].reqL, left);
				InitReqClone(req0, blocks[1].reqR, right);
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
		CloseDevice((IORequest*)blocks[0].reqL);
		D("PlayerPaula[$%08lx]: device closed.\n", this);
	}

	for (WORD i = 1; i >= 0; i--)
	{
		if (blocks[i].reqL)
		{
			DeleteIORequest((IORequest*)blocks[i].reqL);
			D("PlayerPaula[$%08lx]: request $%08lx deleted.\n", this, blocks[i].reqL);
		}

		if (blocks[i].reqR)
		{
			DeleteIORequest((IORequest*)blocks[i].reqR);
			D("PlayerPaula[$%08lx]: request $%08lx deleted.\n", this, blocks[i].reqR);
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

void PlayerPaula::InitReq0(IOAudio *req)
{
	req->ioa_AllocKey = 0;
	req->ioa_Data = ChannelMatrix;
	req->ioa_Length = sizeof(ChannelMatrix);
	req->ioa_Request.io_Message.mn_Node.ln_Pri = 120;
}


void PlayerPaula::InitReqClone(IOAudio *src, IOAudio *clone, UBYTE side)
{
	clone->ioa_AllocKey = src->ioa_AllocKey;
	clone->ioa_Request.io_Unit = (Unit*)side;
	clone->ioa_Cycles = 1;
	clone->ioa_Period = period;
	clone->ioa_Volume = 64;
}


void PlayerPaula::AudioProblem(LONG err)
{
	if (err == IOERR_OPENFAIL) Problem(E_APP_AUDIO_DEVICE_FAILED);
	else if (err = ADIOERR_ALLOCFAILED) Problem(E_APP_AUDIO_NO_CHANNELS);
}
