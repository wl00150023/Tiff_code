#include "stdafx.h"
#include "ifd.h"

IFD::IFD()
{
	m_Next_ifd = 0 ;
}
IFD::~IFD()
{
	clear_TagList() ;
}
void IFD::addTag(const TiffTagSignature& Tagsig , const DWORD& Type , const DWORD& Count , const DWORD& Value , FILE* file) 
{
	TiffTag* Newtag ;
	switch( Tagsig )
	{
	case BitsPerSample :
		Newtag = new Bitspersample(Tagsig,Type,Count,Value,file) ;
		break ;
	case StripByteCounts :
	case StripOffsets :
		Newtag = new Strip(Tagsig,Type,Count,Value,file) ;
		break ;
	case XResolution :
		Newtag = new Resolution(Tagsig,Type,Count,Value,file) ;
		break ;
	case YResolution :
		Newtag = new Resolution(Tagsig,Type,Count,Value,file) ;
		break ;
	case Exif_IFD :
		Newtag = new Exif(Tagsig,Type,Count,Value,file) ;
		break ;
	case Make:
	case Model:
	case Software:
		Newtag = new ASCIIString(Tagsig,Type,Count,Value,file) ;
		break ;
	default :
		Newtag = new TiffTag(Tagsig,Type,Count,Value,file) ;
		break ;
	}

	m_TagList.push_back(Newtag) ;
}

ERROR IFD::ReadHeader(FILE* file) 
{
	DWORD tiffCode ;
	fread(&tiffCode,sizeof(DWORD),1,file);
	if( tiffCode != TiffHeader )
		return TIFF_False ;

	return TIFF_OK ;
}

void IFD::ReadTag(FILE* file)
{
	DWORD ifd_offset ;
	fread(&ifd_offset,sizeof(DWORD),1,file);
	fseek(file,ifd_offset,0) ;

	WORD CountTag ;
	fread(&CountTag,sizeof(WORD),1,file) ;
	m_TagList.reserve(CountTag) ;

	DWORD tmp_entry[3] ;
	for( int i = 0 ; i < CountTag ; ++i )
	{
		fread(tmp_entry,sizeof(DWORD),3,file) ;

		addTag(tmp_entry[0],tmp_entry[1],tmp_entry[2],file);

		fseek(file , ifd_offset+sizeof(CountTag)+(i+1)*BytesPerTag , SEEK_SET) ;
	}

	fseek(file , ifd_offset+sizeof(CountTag)+CountTag*BytesPerTag , SEEK_SET) ;
	fread(&m_Next_ifd,sizeof(DWORD),1,file) ;
}

void IFD::ResetOffset() 
{
	DWORD offset = HeaderSize + BytesOfNumTag + m_TagList.size()*BytesPerTag + BytesOfNextIFD ;
	TagItr itr ;
	for( itr = m_TagList.begin() ; itr != m_TagList.end() ; ++itr )
	{
		if((*itr)->is_offset())
		{
			(*itr)->m_value = offset ;
			offset += DataTypeBytes[(*itr)->m_type] * (*itr)->m_count ;
			/*if(offset%2 == 1)
				++offset ;*/
		}
	}

	TiffTag* tag = GetTag(StripOffsets) ;
	if( tag->m_count == 1 )
	{
		tag->m_value = offset ;
	}
	else
	{
		TiffTag* tag2 = GetTag(StripByteCounts);

		LPDWORD stripoffset = (LPDWORD)(tag->m_lpbuf);
		LPDWORD stripbytecount = (LPDWORD)(tag2->m_lpbuf);
		for( int i = 0 ; i < tag->m_count ; ++i )
		{
			stripoffset[i] = offset ;
			offset += stripbytecount[i];
		}
	}
	
}

void IFD::SaveHeader(FILE* file) 
{
	BYTE header[HeaderSize] = { 0x49 , 0x49 , 0x2a , 0x00 , 0x08 , 0x00 , 0x00 , 0x00 } ;
	fwrite(header,sizeof(header),1,file);
}

void IFD::ReadImage(FILE* file)
{
	TiffTag* stripoffset = GetTag(StripOffsets) ;
	int number_strip = stripoffset->m_count ;

	int compress_type = GetTagValue(Compression);
	if( compress_type == 1 )
	{
		if( number_strip == 1 )
			ReadSingleStrip(file) ;
		else
			ReadMultiStrip(file) ;
	}
	else if( compress_type == 5 )
	{
		ReadLzwEncode(file);
	}
	
}

void IFD::ReadSingleStrip(FILE* file)
{
	TiffTag* stripoffset = GetTag(StripOffsets) ;
	Uint32 width = GetTagValue(ImageWidth);
	Uint32 length = GetTagValue(ImageLength);
	Uint32 channel = GetTagValue(SamplesPerPixel);
	Uint32 bits = GetTagValue(BitsPerSample);

	Uint32 imagebytes = width*bits/8*length*channel;

	fseek(file,stripoffset->m_value,SEEK_SET) ;
	LPBYTE ImageBuf = new BYTE[imagebytes] ;

	fread(ImageBuf,sizeof(BYTE),imagebytes,file) ;

	if( stripoffset->m_lpbuf != NULL )
		delete [] stripoffset->m_lpbuf ;
	stripoffset->m_lpbuf = ImageBuf ;
}

void IFD::ReadMultiStrip(FILE* file)
{
	TiffTag* stripoffset = GetTag(StripOffsets) ;
	TiffTag* stripbytecount = GetTag(StripByteCounts) ;
	int number_strip = stripoffset->m_count ;

	LPDWORD Image_offset = (LPDWORD)stripoffset->m_lpbuf ;
	LPDWORD StripByte = (LPDWORD)stripbytecount->m_lpbuf ;

	DWORD ByteCount = 0 ;
	for( int n = 0 ; n < number_strip ; ++n )
		ByteCount += StripByte[n] ;

	stripbytecount->m_value = ByteCount ;
		
	LPBYTE ImageBuf = new BYTE[ByteCount] ;

	int totalPixels = GetTagValue(ImageWidth)*GetTagValue(ImageLength) ;

	if( GetTagValue(PlanarConfiguration) == 2 )
	{
		int bytes = GetTagValue(BitsPerSample)/8;
		int StripPixel = StripByte[0]/bytes;
		LPBYTE tempBuf = new BYTE[ByteCount] ;
		int Image_index = 0 ;

		for( int j = 0 ; j < number_strip ; ++j )
		{
			fseek(file,Image_offset[j],SEEK_SET);
			fread(tempBuf+Image_index,sizeof(BYTE),StripByte[j],file);
			Image_index += StripByte[j] ;
		}

		LPBYTE tempPtr = tempBuf ;
		LPBYTE ImagePtr = ImageBuf ;

		for( int j = 0 ; j < number_strip ; ++j )
		{
			ImagePtr = ImageBuf + j*bytes ;
			for( int k = 0 ; k < StripPixel ; ++k )
			{
				for( int i = 0 ; i < bytes ; ++i )
				{
					ImagePtr[i] = tempPtr[i] ;
				}

				ImagePtr += number_strip*bytes;
				tempPtr += bytes ;
			}
		}

		delete [] tempBuf ;
	}
	else
	{
		int Image_index = 0 ;
		for( int j = 0 ; j < number_strip ; ++j )
		{
			fseek(file,Image_offset[j],SEEK_SET);
			fread(ImageBuf+Image_index,sizeof(BYTE),StripByte[j],file);
			Image_index += StripByte[j] ;
		}
	}

	delete [] stripoffset->m_lpbuf ;
	delete [] stripbytecount->m_lpbuf ;

	stripoffset->m_lpbuf = ImageBuf ;
	stripbytecount->m_lpbuf = NULL ;

	stripoffset->m_count = 1 ;
	stripbytecount->m_count = 1 ;

	SetTagValue(RowsPerStrip,GetTagValue(ImageLength)) ;

	int Sampleperpixel = GetTagValue(SamplesPerPixel);
	if( Sampleperpixel == 1 )
	{
		deleteTag(PlanarConfiguration) ;
		SetTagValue(PhotometricInterpretation,1) ;
	}
	else if( Sampleperpixel == 3 )
	{
		SetTagValue(PlanarConfiguration,1) ;
		SetTagValue(PhotometricInterpretation,2) ;
	}
}

void IFD::SaveTag(FILE* file)
{
	WORD number_tag = m_TagList.size() ;
	fwrite(&number_tag,sizeof(WORD),1,file);

	int index = 0 ;
	const int ifdOfset = HeaderSize + BytesOfNumTag ;
	TagItr itr ;
	for( itr = m_TagList.begin() ; itr != m_TagList.end() ; ++itr )
	{	
		++index ;
		(*itr)->SaveTag(file) ;
		fseek(file,ifdOfset+index*BytesPerTag,SEEK_SET) ;
	}

	fwrite(&m_Next_ifd,sizeof(DWORD),1,file);
}

void IFD::SaveImage(LPBYTE buf , FILE* file)
{
	TiffTag* stripoffset = GetTag(StripOffsets);
	TiffTag* stripbytecount = GetTag(StripByteCounts);

	if( stripoffset->m_count > 1 )
	{
		LPDWORD offset = (LPDWORD)stripoffset->m_lpbuf;
		LPDWORD stripbyte = (LPDWORD)stripbytecount->m_lpbuf;
		LPBYTE tempbuf = buf;
		for( int i = 0 ; i < stripoffset->m_count ; ++i )
		{
			fseek(file,offset[i],SEEK_SET) ;
			fwrite(tempbuf,sizeof(BYTE),stripbyte[i],file) ;
			tempbuf += stripbyte[i];
		}
	}
	else
	{
		fseek(file,stripoffset->m_value,SEEK_SET) ;
		fwrite(buf,sizeof(BYTE),stripbytecount->m_value,file) ;
	}

	
}

void IFD::addTag(const DWORD& TagType , const DWORD& Count , const DWORD& Value , FILE* file) 
{
	addTag((TiffTagSignature)(TagType & 0xFFFF) , (TagType >> 16) & 0xFFFF , Count , Value , file ) ;
}

TiffTag* IFD::GetTag(TiffTagSignature Tagsig)
{
	TagItr itr ;
	for( itr = m_TagList.begin() ; itr != m_TagList.end() ; ++itr )
	{
		if( (*itr)->m_tagsig == Tagsig )
			break ;
	}
	if( itr != m_TagList.end() )
		return *itr ;
	else
	{
		//cout << "Tag does not exist. We return a NULL pointer." << endl ;
		return NULL ;
	}
}
DWORD IFD::GetTagValue(TiffTagSignature Tagsig)
{
	TiffTag* tmptag = GetTag(Tagsig) ;

	if(tmptag != NULL)
		return tmptag->GetTagValue() ;
	else
	{
		//cout << "Tag does not exist." << endl ;
		return 0xFFFFFFFF ;
	}
}
void IFD::SetTagValue(TiffTagSignature Tagsig , DWORD value) 
{
	TagItr itr ;
	for( itr = m_TagList.begin() ; itr != m_TagList.end() ; ++itr )
	{
		if( (*itr)->m_tagsig == Tagsig )
			break ;
	}
	if( itr != m_TagList.end() )
	{
		if( (*itr)->is_offset() )
		{
			(*itr)->SetValue(value) ;
		}
		else
			(*itr)->m_value = value ;
	}
	else
		addTag(Tagsig,Long,1,value) ;
}
void IFD::clear_TagList()
{
	TagItr itr ;
	for( itr = m_TagList.begin() ; itr != m_TagList.end() ; ++itr )
		delete (*itr) ;

	m_TagList.clear() ;
}

void IFD::deleteTag( const TiffTagSignature& Tagsig )
{
	vector<TiffTag*>::iterator itr ;
	for( itr = m_TagList.begin() ; itr != m_TagList.end() ; ++itr )
		if( (*itr)->m_tagsig == Tagsig )
		{
			m_TagList.erase(itr) ;
			break ;
		}
}

void IFD::ReadLzwEncode(FILE* file)
{
	TiffTag* stripoffset = GetTag(StripOffsets) ;
	TiffTag* stripbytecount = GetTag(StripByteCounts) ;

	int width = GetTagValue(ImageWidth);
	int length = GetTagValue(ImageLength);
	int Sampleperpixel = GetTagValue(SamplesPerPixel);
	int bytes = GetTagValue(BitsPerSample)/8;
	int image_size = width*length*Sampleperpixel*bytes;

	LPBYTE ImageBuf = new BYTE[image_size] ;

	if( stripoffset->m_count == 1 )
	{
		fseek(file,stripoffset->m_value,SEEK_SET) ;
		LPBYTE DecodeBuf = new BYTE[stripbytecount->m_value] ;
		fread(DecodeBuf,sizeof(BYTE),stripbytecount->m_value,file) ;

		LzwDecode(DecodeBuf, ImageBuf, image_size);

		delete [] DecodeBuf;
	}
	else
	{
		Uint32 number_strip = stripoffset->m_count;
		Uint8 *tempBuf = ImageBuf;

		LPDWORD Image_offset = new DWORD[number_strip] ;
		fseek(file,stripoffset->m_value,SEEK_SET);
		fread(Image_offset,sizeof(DWORD),number_strip,file) ;

		LPDWORD StripByte = new DWORD[number_strip] ;
		fseek(file,stripbytecount->m_value,SEEK_SET);
		fread(StripByte,sizeof(DWORD),number_strip,file) ;

		Uint32 rowsperstrip = GetTagValue(RowsPerStrip);
		Uint32 rowsbytes = rowsperstrip*width*Sampleperpixel*bytes;

		for( int i = 0 ; i < stripoffset->m_count ; ++i )
		{
			fseek(file, Image_offset[i] ,SEEK_SET) ;
			LPBYTE DecodeBuf = new BYTE[StripByte[i]] ;
			fread(DecodeBuf,sizeof(BYTE),StripByte[i],file) ;

			LzwDecode(DecodeBuf, tempBuf, rowsbytes);
			tempBuf += rowsbytes;

			delete [] DecodeBuf;
		}

		delete [] StripByte;
		delete [] Image_offset;

		stripoffset->m_count = 1;
		stripbytecount->m_count = 1;
		
		SetTagValue(RowsPerStrip, length);
	}
	
	if( stripoffset->m_lpbuf != NULL )
		delete [] stripoffset->m_lpbuf ;

	stripoffset->m_lpbuf = ImageBuf ;
	stripbytecount->m_value = image_size;

	if( GetTagValue(Predicator) == 2 )
	{
		PredicatorDecode(ImageBuf, width, length, Sampleperpixel);
		SetTagValue(Predicator, 1);
	}

	SetTagValue(Compression, 1);
}

#include "lzw.h"
int IFD::LzwDecode(Uint8 *inbuf, Uint8 *outbuf, Uint32 outbuf_size)
{
	Lzw lzw;

	return lzw.Decode(inbuf, outbuf, outbuf_size);
}

int IFD::LzwEncode(Uint8 *inbuf, Uint8 *outbuf, Uint32 inbuf_size)
{
	Lzw lzw;

	return lzw.Encode(inbuf, outbuf, inbuf_size);
}

void IFD::PredicatorDecode(Uint8 *inbuf, Uint32 width, Uint32 length, Uint32 channel)
{
	Uint8 *pixel;
	for( int i = 0 ; i < length ; ++i )
	{
		pixel = inbuf + i*width*channel ;
		for( int j = 1 ; j < width ; ++j )
		{
			for( int k = 0 ; k < channel ; ++k )
			{
				pixel[k+channel] += pixel[k];
			}

			pixel += channel;
		}
	}
}

void IFD::PredicatorEncode(Uint8 *inbuf, Uint32 width, Uint32 length, Uint32 channel)
{
	Uint8 *pixel;
	for( int i = 0 ; i < length ; ++i )
	{
		pixel = inbuf + i*width*channel + (width-2)*channel;
		for( int j = width-1 ; j > 0 ; --j )
		{
			for( int k = 0 ; k < channel ; ++k )
			{
				pixel[k+channel] -= pixel[k];
			}

			pixel -= channel;
		}
	}
}

ERROR IFD::CheckTiffIsCorret()
{
	TiffTag* stripbytecount = GetTag(StripByteCounts) ;
	int count = stripbytecount->m_count;
	int imagesize = 0;

	if( count == 1)
	{
		imagesize = stripbytecount->m_value;
	}
	else
	{
		LPDWORD stripcount = (LPDWORD)(stripbytecount->m_lpbuf);
		for( int i = 0 ; i < count ; ++i )
		{
			imagesize += *stripcount++;
		}
	}
	
	int width = GetTagValue(ImageWidth);
	int length = GetTagValue(ImageLength);
	int channel = GetTagValue(SamplesPerPixel);
	int bits = GetTagValue(BitsPerSample);
	
	if( imagesize != (width*length*channel*bits/8) )
		return TIFF_False;
	else
		return TIFF_OK;
}