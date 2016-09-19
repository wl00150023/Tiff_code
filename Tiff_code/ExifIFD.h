#include"ifd.h"

using namespace std ;

struct TagPos
{
	TiffTagSignature	m_tag ;
	DWORD				m_pos ;
};

class ExifIFD
{
public :
	IFD m_ifd;
	IFD m_subifd;
	IFD m_inteifd;
	IFD m_thumbnailifd;
	LPBYTE m_thumbnail;
	TagPos *m_TagPos ;

	ExifIFD();
	~ExifIFD();

	void CreateNew(int width , int length , int resolution, int sampleperpixel) ;
	void addMake(char *str) ;
	void addModel(char *str) ;
	void addSoftware(char *str) ;
	void Save(FILE* file) ;
	void SavePos(string outfile) ;
	void ResetIFD();
	void AddJPEGThumbnail(string infile) ;
	void AddNullJPEGThumbnail() ;
	void SaveIFD(IFD *ifd , DWORD pos , FILE *file) ;
	void SetIFDInfo(IFD *ifd , DWORD *pos , DWORD *index ) ;
};