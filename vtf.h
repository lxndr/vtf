#ifndef __VTF_H__
#define __VTF_H__

#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <vector>


namespace Vtf {

/* Format */
#define VTF_FORMAT_NONE					-1
#define VTF_FORMAT_RGBA8888				0
#define VTF_FORMAT_ABGR8888				1
#define VTF_FORMAT_RGB888				2
#define VTF_FORMAT_BGR888				3
#define VTF_FORMAT_RGB565				4
#define VTF_FORMAT_I8					5
#define VTF_FORMAT_IA88					6
#define VTF_FORMAT_P8					7
#define VTF_FORMAT_A8					8
#define VTF_FORMAT_RGB888_BLUESCREEN	9
#define VTF_FORMAT_BGR888_BLUESCREEN	10
#define VTF_FORMAT_ARGB8888				11
#define VTF_FORMAT_BGRA8888				12
#define VTF_FORMAT_DXT1					13
#define VTF_FORMAT_DXT3					14
#define VTF_FORMAT_DXT5					15
#define VTF_FORMAT_BGRX8888				16
#define VTF_FORMAT_BGR565				17
#define VTF_FORMAT_BGRX5551				18
#define VTF_FORMAT_BGRA4444				19
#define VTF_FORMAT_DXT1_ONEBITALPHA		20
#define VTF_FORMAT_BGRA5551				21
#define VTF_FORMAT_UV88					22
#define VTF_FORMAT_UVWQ8888				23
#define VTF_FORMAT_RGBA16161616F		24
#define VTF_FORMAT_RGBA16161616			25
#define VTF_FORMAT_UVLX8888				26



class Resource
{
public:
	enum Type {
		TypeUnknown	= 0x00000000,
		TypeLowres	= 0x00000001,
		TypeHires	= 0x00000030,
		TypeCRC		= 0x02435243
	};
	
	inline Resource(Type type) : mType(type)
		{}
	virtual inline ~Resource()
		{};
	
	inline Type getType() const
		{return mType;}
	
private:
	Type	mType;
};



class ImageResource : public Resource
{
public:
	ImageResource(Type type) : Resource(type)
		{}
	
	inline int32_t getFormat()
		{return mFormat;}
	
	inline uint16_t getWidth()
		{return mWidth;}
	
	inline uint16_t getHeight()
		{return mHeight;}
	
protected:
	int32_t mFormat;
	uint16_t mWidth;
	uint16_t mHeight;
};


class LowresImageResource : public ImageResource
{
public:
	inline LowresImageResource() : ImageResource(TypeLowres), mImage(NULL)
		{}
	
	inline ~LowresImageResource()
		{ if (mImage) delete[] mImage; }
	
	void read(std::istream& stm, uint32_t offset, int32_t format,
			uint16_t width, uint16_t height);
	
	void setup(int32_t format, uint16_t width, uint16_t height);
	
private:
	uint8_t* mImage;
};



class HiresImageResource : public Resource
{
public:
	HiresImageResource();
	inline ~HiresImageResource()
		{clear();}
	
	void read(std::istream& stm, uint32_t offset, int32_t format,
			uint16_t width, uint16_t height, uint16_t depth,
			uint8_t mipmaps, uint16_t frames);
	
	inline uint16_t getWidth()
		{return mWidth;}
	
	inline uint16_t getHeight()
		{return mHeight;}
	
	inline uint16_t getDepth()
		{return mDepth;}
	
	inline uint32_t getFormat()
		{return mFormat;}
	
	inline uint32_t getFrameCount()
		{return mFrameCount;}
	
	inline uint8_t getMipmapCount()
		{return mMipmapCount;}
	
	uint8_t* getImage(uint8_t mipmap, uint16_t frame, uint16_t face, uint16_t slice);
	uint8_t* getImageRGBA(uint8_t mipmap, uint16_t frame, uint16_t face, uint16_t slice);
	
	void clear();
	void setup(int32_t format, uint16_t width, uint16_t height, uint8_t mipmaps, uint16_t frames,
			uint16_t faces, uint16_t slices);
	void setImage(uint8_t mipmap, uint16_t frame, uint16_t face, uint16_t slice, uint8_t* data);
	bool check();
	
private:
	int32_t mFormat;
	uint16_t mWidth;
	uint16_t mHeight;
	uint16_t mDepth;
	
	uint8_t mMipmapCount;
	uint16_t mFrameCount;
	
	typedef std::vector<uint8_t*>	SliceList;
	typedef std::vector<SliceList>	FaceList;
	typedef std::vector<FaceList>	FrameList;
	typedef std::vector<FrameList>	MipmapList;
	MipmapList mImages;
};


class CRCResource : public Resource
{
public:
	inline CRCResource() : Resource(TypeCRC), mCRC(0)
		{}
	
	inline ~CRCResource()
		{}
	
	inline uint32_t get()
		{return mCRC;}
	
	inline void set(uint32_t crc)
		{mCRC = crc;}
	
private:
	uint32_t mCRC;
};


class File
{
public:
	File();
	~File();
	
	void load(const std::string& fname);
	void load(const char* data, std::size_t length);
	void load(FILE* f);
	void load(std::istream& stm);
	
	void save(const std::string& fname, uint32_t version);
	void save(std::ostream& stm, uint32_t version);
	
	void addResource(Resource* res);
	void delResource(Resource::Type type);
	Resource* findResource(Resource::Type type);
	
private:
	typedef std::vector<Resource*> ResourceList;
	ResourceList mResourceList;
};



class Exception : public std::exception
{
public:
	Exception(const std::string& message) throw ();
	~Exception() throw ();
	
	const char* what() const throw ();

private:
	std::string mMessage;
};


const char* formatToString (uint32_t format);


}

#endif
