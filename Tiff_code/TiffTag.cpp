#include "stdafx.h"
#include "tifftag.h"
#include "string.h"

TiffTag::TiffTag()
{
	m_tagsig = NullTag ;
	m_type = Unknow ;
	m_count = 0 ;
	m_value = 0 ;
	m_lpbuf = NULL ;
}
TiffTag::~TiffTag()
{
	if( m_lpbuf != NULL )
	{
		delete [] m_lpbuf ;
		m_lpbuf = NULL ;
	}
}

TiffTag::TiffTag(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file):
m_tagsig(Tagsig) , m_type((DataType)Type) , m_count(Count) , m_value(Value) , m_lpbuf(NULL)
{
	if( file != NULL )
	{
		int byteCount = DataTypeBytes[m_type]*m_count ;
		if( byteCount > 4 )
		{
			fseek(file,m_value,SEEK_SET) ;

			LPBYTE lptmpbuf = new BYTE[byteCount] ;
			fread(lptmpbuf,DataTypeBytes[m_type],m_count,file) ;
			m_lpbuf = lptmpbuf ;
		}
	}
	else
	{
		int byteCount = DataTypeBytes[m_type]*m_count ;
		if( byteCount > 4 )
		{
			LPBYTE lptmpbuf = new BYTE[byteCount] ;
			memset(lptmpbuf, 0, byteCount);
			m_lpbuf = lptmpbuf ;
		}
	}
}

DWORD TiffTag::GetTagValue()
{
	if( is_offset() )
		return *(LPDWORD)m_lpbuf ;
	else
		return m_value ;
}

bool TiffTag::is_offset()
{
	if( DataTypeBytes[m_type]*m_count > 4 )
		return true ;
	else 
		return false ;
}

Bitspersample::Bitspersample(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file) :
TiffTag(Tagsig,Type,Count,Value,file) 
{
	if( file == NULL )
	{
		if( is_offset() )
		{
			LPWORD lptmpbuf = new WORD[m_count] ;
			for( int i = 0 ; i < m_count ; ++i )
				lptmpbuf[i] = (WORD)Value ;

			m_lpbuf = (LPBYTE)lptmpbuf ;
		}
	}
}

DWORD Bitspersample::GetTagValue()
{
	if( m_count > 1 )
		return *m_lpbuf ;
	else 
		return m_value ;
}

Resolution::Resolution(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file) :
TiffTag(Tagsig,Type,Count,Value,file) 
{
	if( file == NULL )
	{
		LPDWORD lptmpbuf = new DWORD[2] ;
		lptmpbuf[0] = Value*1 ;
		lptmpbuf[1] = 1 ;
		m_lpbuf = (LPBYTE)lptmpbuf ;
	}
}
DWORD Resolution::GetTagValue()
{
	return (DWORD)(*(LPDWORD)m_lpbuf/ *(LPDWORD)(m_lpbuf+4) ) ;
}

Exif::Exif(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file) :
TiffTag(Tagsig,Type,Count,Value,NULL) 
{
	if( file != NULL )
	{
		fseek(file,m_value,SEEK_SET) ;
		WORD count ;
		fread(&count,sizeof(WORD),1,file) ;
		int byteCount = count*12 + sizeof(WORD) + sizeof(DWORD) ;

		LPBYTE lptmpbuf = new BYTE[byteCount] ;
		*((LPWORD)lptmpbuf) = count ;
		m_count = byteCount ;
		m_type = Byte ;

		fread(lptmpbuf+2,count*12 + sizeof(DWORD),1,file) ;
		m_lpbuf = lptmpbuf ;
	}
	else
	{
	}
	
}

#define MAX_STRING 20 
ASCIIString::ASCIIString(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file) :
TiffTag(Tagsig,Type,Count > MAX_STRING ? Count : MAX_STRING,Value,file) 
{
	if( file == NULL )
	{
		m_lpbuf = new BYTE[m_count];
		memset(m_lpbuf,0,m_count) ;
		memcpy(m_lpbuf,(LPBYTE)m_value,Count) ;
	}
}

void TiffTag::SetValue( DWORD value )
{
}

void Bitspersample::SetValue( DWORD value )
{
	if( is_offset() )
	{
		if( m_lpbuf != NULL )
			delete [] m_lpbuf ;

		LPWORD lptmpbuf = new WORD[m_count] ;
		for( int i = 0 ; i < m_count ; ++i )
			lptmpbuf[i] = (WORD)value ;

		m_lpbuf = (LPBYTE)lptmpbuf ;
	}
}

void Resolution::SetValue( DWORD value )
{
	if( m_lpbuf != NULL )
			delete [] m_lpbuf ;

	LPDWORD lptmpbuf = new DWORD[2] ;
	lptmpbuf[0] = value*1 ;
	lptmpbuf[1] = 1 ;
	m_lpbuf = (LPBYTE)lptmpbuf ;
}

void TiffTag::SaveTag( FILE* file)
{
	DWORD tmp_entry[3] ;
	tmp_entry[0] = (DWORD)m_tagsig + ( (DWORD)m_type << 16 ) ;
	tmp_entry[1] = m_count ;
	tmp_entry[2] = m_value ;

	fwrite(tmp_entry,sizeof(DWORD),3,file) ;

	if( is_offset() )
	{
		int databyte = DataTypeBytes[m_type]*m_count ;
		fseek(file,m_value,SEEK_SET);
		LPBYTE tmplpbuf = m_lpbuf ;
		fwrite(tmplpbuf,sizeof(BYTE),databyte,file) ;
	}
}

void Exif::SaveTag( FILE* file)
{
	DWORD tmp_entry[3] ;
	tmp_entry[0] = (DWORD)m_tagsig + ( (DWORD)(Long) << 16 ) ;
	tmp_entry[1] = 1 ;
	tmp_entry[2] = m_value ;

	fwrite(tmp_entry,sizeof(DWORD),3,file) ;

	if( is_offset() )
	{
		int databyte = DataTypeBytes[m_type]*m_count ;
		fseek(file,m_value,SEEK_SET);
		LPBYTE tmplpbuf = m_lpbuf ;
		fwrite(tmplpbuf,sizeof(BYTE),databyte,file) ;
	}
}

Strip::Strip(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file) :
TiffTag(Tagsig,Type,Count,Value,file) 
{
	if( file == NULL )
	{
		if( is_offset() )
		{
			LPDWORD lptmpbuf = new DWORD[m_count] ;
			for( int i = 0 ; i < m_count ; ++i )
				lptmpbuf[i] = (DWORD)m_value ;

			m_lpbuf = (LPBYTE)lptmpbuf ;
		}
	}
}