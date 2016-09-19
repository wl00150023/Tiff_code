#include "stdafx.h"
#include "tiff.h"
#include "ifd.h"
#include "assert.h"

Tiff::Tiff()
{
	m_ifd = NULL ;
	m_lpImage = NULL ;
}
Tiff::~Tiff()
{
	if( m_ifd != NULL )
	{
		delete m_ifd ;
		m_ifd = NULL ;
	}

	if( m_lpImage != NULL )
	{
		delete [] m_lpImage ;
		m_lpImage = NULL ;
	}
}
/*
Tiff::Tiff(string filename)
{
	m_ifd = NULL ;
	read(filename) ;
}
*/
void Tiff::ClearIFD() 
{
	if( m_ifd == NULL )
		m_ifd = new IFD ;

	m_ifd->clear_TagList() ;
}

void Tiff::Clear()
{
	Tiff::ClearIFD();

	if( m_lpImage != NULL )
	{
		delete [] m_lpImage;
		m_lpImage = NULL;
	}
}

ERROR Tiff::read(string filename)
{
	FILE* file ;
	errno_t err = fopen_s(&file,filename.c_str(),"rb") ;

	if( err != TIFF_OK )
	{
		cout << "Read file error. " ;
		cout << "File Name : " << filename.c_str() << endl ;
		return TIFF_False ;
	}

	ERROR ReadErr = m_ifd->ReadHeader(file) ;
	if( ReadErr == TIFF_False )
	{
		cout << "It is not a tiff file. " ;
		cout << "File Name : " << filename.c_str() << endl ;
		return TIFF_False ;
	}

	Tiff::Clear() ;

	m_ifd->ReadTag(file) ;

	if( GetTagValue(Compression) != 1 )
	{
		if( GetTagValue(Compression) == 5 )
		{
			cout << "It's LZW-compressd file." << endl;
			if( GetTagValue(BitsPerSample) != 8 )
			{
				cout << "Only support 8bit Lzw compressed." << endl;
				return TIFF_False;
			}
		}
		else
		{
			cout << "Not support file." << endl;
			return TIFF_False;
		}
	}
	else
	{	// photoshop can open the problem file
		// so we do not check this file
		/*ReadErr = m_ifd->CheckTiffIsCorret();
		if( ReadErr == TIFF_False )
		{
			cout << "Tiff file is corrupt. " << endl ;
			return TIFF_False ;
		}*/
	}

	m_ifd->ReadImage(file) ;

	fclose(file) ;

	TiffTag* stripoffsets = m_ifd->GetTag(StripOffsets) ;
	m_lpImage = stripoffsets->m_lpbuf ;
	stripoffsets->m_lpbuf = NULL ;

	return TIFF_OK ;
}

ERROR Tiff::save(string filename)
{
	if( m_ifd->m_TagList.size() == 0 )
	{
		cout << "Tiff is not contructed yet." << endl ;
		return TIFF_False;
	}

	m_ifd->ResetOffset() ;

	FILE* file ;
	fopen_s(&file,filename.c_str(),"wb");

	m_ifd->SaveHeader(file) ;
	m_ifd->SaveTag(file) ;
	m_ifd->SaveImage(m_lpImage, file) ;

	fclose(file) ;

	return TIFF_OK ;
}

DWORD Tiff::GetTagValue(TiffTagSignature Tagsig)
{
	return m_ifd->GetTagValue(Tagsig) ;
}
void Tiff::SetTagValue(TiffTagSignature TagSig , DWORD value) 
{
	m_ifd->SetTagValue(TagSig,value) ;
}

void Tiff::addTag(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value ) 
{
	m_ifd->addTag(Tagsig,Type,Count,Value) ;
}

void Tiff::delTag(const TiffTagSignature& Tagsig) 
{
	m_ifd->deleteTag(Tagsig) ;
}

/****************************************CTiff*********************************************/


CTiff::CTiff()
{
	m_width = m_length = m_bitspersample = m_samplesperpixel = m_resolution = m_BytesPerLine = 0 ;
}
CTiff::~CTiff()
{}
LPBYTE CTiff::GetImageBuffer()
{
	return m_lpImage ;
}
/*
CTiff::CTiff(string filename) 
{
	ERROR err = CTiff::read(filename) ;
	if( err != TIFF_OK )
		assert(0);
}
*/
ERROR CTiff::read(string filename)
{
	errno_t err = Tiff::read(filename) ;
	if( err != TIFF_OK )
		return TIFF_False ;

	m_width = GetTagValue(ImageWidth) ;
	m_length = GetTagValue(ImageLength) ;
	m_bitspersample = GetTagValue(BitsPerSample) ;
	m_samplesperpixel = GetTagValue(SamplesPerPixel) ;
	m_resolution = GetTagValue(XResolution) ;

	m_BytesPerLine = BytesPLine(m_width , m_samplesperpixel , m_bitspersample) ;

	CreateNewIFD();

	return TIFF_OK ;
}

void CTiff::CreateNewIFD()
{
	Tiff::ClearIFD() ;

	addTag(ImageWidth,Short,1,m_width) ;
	addTag(ImageLength,Short,1,m_length) ;
	
	addTag(XResolution,Rational,1,m_resolution) ;
	addTag(YResolution,Rational,1,m_resolution) ;
	
	addTag(SamplesPerPixel,Short,1,m_samplesperpixel) ;
	addTag(BitsPerSample,Short,m_samplesperpixel,m_bitspersample) ;
	
	if( m_samplesperpixel > 1 )
	{
		if( m_samplesperpixel == 4)
			addTag(PhotometricInterpretation,Short,1,5) ;
		else 
			addTag(PhotometricInterpretation,Short,1,2) ;
	}
	else
	{
		if( m_bitspersample == 1)
			addTag(PhotometricInterpretation,Short,1,0) ;
	    else 
			addTag(PhotometricInterpretation,Short,1,1) ;
	}

	addTag(Compression,Short,1,1) ;

	addTag(StripOffsets,Long,1,0) ;
	addTag(RowsPerStrip,Long,1,m_length) ;

	int bytecount = m_BytesPerLine*m_length ;
	addTag(StripByteCounts,Long,1,bytecount) ;
	addTag(ResolutionUnit,Short,1,2) ;
	addTag(PlanarConfiguration, Short, 1, 1);

	Tiff::m_ifd->m_Next_ifd = 0 ;
}

void CTiff::Convert2PlaneChunk()
{
	if( (m_samplesperpixel > 1) && (GetTagValue(PlanarConfiguration) == 1) )
	{
		Uint8 *newbuf = new Uint8[m_BytesPerLine*m_length];
		Uint8 *tempbuf1 = newbuf;
		Uint8 *tempbuf2;
		Uint8 *oldbuf = GetImageBuffer();
		Uint32 bytes = m_bitspersample/8;
		int i, j, k;
		Uint32 plane_imagebytes = m_width*m_length*bytes;

		for( j = 0 ; j < m_length ; ++j )
		{
			tempbuf1 = newbuf + j*m_width*bytes;
			for( i = 0 ; i < m_width ; ++i )
			{
				tempbuf2 = tempbuf1 + i*bytes;
				for( k = 0 ; k < m_samplesperpixel ; ++k )
				{
					memcpy(tempbuf2, oldbuf, bytes);
					tempbuf2 += plane_imagebytes;
					oldbuf += bytes;
				}
			}
		}

		SetImageBuffer(newbuf);
		SetTagValue(PlanarConfiguration, 2);

		delTag(StripOffsets);
		delTag(StripByteCounts);

		addTag(StripOffsets, Long, m_samplesperpixel, 0) ;
		addTag(StripByteCounts, Long, m_samplesperpixel, plane_imagebytes) ;
	}
}

DWORD CTiff::BytesPLine( int width , int samplesperpixel , int bitspersample ) 
{
	int BitsPerLine = width*samplesperpixel*bitspersample ;

	if( BitsPerLine%8 != 0 )
		return (BitsPerLine >> 3)+1 ;
	else 
		return (BitsPerLine >> 3) ;
}

template<class T>
void CTiff::GetRow( T* lpBuf , int row )
{
	LPBYTE tmpImage = m_lpImage + row*m_BytesPerLine ;
	memcpy(lpBuf , tmpImage , m_BytesPerLine) ;
}
void CTiff::GetRow( LPBYTE lpBuf , int row )
{
	GetRow<BYTE>(lpBuf,row) ;
}
void CTiff::GetRow( LPWORD lpBuf , int row )
{
	GetRow<WORD>(lpBuf,row) ;
}

template<class T>
void CTiff::PutRow( T* lpBuf , int row )
{
	LPBYTE tmpImage = m_lpImage + row*m_BytesPerLine ;
	memcpy(tmpImage , lpBuf , m_BytesPerLine) ;
}
void CTiff::PutRow( LPBYTE lpBuf , int row )
{
	PutRow<BYTE>(lpBuf,row) ;
}
void CTiff::PutRow( LPWORD lpBuf , int row )
{
	PutRow<WORD>(lpBuf,row) ;
}

void CTiff::CreateNew(int Width , int Length , int Resolution , int SamplesPerpixel , int Bitspersample ) 
{
	Tiff::Clear() ;

	m_width = Width ;
	addTag(ImageWidth,Short,1,Width) ;

	m_length = Length ;
	addTag(ImageLength,Short,1,Length) ;
	
	m_resolution = Resolution ;
	addTag(XResolution,Rational,1,Resolution) ;
	addTag(YResolution,Rational,1,Resolution) ;
	
	m_samplesperpixel = SamplesPerpixel ;
	addTag(SamplesPerPixel,Short,1,SamplesPerpixel) ;
	m_bitspersample = Bitspersample ;
	addTag(BitsPerSample,Short,SamplesPerpixel,Bitspersample) ;
	
	if( SamplesPerpixel > 1 )
	{
		if( SamplesPerpixel == 4)
			addTag(PhotometricInterpretation,Short,1,5) ;
		else 
			addTag(PhotometricInterpretation,Short,1,2) ;
	}
	else
	{
		if( Bitspersample == 1)
			addTag(PhotometricInterpretation,Short,1,0) ;
	    else 
			addTag(PhotometricInterpretation,Short,1,1) ;
	}

	addTag(Compression,Short,1,1) ;

	addTag(StripOffsets,Long,1,0) ;
	addTag(RowsPerStrip,Long,1,Length) ;

	m_BytesPerLine = BytesPLine(m_width , m_samplesperpixel , m_bitspersample) ;

	int bytecount = m_BytesPerLine*Length ;
	addTag(StripByteCounts,Long,1,bytecount) ;
	addTag(ResolutionUnit,Short,1,2) ;
	addTag(PlanarConfiguration, Short, 1, 1);

	LPBYTE lptmpImage = new BYTE[bytecount] ;
	
	SetImageBuffer(lptmpImage) ;

	Tiff::m_ifd->m_Next_ifd = 0 ;
}

void CTiff::CreateNewPlaneChuck(int Width , int Length , int Resolution , int SamplesPerpixel , int Bitspersample ) 
{
	Tiff::Clear() ;

	m_width = Width ;
	addTag(ImageWidth,Short,1,Width) ;

	m_length = Length ;
	addTag(ImageLength,Short,1,Length) ;
	
	m_resolution = Resolution ;
	addTag(XResolution,Rational,1,Resolution) ;
	addTag(YResolution,Rational,1,Resolution) ;
	
	m_samplesperpixel = SamplesPerpixel ;
	addTag(SamplesPerPixel,Short,1,SamplesPerpixel) ;
	m_bitspersample = Bitspersample ;
	addTag(BitsPerSample,Short,SamplesPerpixel,Bitspersample) ;
	
	if( SamplesPerpixel > 1 )
	{
		if( SamplesPerpixel == 4)
			addTag(PhotometricInterpretation,Short,1,5) ;
		else 
			addTag(PhotometricInterpretation,Short,1,2) ;
	}
	else
	{
		if( Bitspersample == 1)
			addTag(PhotometricInterpretation,Short,1,0) ;
	    else 
			addTag(PhotometricInterpretation,Short,1,1) ;
	}

	addTag(Compression,Short,1,1) ;

	addTag(StripOffsets,Long,SamplesPerpixel,0) ;
	addTag(RowsPerStrip,Long,1,Length) ;

	m_BytesPerLine = BytesPLine(m_width , m_samplesperpixel , m_bitspersample) ;

	int bytecount = m_BytesPerLine*Length ;
	addTag(StripByteCounts,Long,SamplesPerpixel,bytecount/SamplesPerpixel) ;
	addTag(ResolutionUnit,Short,1,2) ;
	addTag(PlanarConfiguration, Short, 1, 2);

	LPBYTE lptmpImage = new BYTE[bytecount] ;
	
	SetImageBuffer(lptmpImage) ;

	Tiff::m_ifd->m_Next_ifd = 0 ;
}

LPBYTE CTiff::GetXY( int x , int y )
{
	int Bytepersample = (m_bitspersample >> 3) ;
	return (m_lpImage+m_BytesPerLine*y+x*m_samplesperpixel*Bytepersample) ;
}
void CTiff::SetImageBuffer( LPBYTE lpBuf ) 
{
	if( m_lpImage != NULL )
		delete [] m_lpImage ;
	
	m_lpImage = lpBuf ;
}

template<class T>
void CTiff::GetRowColumn( T* lpBuf , int x , int y , int RecX , int RecY )
{
	T* lpPosition = (T*)GetXY(x,y);
	LPBYTE lpCurrent = (LPBYTE)lpBuf;
	int LineBufSize = (int)(RecX * m_samplesperpixel * m_bitspersample) >> 3;
	int StartY = y;
	int EndY = y + RecY;

	for(int i = StartY; i < EndY; i++)
	{
		memcpy(lpCurrent, (LPBYTE)lpPosition, LineBufSize);
		lpCurrent += LineBufSize;
		lpPosition += m_BytesPerLine;
	}
}
void CTiff::GetRowColumn( LPBYTE lpBuf , int x , int y , int RecX , int RecY )
{
	GetRowColumn<BYTE>(lpBuf, x, y, RecX, RecY);
}
void CTiff::GetRowColumn( LPWORD lpBuf , int x , int y , int RecX , int RecY ) 
{
	GetRowColumn<WORD>(lpBuf, x, y, RecX, RecY);
}

template<class T>
void CTiff::SetRowColumn( T* lpBuf , int x , int y , int RecX , int RecY )
{
	LPBYTE lpTemp = (LPBYTE)lpBuf;
	int LineBufSize = (int)(RecX * m_samplesperpixel * m_bitspersample) >> 3;
	int StartY = y;
	int EndY = y + RecY;

	for(int indexY = StartY; indexY < EndY; ++indexY)
	{
		LPBYTE lpIndex = GetXY(x,indexY);
		memcpy(lpIndex, (LPBYTE)lpTemp, LineBufSize);
		lpTemp += LineBufSize;
	}
}
void CTiff::SetRowColumn( LPBYTE lpBuf , int x , int y , int RecX , int RecY )
{
	SetRowColumn<BYTE>(lpBuf, x, y, RecX, RecY);
}
void CTiff::SetRowColumn( LPWORD lpBuf , int x , int y , int RecX , int RecY ) 
{
	SetRowColumn<WORD>(lpBuf, x, y, RecX, RecY);
}
void CTiff::SetRowColumn( BYTE value , int x , int y , int RecX , int RecY ) 
{
	int LineBufSize = (int)(RecX * m_bitspersample) >> 3;
	int StartY = y;
	int EndY = y + RecY;
	int channel = m_samplesperpixel ;

	for(int indexY = StartY; indexY < EndY; ++indexY)
	{
		LPBYTE lpIndex = GetXY(x,indexY) + channel ;
		for( int i = 0 ; i < LineBufSize ; ++i )
		{
			*lpIndex = value ;
			lpIndex += m_samplesperpixel ;
		}
	}
}

void CTiff::SetXY( int x , int y , BYTE value ) 
{
	*(GetXY(x,y)) = value ;
}

ERROR CTiff::LzwCompress(string filename)
{
	if( m_bitspersample == 8 )
	{
		CTiff tiff;
		tiff.CreateNew(m_width, m_length, 600, m_samplesperpixel, m_bitspersample);

		Uint32 image_size = m_width*m_length*m_samplesperpixel*m_bitspersample/8;
		Uint32 encode_size = 0;
		Uint32 encode_buf_size = image_size*2;
		Uint8 *encode_buf = new Uint8[encode_buf_size];

		Tiff::m_ifd->PredicatorEncode(GetImageBuffer(), m_width, m_length, m_samplesperpixel);

		encode_size = Tiff::m_ifd->LzwEncode(GetImageBuffer(), encode_buf, image_size);

		Tiff::m_ifd->PredicatorDecode(GetImageBuffer(), m_width, m_length, m_samplesperpixel);

		tiff.SetTagValue(Compression, 5);
		tiff.SetTagValue(Predicator, 2);
		tiff.SetTagValue(StripByteCounts, encode_size);
		tiff.SetImageBuffer(encode_buf);

		return tiff.save(filename);
	}
	else
	{
		cout << "Only support 8bit Lzw compressed." << endl;
		return TIFF_False;
	}
}

#include<fstream>
static const char* TagStr(TiffTagSignature tag)
{
#define DEBUG_STR(s)	case(s):	return #s
	
	switch(tag)
	{
		DEBUG_STR(ImageWidth);
		DEBUG_STR(ImageLength);
		DEBUG_STR(BitsPerSample);
		DEBUG_STR(XResolution);
		DEBUG_STR(YResolution);
		DEBUG_STR(Orientation);
		DEBUG_STR(Compression);
		DEBUG_STR(PhotometricInterpretation);
		DEBUG_STR(StripByteCounts);
		DEBUG_STR(RowsPerStrip);
		DEBUG_STR(Make);
		DEBUG_STR(Model);
		DEBUG_STR(Software);
		default:
			return "unknown Tag";
	}
#undef DEBUG_STR
}

void CTiff::SavePos(string outfile)
{
	ofstream out ;
	out.open(outfile.c_str());

	m_ifd->ResetOffset() ;

	DWORD total_tag = m_ifd->m_TagList.size() ;
	Uint32 offset = HeaderSize + BytesOfNumTag + ValuePosPerTag;
	for( int i = 0 ; i < total_tag ; ++i )
	{
		out << TagStr(m_ifd->m_TagList[i]->m_tagsig) << "		, value pos = " << dec << offset  
			<< "		, value = " << dec << m_ifd->m_TagList[i]->m_value << endl ;

		offset += BytesPerTag;
	}

	out.close() ;
}