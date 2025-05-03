#include "player-paula.h"

class PlayerPaulaMono8 : public PlayerPaula
{
	BYTE *buf0;
	BYTE *buf1;

	public:

	BOOL ready;
	PlayerPaulaMono8(LONG frequency);
	~PlayerPaulaMono8();
	//BYTE* GetBuffer();
};
