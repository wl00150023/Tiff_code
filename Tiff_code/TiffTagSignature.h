#pragma once

#ifndef TIFFTAGSIGNATURE_
#define TIFFTAGSIGNATURE_

enum TiffTagSignature{
	NullTag						= 0x0000L,
	NewSubfileType				= 0x00FEL,
	SubfileType					= 0x00FFL,
	ImageWidth					= 0x0100L,
	ImageLength					= 0x0101L,
	BitsPerSample				= 0x0102L,
	Compression					= 0x0103L,
	PhotometricInterpretation	= 0x0106L,
	Threshholding				= 0x0107L,
	CellWidth					= 0x0108L,
	CellLength					= 0x0109L,
	FillOrder					= 0x010AL,
	DocumentName				= 0x010DL,
	ImageDescription			= 0x010EL,
	Make						= 0x010FL,
	Model						= 0x0110L,
	StripOffsets				= 0x0111L,
	Orientation					= 0x0112L,
	SamplesPerPixel				= 0x0115L,
	RowsPerStrip				= 0x0116L,
	StripByteCounts				= 0x0117L,
	MinSampleValue				= 0x0118L,
	MaxSampleValue				= 0x0119L,
	XResolution					= 0x011AL,
	YResolution					= 0x011BL,
	PlanarConfiguration			= 0x011CL,
	PageName					= 0x011DL,
	XPosition					= 0x011EL,
	YPosition					= 0x011FL,
	FreeOffsets					= 0x0120L,
	FreeByteCounts				= 0x0121L,
	GrayResponseUnit			= 0x0122L,
	GrayResponsCurve			= 0x0123L,
	T4Options					= 0x0124L,
	T6Options					= 0x0125L,
	ResolutionUnit				= 0x0128L,
	PageNumber					= 0x0129L,
	TransferFunction			= 0x012DL,
	Software					= 0x0131L,
	DateTime					= 0x0132L,
	Artist						= 0x013BL,
	HostComputer				= 0x013CL,
	Predicator					= 0x013DL,
	WhitePoint					= 0x013EL,
	PrimaryChromaticities		= 0x013FL,
	ColorMap					= 0x0140L,
	HalftoneHints				= 0x0141L,
	TileWidth					= 0x0142L,
	TileLength					= 0x0143L,
	TileOffsets					= 0x0144L,
	TileByteCounts				= 0x0145L,
	InkSet						= 0x014CL,
	InkNames					= 0x014DL,
	NumberOfInks				= 0x014EL,
	DotRange					= 0x0150L,
	TargetPrinter				= 0x0151L,
	ExtraSamples				= 0x0152L,
	SampleFormat				= 0x0153L,
	SMinSampleValue				= 0x0154L,
	SMaxSampleValue				= 0x0155L,
	TransforRange				= 0x0156L,
	JPEGProc					= 0x0157L,
	JPEGInterchangeFormat		= 0x0201L,
	JPEGIngerchangeFormatLength = 0x0202L,
	JPEGRestartInterval			= 0x0203L,
	JPEGLosslessPredicators		= 0x0205L,
	JPEGPointTransforms			= 0x0206L,
	JPEGQTable					= 0x0207L,
	JPEGDCTable					= 0x0208L,
	JPEGACTable					= 0x0209L,
	YCbCrCoefficients			= 0x0211L,
	YCbCrSampling				= 0x0212L,
	YCbCrPositioning			= 0x0213L,
	ReferenceBlackWhite			= 0x0214L,
	XML_Data					= 0x02BCL,
	CopyRight					= 0x8298L,
	IPTC						= 0x83BBL,/*!< IPTC (International Press Telecommunications Council) metadata.*/
	Photoshop					= 0x8649L,
	Exif_IFD					= 0x8769L,
	IccProfile					= 0x8773L
};

enum DataType{
	Unknow     = 0x0000L,
	Byte       = 0x0001L,
	Asciiz     = 0x0002L,
	Short      = 0x0003L,
	Long       = 0x0004L,
	Rational   = 0x0005L,
	Sbyte      = 0x0006L,
	Undefined  = 0x0007L,
	Sshort     = 0x0008L,
	Slong      = 0x0009L,
	Srational  = 0x0010L,
	Float      = 0x0011L,
	Double     = 0x0012L,

} ;

const int DataTypeBytes[]=
{
	0,
	1,
	1,
	2,
	4,
	8,
	1,
	1,
	2,
	4,
	8,
	4,
	8
} ;

enum ExifTagSignature{
	JpegIFOffset				= 0x0201,
	JpegIFByteCount				= 0x0202,
	ExifVersion					= 0x9000,
	ComponentsConfiguration		= 0x9101,
	FlashPixVersion				= 0xa000,
	ColorSpace					= 0xa001,
	ExifImageWidth				= 0xa002,
	ExifImageHeight				= 0xa003,
	ExifInteroperabilityOffset	= 0xa005,
	PreviewColorSpace			= 0xc71a
} ;

enum InteroperabilityTagSignature{
	InteroperabilityIndex				= 0x0001,
	InteroperabilityVersion				= 0x0002
} ;

#endif //TIFFTAGSIGNATURE_