#pragma once 

#include<iostream>
#include<vector>
#include"TiffTag.h"

using namespace std ;

typedef vector<TiffTag*>::iterator TagItr ;

#ifndef _ERROR
#define _ERROR

enum ERROR{
	TIFF_OK  = 0 ,
	TIFF_False = -1 
} ;

#endif

#define BytesPerTag		12
#define HeaderSize		8
#define ValuePosPerTag	8
#define BytesOfNumTag	sizeof(WORD)
#define BytesOfNextIFD	sizeof(DWORD)
#define TiffHeader		0x002a4949

class IFD
{
public :
	vector<TiffTag*> m_TagList ;
	DWORD m_Next_ifd ;

	IFD();
	~IFD() ;
	void addTag(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file = NULL) ;
	void addTag(const DWORD& TagType , const DWORD& Count , const DWORD& Value , FILE* file = NULL) ;
	
	TiffTag* GetTag(TiffTagSignature Tagsig) ;
	DWORD GetTagValue(TiffTagSignature Tagsig);
	void SetTagValue(TiffTagSignature Tagsig , DWORD value) ;
	void clear_TagList() ;

	void ReadTag(FILE* file) ;
	void ReadImage(FILE* file) ;
	void ReadSingleStrip(FILE* file) ;
	void ReadMultiStrip(FILE* file) ;
	void ReadLzwEncode(FILE* file) ;
	ERROR ReadHeader(FILE* file) ;
	void SaveTag(FILE* file) ;
	void SaveImage(LPBYTE buf , FILE* file) ;
	void SaveHeader(FILE* file) ;
	void ResetOffset() ;

	void CheckExif(FILE* file);

	void deleteTag( const TiffTagSignature& Tagsig ) ;

	int LzwDecode(Uint8 *inbuf, Uint8 *outbuf, Uint32 outbuf_size);
	int LzwEncode(Uint8 *inbuf, Uint8 *outbuf, Uint32 inbuf_size);
	void PredicatorDecode(Uint8 *inbuf, Uint32 width, Uint32 length, Uint32 channel);
	void PredicatorEncode(Uint8 *inbuf, Uint32 width, Uint32 length, Uint32 channel);

	ERROR CheckTiffIsCorret();
};