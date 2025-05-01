/* error codes */

#define IOERR                    0x10000    /* append DOS fault message */
#define FEOF                     0x20000    /* unexpected end of file if IoErr() == 0 */

#define E_QOA_LESS_THAN_40_BYTES       0    /* QOA file can't be shorter */
#define E_QOA_FILE_TOO_BIG             1    /* won't fit to 2 GB AIFF file */
#define E_QOA_NO_QOAF_MARKER           2    /* file does not start with 'qoaf' */
#define E_QOA_ZERO_SAMPLES             3    /* zero samples specified in header */
#define E_QOA_ZERO_CHANNELS            4    /* zero audio channels specifed in the first frame */
#define E_QOA_TOO_MANY_CHANNELS        5    /* QoaToAiff only supports 1/2 channels */
#define E_QOA_ZERO_SAMPLING_RATE       6    /* 0 Hz sampling rate specified */
#define E_QOA_FILE_TOO_SHORT           7    /* real file length lower than calculated */
#define E_QOA_FILE_EXTRA_DATA          8    /* real file length higher than calculated */
#define E_QOA_VARIABLE_CHANNELS        9    /* variable channels frame detected */
#define E_QOA_VARIABLE_SAMPLING       10    /* variable sampling rate frame detected */
#define E_QOA_ZERO_SAMPLES_FRAME      11    /* frame with zero samples specified */
#define E_QOA_TOO_MANY_SAMPLES        12    /* too many samples in a frame specified */
#define E_QOA_WRONG_FRAME_SIZE        13    /* specified frame size different from calculated */
#define E_QOA_UNEXP_PARTIAL_FRAME     14    /* partial frame too early in stream */
#define E_APP_NO_UTILITY_LIBRARY      15    /* can't open utility.library v39+ */
#define E_APP_NO_MATHIEEE_LIBRARY     16    /* can't open mathieeesingbas.library */
#define E_APP_COMMANDLINE_ARGS        17    /* wrong commandline arguments */
#define E_APP_OUT_OF_MEMORY           18    /* some allocation failed */

// end of codes directly related to ErrorMessages[]

#define E_ENTRY_COUNT                 19

// codes below have no entry in ErrorMessages[]

#define E_APP_FILE                    19    /* DOS problem with a file */
