#pragma once
#include <iostream>
#include <fstream>  
#define ZLIB_WINAPI
#include <zlib/zlib.h>
#include <map>
#include "data.hpp"

class MResource
{
public:
	MResource(UINT64 _size)
	{
		size = _size;
		addr = (unsigned char*)malloc(_size);
	}
	void FillData(std::istream& is, int64_t start, int64_t end, PackType type);

	~MResource()
	{
		free(addr);
	}
//private:
	UINT64 size;
	unsigned char* addr;
};

class MUnpacker
{
public:
	MUnpacker(std::wstring _path, PackType _type)
	{
		sourcePath = _path;
		type = _type;
		
	}
	~MUnpacker()
	{

	}

	static int ZlibInflate(std::istream& is, unsigned char* addr, int64_t inSize);

	void Unpack()
	{
		LoadHeader();
	}


	MResource LoadResource(std::wstring filePath)
	{
		using namespace std;
		FileData fd = fileDataMap[filePath];
		ifstream ifs(sourcePath, ios::in | ios::binary);
		if (ifs.bad())
		{
			ifs.close();
			wcout << L"ifstream error for file:" << sourcePath << endl;
			return NULL;
		}
	    MResource mr(fd.originSize);

		mr.FillData(ifs, headerSize + fd.start, headerSize + fd.end, type);
		ifs.close();
	}
private:
	void LoadHeader() {
		using namespace std;
		ifstream ifs(sourcePath, ios::in | ios::binary);
		char header_buffer[4];
		if (ifs.bad())
		{
			ifs.close();
			wcout << L"ifstream error for file:" << sourcePath << endl;
			return;
		}
		ifs.read(header_buffer, 4);
		memcpy(&headerSize, header_buffer, 4);
		cout << "Header Size:" << headerSize <<endl;
		
		ifs.read(header_buffer, 4);
		memcpy(&originHeaderSize, header_buffer, 4);
		cout << "Origin Header Size:" << originHeaderSize << endl;
		size_t pos = 0;
		size_t file_header_size;
		size_t file_path_length;
		wchar_t file_path[MAX_PATH];
		UINT64 file_start_pos;
		UINT64 file_end_pos;
		UINT64 origin_size;
		if (type == PackType::raw)
		{
			while (pos < originHeaderSize)
			{
				ifs.read(header_buffer, 4);
				memcpy(&file_header_size, header_buffer, 4);
				pos += file_header_size;

				file_path_length = file_header_size - 28;
				ifs.read(header_buffer, file_path_length);
				memcpy(&file_path, header_buffer, file_path_length);

				ifs.read(header_buffer, 8);
				memcpy(&file_start_pos, header_buffer, 8);

				ifs.read(header_buffer, 8);
				memcpy(&file_end_pos, header_buffer, 8);

				ifs.read(header_buffer, 8);
				memcpy(&origin_size, header_buffer, 8);

				fileDataMap.insert(make_pair(wstring(file_path), FileData(file_start_pos, file_end_pos, origin_size)));

				//wcout << L"File Data:" << file_path << L",start:" << file_start_pos << L",end:" << file_end_pos << L",origin size:" << origin_size << endl;
			}
		}
		else if (type == PackType::zlib)
		{
			unsigned char* buf = new unsigned char[originHeaderSize];
			UINT64 offset = 0;
			ZlibInflate(ifs, buf, headerSize);

			while (pos < originHeaderSize)
			{
				memcpy(&file_header_size, buf + pos, 4);
				pos += 4;

				file_path_length = file_header_size - 28;
				memcpy(&file_path, buf+pos, file_path_length);
				pos += file_path_length;

				memcpy(&file_start_pos, buf + pos, 8);
				pos += 8;

				memcpy(&file_end_pos, buf + pos, 8);
				pos += 8;

				memcpy(&origin_size, buf + pos, 8);
				pos += 8;

				fileDataMap.insert(make_pair(wstring(file_path, file_path_length), FileData(file_start_pos, file_end_pos, origin_size)));

				//wcout << L"File Data:" << file_path << L",start:" << file_start_pos << L",end:" << file_end_pos << L",origin size:" << origin_size << endl;
			}

			delete buf;
		}
		ifs.close();
	}
	size_t originHeaderSize;
	size_t headerSize;
	std::wstring sourcePath;
	std::map<std::wstring, FileData> fileDataMap;
	PackType type;
};


/* Decompress from file source to file dest until stream ends or EOF.
inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
allocated for processing, Z_DATA_ERROR if the deflate data is
invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
the version of the library linked do not match, or Z_ERRNO if there
is an error reading or writing the files. */
int MUnpacker::ZlibInflate(std::istream& is, unsigned char* addr, int64_t inSize)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char in[BUFFER_SIZE];
	unsigned char out[BUFFER_SIZE];
	int offset = 0;
	int readSize = 0;
	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	/* decompress until deflate stream ends or end of file */
	do {
		if (readSize + BUFFER_SIZE < inSize)
		{
			is.read((char*)in, BUFFER_SIZE);
		}
		else if (readSize < inSize)
		{
			is.read((char*)in, inSize - readSize);
		}
		else
		{
			break;
		}
		strm.avail_in = is.gcount();
		if (strm.avail_in == 0)
			break;

		readSize += strm.avail_in;
		strm.next_in = in;

		/* run inflate() on input until output buffer not full */
		do {			
			strm.avail_out = BUFFER_SIZE;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			have = BUFFER_SIZE - strm.avail_out;
			memcpy(addr + offset, out, have);
			offset += have;
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

//void MUnpacker::ZlibInflate(std::istream& is, unsigned char* addr)
//{
//	unsigned char buf[BUFFER_SIZE];
//
//	unsigned int have = 0;
//	z_stream strm;
//
//	/* allocate inflate state */
//	strm.zalloc = Z_NULL;
//	strm.zfree = Z_NULL;
//	strm.opaque = Z_NULL;
//	strm.avail_in = 0;
//	strm.next_in = Z_NULL;
//	int ret = inflateInit(&strm);
//	if (ret != Z_OK)
//	{
//		(void)inflateEnd(&strm);
//		return;
//	}
//
//	do{
//		is.read((char*)buf, BUFFER_SIZE);
//		strm.avail_in = is.gcount();
//		strm.next_in = buf;
//
//		/* run inflate() on input until output buffer not full */
//		do {
//			strm.avail_out = BUFFER_SIZE;
//			strm.next_out = addr + have;
//			ret = inflate(&strm, Z_NO_FLUSH);
//			//assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
//			switch (ret) {
//			case Z_NEED_DICT:
//				ret = Z_DATA_ERROR;     /* and fall through */
//			case Z_DATA_ERROR:
//			case Z_MEM_ERROR:
//				(void)inflateEnd(&strm);
//				return;
//			}
//			have += BUFFER_SIZE - strm.avail_out;
//			//if (strm.avail_out == 0 && outSize != have)
//			//{
//			//	//ERROR
//			//	std::cout << "inflate data exceeds origin size" << std::endl;
//			//	(void)inflateEnd(&strm);
//			//	return;
//			//}
//		} while (strm.avail_out == 0);
//
//		
//	} while (ret != Z_STREAM_END);
//
//	//is.read((char*)buf, inSize - offset);
//	//strm.avail_in = inSize - offset;
//	//if (strm.avail_in == 0)
//	//	return;
//	//strm.next_in = buf;
//
//	//strm.avail_out = outSize - have;
//	//strm.next_out = addr + have;
//	//ret = inflate(&strm, Z_NO_FLUSH);
//	////assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
//	//switch (ret) {
//	//case Z_NEED_DICT:
//	//	ret = Z_DATA_ERROR;     /* and fall through */
//	//case Z_DATA_ERROR:
//	//case Z_MEM_ERROR:
//	//	(void)inflateEnd(&strm);
//	//	return;
//	//}
//	//have += size - have - strm.avail_out;
//	//if (strm.avail_out != 0)
//	//{
//	//	//ERROR
//	//	std::cout << "inflate data size not match" << std::endl;
//	//}
//
//	/* clean up */
//	(void)inflateEnd(&strm);
//}

void MResource::FillData(std::istream& is, int64_t start, int64_t end, PackType type)
{
	int64_t inSize = end - start;
	if (inSize <= 0)
	{
		std::cout << "fill data error: end value can not be smaller than start value" << std::endl;
	}
	is.seekg(start, is.beg);

	UINT64 offset = 0;

	if (type == PackType::zlib)
	{
		MUnpacker::ZlibInflate(is, addr, inSize);
	}
	else if (type == PackType::raw)
	{
		while (offset < inSize - BUFFER_SIZE)
		{
			is.read((char *)addr + offset, BUFFER_SIZE);
			offset += BUFFER_SIZE;
		}
		//offset >= inSize - BUFFER_SIZE
		is.read((char *)addr + offset, inSize - offset);//last part
	}
}