#include <proto/locale.h>

extern Library *LocaleBase;
extern Catalog *Cat;

static inline char* LS(LONG id, char* defstr)
{
	return (Cat ? GetCatalogStr(Cat, id, defstr) : defstr);
}

// texts in ErrorMessages[]

#define MSG_ERR_QOA_FILE_TOO_SHORT                     0
#define MSG_ERR_QOA_FILE_TOO_BIG                       1
#define MSG_ERR_NOT_QOA_FILE                           2
#define MSG_ERR_ZERO_SAMPLES_SPECIFIED                 3
#define MSG_ERR_ZERO_AUDIO_CHANNELS_SPECIFIED          4
#define MSG_ERR_MULTICHANNEL_NOT_SUPPORTED             5
#define MSG_ERR_ZERO_SAMPLING_RATE                     6
#define MSG_ERR_QOA_FILE_SHORTER_THAN_EXPECTED         7
#define MSG_ERR_QOA_FILE_LONGER_THAN_EXPECTED          8
#define MSG_ERR_VARIABLE_CHANNELS_NOT_SUPPORTED        9
#define MSG_ERR_VARIABLE_SAMPLERATE_NOT_SUPPORTED     10
#define MSG_ERR_ZERO_SAMPLES_FRAME                    11
#define MSG_ERR_OVERLONG_FRAME                        12
#define MSG_ERR_FRAME_SIZE_MISMATCH                   13
#define MSG_ERR_UNEXPECTED_PARTIAL_FRAME              14
#define MSG_ERR_CANT_OPEN_UTILITY_LIBRARY             15
#define MSG_ERR_CANT_OPEN_MATHIEEE_LIBRARY            16
#define MSG_COMMANDLINE_ARGUMENTS                     17
#define MSG_OUT_OF_MEMORY                             18

// other texts

#define MSG_UNEXPECTED_END_OF_FILE                    19
#define MSG_DECODING_PROGRESS_INDICATOR               20
#define MSG_CONVERSION_ABORTED                        21
#define MSG_TIMING_REPORT                             22
#define MSG_DECODING_SPEED_REPORT                     23
#define MSG_QOA_STREAM_INFO                           24
#define MSG_QOA_STREAM_MONO                           25
#define MSG_QOA_STREAM_STEREO                         26
