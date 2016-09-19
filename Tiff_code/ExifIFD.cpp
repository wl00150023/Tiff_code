#include "exififd.h"
#include<fstream>

ExifIFD::ExifIFD()
{
	m_TagPos = NULL ;
	m_thumbnail = NULL ;
}

ExifIFD::~ExifIFD()
{
	if( m_TagPos != NULL )
		delete m_TagPos ;

	if( m_thumbnail != NULL )
		delete m_thumbnail ;
}

void ExifIFD::CreateNew(int width, int length, int resolution, int sampleperpixel)
{
	m_ifd.addTag(XResolution,Rational,1,resolution);
	m_ifd.addTag(YResolution,Rational,1,resolution);
	m_ifd.addTag(ResolutionUnit,Short,1,2);
	m_ifd.addTag(YCbCrPositioning,Short,1,2);
	m_ifd.addTag(Orientation,Short,1,0);

	m_subifd.addTag((TiffTagSignature)ExifVersion,Undefined,4,0x30323230);
	m_subifd.addTag((TiffTagSignature)ComponentsConfiguration,Undefined,4,0x00030201);
	m_subifd.addTag((TiffTagSignature)FlashPixVersion,Undefined,4,0x30303130);
	if( sampleperpixel == 3 )
		m_subifd.addTag((TiffTagSignature)ColorSpace,Short,1,1);
	else
	{
		m_subifd.addTag((TiffTagSignature)ColorSpace,Short,1,65535);
	}

	m_subifd.addTag((TiffTagSignature)ExifImageWidth,Long,1,width);
	m_subifd.addTag((TiffTagSignature)ExifImageHeight,Long,1,length);
	m_subifd.addTag((TiffTagSignature)ExifInteroperabilityOffset,Long,1,0);

	m_inteifd.addTag((TiffTagSignature)InteroperabilityIndex,Asciiz,4,0x00383952);
	m_inteifd.addTag((TiffTagSignature)InteroperabilityVersion,Undefined,4,0x30303130);
}

void ExifIFD::addMake(char *str)
{
	m_ifd.addTag(Make,Asciiz,26,(DWORD)str);
}

void ExifIFD::addModel(char *str)
{
	m_ifd.addTag(Model,Asciiz,26,(DWORD)str);
}

void ExifIFD::addSoftware(char *str)
{
	m_ifd.addTag(Software,Asciiz,28,(DWORD)str);
}

void ExifIFD::SaveIFD(IFD *ifd , DWORD pos , FILE *file)
{
	TagItr itr ;
	fseek(file,pos,SEEK_SET) ;

	WORD number_tag = ifd->m_TagList.size() ;
	int index = 1 ;
	fwrite(&number_tag,BytesOfNumTag,1,file);
	for( itr = ifd->m_TagList.begin() ; itr != ifd->m_TagList.end() ; ++itr )
	{	
		(*itr)->SaveTag(file) ;
		fseek(file,pos+BytesOfNumTag+index*BytesPerTag,SEEK_SET) ;
		++index ;
	}
	fwrite(&ifd->m_Next_ifd,BytesOfNextIFD,1,file) ;
}

void ExifIFD::Save(FILE *file)
{
	m_ifd.addTag(Exif_IFD,Long,1,0);

	ResetIFD();

	m_ifd.SaveHeader(file);
	m_ifd.SaveTag(file);

	TagItr itr ;
	TiffTag* tag = m_ifd.GetTag(Exif_IFD) ;

	if( tag != NULL )
	{
		SaveIFD(&m_subifd,tag->m_value,file) ;

		tag = m_subifd.GetTag((TiffTagSignature)ExifInteroperabilityOffset) ;
		if( tag != NULL )
		{
			SaveIFD(&m_inteifd,tag->m_value,file) ;
		}
	}

	if( m_thumbnailifd.m_TagList.size() > 0 )
	{
		SaveIFD(&m_thumbnailifd,m_ifd.m_Next_ifd,file) ;

		tag = m_thumbnailifd.GetTag((TiffTagSignature)JpegIFOffset) ;
		fseek(file,tag->m_value,SEEK_SET);

		if( m_thumbnail != NULL )
		{
			tag = m_thumbnailifd.GetTag((TiffTagSignature)JpegIFByteCount) ;
			fwrite(m_thumbnail,tag->m_value,1,file) ;
		}
	}
}

void ExifIFD::SetIFDInfo(IFD *ifd , DWORD *pos , DWORD *index )
{
	TagItr itr ;
	DWORD value_pos = (*pos) + BytesOfNumTag + ValuePosPerTag ; 
	(*pos) += BytesOfNumTag + ifd->m_TagList.size()*BytesPerTag + BytesOfNextIFD ;
	for( itr = ifd->m_TagList.begin() ; itr != ifd->m_TagList.end() ; ++itr )
	{
		m_TagPos[*index].m_tag = (*itr)->m_tagsig ;
		m_TagPos[*index].m_pos = value_pos ;
		value_pos += BytesPerTag ;

		if((*itr)->is_offset())
		{
			(*itr)->m_value = (*pos) ;
			m_TagPos[*index].m_pos = (*pos) ;
			(*pos) += DataTypeBytes[(*itr)->m_type] * (*itr)->m_count ;
			/*if(offset%2 == 1)
				++(*pos) ;*/
		}

		++(*index) ;
	}
}

void ExifIFD::ResetIFD()
{
	DWORD total_tag = m_ifd.m_TagList.size() + m_subifd.m_TagList.size()
						+ m_inteifd.m_TagList.size() + m_thumbnailifd.m_TagList.size() ;
	m_TagPos = new TagPos[total_tag] ;

	DWORD TagPos_index = 0 ;
	DWORD offset = HeaderSize ;
	TagItr itr ;

	SetIFDInfo(&m_ifd,&offset,&TagPos_index) ;

	TiffTag* tag = m_ifd.GetTag(Exif_IFD) ;
	if( tag != NULL )
	{
		tag->m_value = offset ;

		SetIFDInfo(&m_subifd,&offset,&TagPos_index) ;
		
		tag = m_subifd.GetTag((TiffTagSignature)ExifInteroperabilityOffset) ;
		if ( tag != NULL )
		{
			tag->m_value = offset ;

			SetIFDInfo(&m_inteifd,&offset,&TagPos_index) ;
		}
	}

	if( m_thumbnailifd.m_TagList.size() > 0 )
	{
		m_ifd.m_Next_ifd = offset ;

		SetIFDInfo(&m_thumbnailifd,&offset,&TagPos_index) ;

		tag = m_thumbnailifd.GetTag((TiffTagSignature)JpegIFOffset) ;
		tag->m_value = offset ;
	}
}

static const char* TagStr(TiffTagSignature tag)
{
#define DEBUG_STR(s)	case(s):	return #s
	
	switch(tag)
	{
		DEBUG_STR(ImageWidth);
		DEBUG_STR(ImageLength);
		DEBUG_STR(BitsPerSample);
		DEBUG_STR(Make);
		DEBUG_STR(Model);
		DEBUG_STR(Software);
		DEBUG_STR(XResolution);
		DEBUG_STR(YResolution);
		DEBUG_STR(ResolutionUnit);
		DEBUG_STR(YCbCrPositioning);
		DEBUG_STR(Orientation);
		DEBUG_STR(ExifVersion);
		DEBUG_STR(ComponentsConfiguration);
		DEBUG_STR(FlashPixVersion);
		DEBUG_STR(ColorSpace);
		DEBUG_STR(ExifImageWidth);
		DEBUG_STR(ExifImageHeight);
		DEBUG_STR(ExifInteroperabilityOffset);
		DEBUG_STR(InteroperabilityIndex);
		DEBUG_STR(InteroperabilityVersion);
		DEBUG_STR(Exif_IFD);
		DEBUG_STR(Compression);
		DEBUG_STR(JpegIFOffset);
		DEBUG_STR(JpegIFByteCount);
		default:
			return "unknown Tag";
	}
#undef DEBUG_STR

}

void ExifIFD::SavePos(string outfile)
{
	ofstream out ;
	out.open(outfile.c_str());

	DWORD total_tag = m_ifd.m_TagList.size() + m_subifd.m_TagList.size()
						+ m_inteifd.m_TagList.size() + m_thumbnailifd.m_TagList.size() ;
	for( int i = 0 ; i < total_tag ; ++i )
	{

		out << TagStr(m_TagPos[i].m_tag) << "		" << dec << m_TagPos[i].m_pos  
			<< "		,0x" << hex << m_TagPos[i].m_pos << endl ;
	}

	out.close() ;
}

void ExifIFD::AddJPEGThumbnail(string infile)
{
	FILE *file = fopen(infile.c_str(),"rb");
	if( file == NULL )
		return ;

	BYTE tmp ;
	DWORD count = 0 ;
	while( fread(&tmp,1,1,file) )
	{
		++count ;
	}
	
	m_thumbnail = new BYTE[count] ;

	fseek(file,0,SEEK_SET);
	fread(m_thumbnail,count,1,file) ;

	m_thumbnailifd.addTag(Compression,Short,1,6);
	m_thumbnailifd.addTag((TiffTagSignature)JpegIFOffset,Long,1,0);
	m_thumbnailifd.addTag((TiffTagSignature)JpegIFByteCount,Long,1,count);
}

void ExifIFD::AddNullJPEGThumbnail()
{
	m_thumbnailifd.addTag(Compression,Short,1,6);
	m_thumbnailifd.addTag((TiffTagSignature)JpegIFOffset,Long,1,0);
	m_thumbnailifd.addTag((TiffTagSignature)JpegIFByteCount,Long,1,0);
}