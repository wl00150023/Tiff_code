#include "windef.h"

#define LZW_MAX_BIT		12
#define LZW_MIN_BIT		9
#define CLEAR_CODE		256
#define END_CODE		257
#define MAX_DIC_SIZE	4094

#define NULL_CODE -1

struct Code
{
	int pre ;		// PreCode;
	int head;		// HeadCodeOfPreCodeIsItself;
	int next;		// NextCodeOfHaveSamePreCode;
	int str;
	int str_size;
};

class Lzw
{
private:
	Code code_list[1<<LZW_MAX_BIT];
	int code_size;
	Uint8 encode_bit;
	Uint16 pre_index;
	Uint32 pre_size;

	void init();
	void table_reset();
	void deinit();
	int AddCode( int pre_code, int code );
	int FindStr(int pre_code, int code);
	int ReadCode(Uint8 **inbuf);
	int WriteValue(Uint8 **outbuf, int code);
	int WriteCode(Uint8 **outbuf, Uint16 index);
	int WriteCodeFlush(Uint8 **outbuf);
	int GetFirstCode(int code);
	int CheckEncodeListFull();
	int CheckDecodeListFull();
public :
	int Encode(Uint8 *inbuf, Uint8 *outbuf, Uint32 inbuf_size);
	int Decode(Uint8 *inbuf, Uint8 *outbuf, Uint32 outbuf_size);
};