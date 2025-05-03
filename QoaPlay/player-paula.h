#include <devices/audio.h>

// Regardless of mono / stereo, I will need two Paula channels (one left, one right), as mono
// files are played on both sides. As I use doublebuffering, 4 IORequests will be needed.
// However I only need two chip-RAM buffers for mono (I can pass the same data to two IORequests)
// and four buffers for stereo. That is why buffers are not allocated here, but in subclasses.

class PlayerPaula
{
	protected:

	UWORD period;
	UBYTE left;                // single channel mask for stereo L
	UBYTE right;               // single channel mask for stereo R
	MsgPort *port;
	IOAudio *reqs[4];
	void AudioProblem(LONG error);
	void InitReq0();
	BOOL ready;
	PlayerPaula(LONG frequency);
	~PlayerPaula();
};
