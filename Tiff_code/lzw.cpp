/******************************************************************************
**  LZW compression
**  --------------------------------------------------------------------------
**  
**  Compresses data using LZW algorithm.
**  
**  Author: V.Antonenko
**
**
******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#define DICT_SIZE       (1 << 20)
#define NODE_NULL       (-1)

typedef unsigned int code_t;

typedef struct _bitbuffer
{
        unsigned buf;
        unsigned n;
}
bitbuffer_t;

// LZW node, represents a string
typedef struct _node
{
        code_t  prev;   // prefix code
        code_t  first;  // firts child code
        code_t  next;   // next child code
        int    ch;             // last symbol
}
node_t;

// LZW context
typedef struct _lzw
{
        node_t        dict[DICT_SIZE];  // code dictionary
        unsigned      max;                              // maximal code
        unsigned      codesize;                 // number of bits in code
        bitbuffer_t   bb;                               // bit-buffer struct
        void          *stream;                  // pointer to the stream object
        unsigned      lzwn;                             // buffer byte counter
        unsigned      lzwm;                             // buffer size (decoder only)
        unsigned char buff[256];                // stream buffer, adjust its size if you need
}
lzw_t;

// Application defined stream callbacks
void lzw_writebuf(void *stream, unsigned char *buf, unsigned size);
unsigned lzw_readbuf(void *stream, unsigned char *buf, unsigned size);

static void lzw_writebyte(lzw_t *ctx, const unsigned char b)
{
        ctx->buff[ctx->lzwn++] = b;

        if (ctx->lzwn == sizeof(ctx->buff)) {
                ctx->lzwn = 0;
                lzw_writebuf(ctx->stream, ctx->buff, sizeof(ctx->buff));
        }
}

static unsigned char lzw_readbyte(lzw_t *ctx)
{
        if (ctx->lzwn == ctx->lzwm)
        {
                ctx->lzwm = lzw_readbuf(ctx->stream, ctx->buff, sizeof(ctx->buff));
                ctx->lzwn = 0;
        }

        return ctx->buff[ctx->lzwn++];
}

/******************************************************************************
**  lzw_writebits
**  --------------------------------------------------------------------------
**  Write bits into bit-buffer.
**  The number of bits should not exceed 24.
**  
**  Arguments:
**      ctx     - pointer to LZW context;
**      bits    - bits to write;
**      nbits   - number of bits to write, 0-24;
**
**  Return: -
******************************************************************************/
static void lzw_writebits(lzw_t *ctx, unsigned bits, unsigned nbits)
{
        // shift old bits to the left, add new to the right
        ctx->bb.buf = (ctx->bb.buf << nbits) | (bits & ((1 << nbits)-1));

        nbits += ctx->bb.n;

        // flush whole bytes
        while (nbits >= 8) {
                unsigned char b;

                nbits -= 8;
                b = ctx->bb.buf >> nbits;

                lzw_writebyte(ctx, b);
        }

        ctx->bb.n = nbits;
}

/******************************************************************************
**  lzw_readbits
**  --------------------------------------------------------------------------
**  Read bits from bit-buffer.
**  The number of bits should not exceed 24.
**  
**  Arguments:
**      ctx     - pointer to LZW context;
**      nbits   - number of bits to read, 0-24;
**
**  Return: bits
******************************************************************************/
static unsigned lzw_readbits(lzw_t *ctx, unsigned nbits)
{
        unsigned bits;

        // read bytes
        while (ctx->bb.n < nbits) {
                unsigned char b = lzw_readbyte(ctx);

                // shift old bits to the left, add new to the right
                ctx->bb.buf = (ctx->bb.buf << 8) | b;
                ctx->bb.n += 8;
        }

        ctx->bb.n -= nbits;
        bits = (ctx->bb.buf >> ctx->bb.n) & ((1 << nbits)-1);

        return bits;
}

/******************************************************************************
**  lzw_flushbits
**  --------------------------------------------------------------------------
**  Flush bits into bit-buffer.
**  If there is not an integer number of bytes in bit-buffer - add zero bits
**  and write these bytes.
**  
**  Arguments:
**      pbb     - pointer to bit-buffer context;
**
**  Return: -
******************************************************************************/
static void lzw_flushbits(lzw_t *ctx)
{
        if (ctx->bb.n & 3)
                lzw_writebits(ctx, 0, 8-(ctx->bb.n & 3));
}


/******************************************************************************
**  lzw_init
**  --------------------------------------------------------------------------
**  Initializes LZW context.
**  
**  Arguments:
**      ctx     - LZW context;
**      stream  - Pointer to Input/Output stream object;
**
**  RETURN: -
******************************************************************************/
void lzw_init(lzw_t *ctx, void *stream)
{
        unsigned i;

        memset(ctx, 0, sizeof(*ctx));
       
        for (i = 0; i < 258; i++)
        {
                ctx->dict[i].prev = NODE_NULL;
                ctx->dict[i].first = NODE_NULL;
                ctx->dict[i].next = NODE_NULL;
                ctx->dict[i].ch = i;
        }

        ctx->max = i-1;
        ctx->codesize = 9;
        ctx->stream = stream;
}

/******************************************************************************
**  lzw_find_str
**  --------------------------------------------------------------------------
**  Searches a string in LZW dictionaly. It is used only in encoder.
**  
**  Arguments:
**      ctx  - LZW context;
**      code - code for the string beginning (already in dictionary);
**      c    - last symbol;
**
**  RETURN: code representing the string or NODE_NULL.
******************************************************************************/
static code_t lzw_find_str(lzw_t *ctx, code_t code, char c)
{
        code_t nc;

        for (nc = ctx->dict[code].first; nc != NODE_NULL; nc = ctx->dict[nc].next)
        {
                if (code == ctx->dict[nc].prev && c == ctx->dict[nc].ch)
                        return nc;
        }

        return NODE_NULL;
}

/******************************************************************************
**  lzw_get_str
**  --------------------------------------------------------------------------
**  Reads string from the LZW dictionaly. Because of particular dictionaty
**  structure the buffer is filled from the end so the offset from the
**  beginning of the buffer will be <buffer size> - <string size>.
**  
**  Arguments:
**      ctx  - LZW context;
**      code - code of the string (already in dictionary);
**      buff - buffer for the string;
**      size - the buffer size;
**
**  Return: the number of bytes in the string
******************************************************************************/
static unsigned lzw_get_str(lzw_t *ctx, code_t code, unsigned char buff[], unsigned size)
{
        unsigned i = size;

        while (code != NODE_NULL && i)
        {
                buff[--i] = ctx->dict[code].ch;
                code = ctx->dict[code].prev;
        }

        return size - i;
}

/******************************************************************************
**  lzw_add_str
**  --------------------------------------------------------------------------
**  Adds string to the LZW dictionaly.
**  It is important that codesize is increased after the code was sent into
**  the output stream.
**  
**  Arguments:
**      ctx  - LZW context;
**      code - code for the string beginning (already in dictionary);
**      c    - last symbol;
**
**  RETURN: code representing the string or NODE_NULL if dictionary is full.
******************************************************************************/
static code_t lzw_add_str(lzw_t *ctx, code_t code, char c)
{
        ctx->max++;

        if (ctx->max >= DICT_SIZE)
                return NODE_NULL;
       
        ctx->dict[ctx->max].prev = code;
        ctx->dict[ctx->max].first = NODE_NULL;
        ctx->dict[ctx->max].next = ctx->dict[code].first;
        ctx->dict[code].first = ctx->max;
        ctx->dict[ctx->max].ch = c;

        return ctx->max;
}

/******************************************************************************
**  lzw_write
**  --------------------------------------------------------------------------
**  Writes an output code into the stream.
**  This function is used only in encoder.
**  
**  Arguments:
**      ctx  - LZW context;
**      code - code for the string;
**
**  RETURN: -
******************************************************************************/
static void lzw_write(lzw_t *ctx, code_t code)
{
        // increase the code size (number of bits) if needed
        if (ctx->max == (1 << ctx->codesize))
                ctx->codesize++;

        lzw_writebits(ctx, code, ctx->codesize);
}

/******************************************************************************
**  lzw_read
**  --------------------------------------------------------------------------
**  Reads a code from the input stream. Be careful about where you put its
**  call because this function changes the codesize.
**  This function is used only in decoder.
**  
**  Arguments:
**      ctx  - LZW context;
**
**  RETURN: code
******************************************************************************/
static code_t lzw_read(lzw_t *ctx)
{
        // increase the code size (number of bits) if needed
        if (ctx->max+1 == (1 << ctx->codesize))
                ctx->codesize++;

        return lzw_readbits(ctx, ctx->codesize);
}

/******************************************************************************
**  lzw_encode
**  --------------------------------------------------------------------------
**  Encodes input byte stream into LZW code stream.
**  
**  Arguments:
**      ctx  - LZW context;
**      fin  - input file;
**      fout - output file;
**
**  Return: error code
******************************************************************************/
int lzw_encode(lzw_t *ctx, FILE *fin, FILE *fout)
{
        code_t   code;
        unsigned isize = 0;
        unsigned strlen = 0;
        int      c;

        lzw_init(ctx, fout);

        if ((c = fgetc(fin)) == EOF)
                return -1;

        code = c;
        isize++;
        strlen++;

        while ((c = fgetc(fin)) != EOF)
        {
                code_t nc;

                isize++;

                nc = lzw_find_str(ctx, code, c);

                if (nc == NODE_NULL)
                {
                        code_t tmp;

                        // the string was not found - write <prefix>
                        lzw_write(ctx, code);

                        // add <prefix>+<current symbol> to the dictionary
                        tmp = lzw_add_str(ctx, code, c);

                        if (tmp == NODE_NULL) {
                                fprintf(stderr, "ERROR: dictionary is full, input %d\n", isize);
                                break;
                        }

                        code = c;
                        strlen = 1;
                }
                else
                {
                        code = nc;
                        strlen++;
                }
        }
        // write last code
        lzw_write(ctx, code);

        lzw_flushbits(ctx);
        lzw_writebuf(ctx->stream, ctx->buff, ctx->lzwn);

        return 0;
}

unsigned char buff[DICT_SIZE];

/******************************************************************************
**  lzw_decode
**  --------------------------------------------------------------------------
**  Decodes input LZW code stream into byte stream.
**  
**  Arguments:
**      ctx  - LZW context;
**      fin  - input file;
**      fout - output file;
**
**  Return: error code
******************************************************************************/
int lzw_decode(lzw_t *ctx, FILE *fin, FILE *fout)
{
        unsigned      isize = 0;
        code_t        code;
        unsigned char c;

        lzw_init(ctx, fin);

        c = code = lzw_readbits(ctx, ctx->codesize);

		code = lzw_read(ctx);
        // write symbol into the output stream
        fwrite(&code, 1, 1, fout);

        for(;;)
        {
                unsigned      strlen;
                code_t        nc;

                nc = lzw_read(ctx);
				if( nc == 257 )
					break;

                // check input strean for EOF (lzwm == 0)
                if (!ctx->lzwm)
                        break;

                // unknown code
                if (nc > ctx->max)
                {
                        if (nc-1 == ctx->max) {
                                //fprintf(stderr, "Create code %d = %d + %c\n", nc, code, c);
                                if (lzw_add_str(ctx, code, c) == NODE_NULL) {
                                        fprintf(stderr, "ERROR: dictionary is full, input %d\n", isize);
                                        break;
                                }
                                code = NODE_NULL;
                        }
                        else {
                                fprintf(stderr, "ERROR: wrong code %d, input %d\n", nc, isize);
                                break;
                        }
                }

                // get string for the new code from dictionary
                strlen = lzw_get_str(ctx, nc, buff, sizeof(buff));
                // remember the first sybmol of this string
                c = buff[sizeof(buff) - strlen];
                // write the string into the output stream
                fwrite(buff+(sizeof(buff) - strlen), strlen, 1, fout);

                if (code != NODE_NULL)
                {
                        // add <prev code str>+<first str symbol> to the dictionary
                        if (lzw_add_str(ctx, code, c) == NODE_NULL) {
                                fprintf(stderr, "ERROR: dictionary is full, input %d\n", isize);
                                break;
                        }
                }

                code = nc;
                isize++;
        }

        return 0;
}

// global object
lzw_t lzw;

void lzw_writebuf(void *stream, unsigned char *buf, unsigned size)
{
        fwrite(buf, size, 1, (FILE*)stream);
}

unsigned lzw_readbuf(void *stream, unsigned char *buf, unsigned size)
{
        return fread(buf, 1, size, (FILE*)stream);
}


//**************************************************************************************************************//
//																												//
//												Lzw class														//
//																												//
//**************************************************************************************************************//
#include "lzw.h"
void Lzw::init()
{
	table_reset();

	pre_index = 0;
	pre_size = 0;
}

void Lzw::table_reset()
{
	for( int i = 0 ; i < 258 ; ++i )
	{
		code_list[i].pre = NULL_CODE;
		code_list[i].head = NULL_CODE;
		code_list[i].next = NULL_CODE;
		code_list[i].str = i;
		code_list[i].str_size = 1;
	}

	code_size = 258;

	encode_bit = LZW_MIN_BIT;
}

void Lzw::deinit()
{
}

int Lzw::AddCode( int pre_code, int code )
{
	code_list[code_size].pre = pre_code;
	code_list[code_size].next = code_list[pre_code].head;
	code_list[code_size].head = NULL_CODE;
	code_list[code_size].str = code;
	code_list[code_size].str_size = code_list[pre_code].str_size + 1;

	code_list[pre_code].head = code_size;
	++code_size;

	return code_size-1;
}

int Lzw::FindStr(int pre_code, int code)
{
	if( pre_code == NULL_CODE )
	{
		return code;
	}

	int temp;
	for( temp = code_list[pre_code].head ; temp != NULL_CODE ; temp = code_list[temp].next )
	{
		if( code_list[temp].str == code )
		{
			return temp;
		}
	}

	return NULL_CODE;
}

int Lzw::WriteCode(Uint8 **outbuf, Uint16 index)
{
	Uint32 write_size = 0;
	Uint8 code;
	Uint8 *buf = *outbuf;

	Uint16 current_index = index;
	Uint16 current_size = encode_bit;

	while( (pre_size+current_size) > 7 )
	{
		if( pre_size > 7 )
		{
			code = (pre_index >> (pre_size - 8)) & 0xFFFF ;
			buf[write_size++] = code;

			pre_size -= 8;
		}
		else
		{
			code = ((pre_index << (8 - pre_size)) & 0xFFFF) + ((current_index >> (current_size - (8 - pre_size))) & 0xFFFF) ;
			buf[write_size++] = code;

			current_size -= 8-pre_size;
			pre_size = 0;
		}
	}

	*outbuf += write_size;

	pre_index = current_index;
	pre_size = current_size;

	return write_size;
}

int Lzw::WriteCodeFlush(Uint8 **outbuf)
{
	Uint32 write_size = 0;

	if( pre_size > 0 )
	{
		Uint8 code;
		Uint8 *buf = *outbuf;

		code = ((pre_index << (8 - pre_size)) & 0xFFFF) ;
		buf[write_size++] = code;

		*outbuf += write_size;
	}

	return write_size;
}

int Lzw::CheckEncodeListFull()
{
	if( code_size == MAX_DIC_SIZE )
	{
		return 1;
	}

	if( code_size  == (1 << encode_bit) )
	{
		++encode_bit;
	}

	return 0;
}

int Lzw::Encode(Uint8 *inbuf, Uint8 *outbuf, Uint32 inbuf_size)
{
	init();

	Uint8 *pixel = inbuf;
	Uint8 *end_pixel = inbuf+inbuf_size;
	Uint8 *encode_buf = outbuf;
	
	int code;
	int pre_code = NULL_CODE;

	Uint32 encode_size = 0;

	encode_size += WriteCode(&encode_buf, CLEAR_CODE);

	while(pixel < end_pixel)
	{
		code = FindStr(pre_code, *pixel);

		if( code != NULL_CODE )
		{
			pre_code = code;
		}
		else
		{
			AddCode(pre_code, *pixel);

			encode_size += WriteCode(&encode_buf, pre_code);

			if( CheckEncodeListFull() )
			{
				encode_size += WriteCode(&encode_buf, CLEAR_CODE);

				table_reset();
				pre_code = NULL_CODE;

				continue;
			}

			pre_code = *pixel;
		}

		++pixel;
	}

	encode_size += WriteCode(&encode_buf, pre_code);
	encode_size += WriteCode(&encode_buf, END_CODE);
	encode_size += WriteCodeFlush(&encode_buf);

	deinit();

	return encode_size;
}

int Lzw::ReadCode(Uint8 **inbuf)
{
	int index;
	Uint8 current_index;
	Uint32 current_size;

	while(1)
	{
		current_index = *(*inbuf)++;
		current_size = 8;

		if( ( pre_size + current_size ) >= encode_bit )
		{
			if( pre_size < 8 )
			{
				pre_index = ((pre_index << (8-pre_size)) & 0xFF) >> (8-pre_size);
			}

			index = (pre_index << (encode_bit - pre_size))  + (current_index >> (current_size - (encode_bit - pre_size))) ;

			pre_size = current_size - (encode_bit - pre_size);
			pre_index = current_index;

			break;
		}
		else
		{
			pre_index = ((pre_index << (8-pre_size)) & 0xFF) >> (8-pre_size);

			pre_size += current_size;
			pre_index = ( pre_index << current_size ) + current_index;
			continue;
		}
	}

	return index;
}

int Lzw::WriteValue(Uint8 **outbuf, int code)
{
	int write_size = code_list[code].str_size;
	int write_pos = write_size-1;
	int temp_code = code;

	do
	{
		(*outbuf)[write_pos--] = code_list[temp_code].str;
		temp_code = code_list[temp_code].pre;
	}while(temp_code != NULL_CODE);

	*outbuf += write_size;

	return write_size;
}

int Lzw::GetFirstCode(int code)
{
	int first = code;
	while( code_list[first].pre != NULL_CODE )
	{
		first = code_list[first].pre;
	}
	return first;
}

int Lzw::CheckDecodeListFull()
{
	if( (code_size+1) == (1 << encode_bit) )
	{
		++encode_bit;
	}

	return 0;
}

int Lzw::Decode(Uint8 *inbuf, Uint8 *outbuf, Uint32 outbuf_size)
{
	Uint8 *tempInBuf = inbuf;
	int code, pre_code = NULL_CODE;

	init();

	while(1)
	{
		code = ReadCode(&tempInBuf);

		if( code == 256 )
		{
			table_reset();

			code = ReadCode(&tempInBuf);
			WriteValue(&outbuf, code);
			pre_code = code;
		}
		else if( code == 257 )
		{
			break;
		}
		else
		{
			if( code < code_size )
			{
				AddCode(pre_code, GetFirstCode(code));
				WriteValue(&outbuf, code);
				pre_code = code;
			}
			else
			{
				int new_code = AddCode(pre_code, GetFirstCode(pre_code));
				WriteValue(&outbuf, new_code);
				pre_code = code;
			}
		}

		CheckDecodeListFull();
	}

	deinit();

	return 0;
}