#pragma once 

#include "tifftagsignature.h"
#include "windef.h"

class TiffTag
{
public :
	TiffTagSignature m_tagsig ;
	DataType m_type ;
	DWORD m_count ;
	DWORD m_value ;
	LPBYTE m_lpbuf ;

	TiffTag() ;
	TiffTag(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file = NULL) ;
	virtual ~TiffTag();

	virtual DWORD GetTagValue() ;
	bool is_offset() ;
	virtual void SetValue( DWORD value ) ;

	virtual void SaveTag( FILE* file) ;

} ;

class Bitspersample : public TiffTag
{
public :
	Bitspersample(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file = NULL) ;
	virtual DWORD GetTagValue() ;
	virtual void SetValue( DWORD value ) ;
} ;
class Resolution : public TiffTag
{
public :
	Resolution(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file = NULL) ;
	virtual DWORD GetTagValue() ;
	virtual void SetValue( DWORD value ) ;
} ;
class Exif : public TiffTag
{
public :
	Exif(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file = NULL) ;
	virtual void SaveTag( FILE* file) ;
} ;
class ASCIIString : public TiffTag
{
public :
	ASCIIString(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file = NULL) ;
} ;

class Strip : public TiffTag
{
public :
	Strip(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file = NULL) ;
} ;