#pragma once
#include <string>
#include <stdint.h>

#define BUFFER_SIZE 16384

enum class PackType
{
	raw = 0,
	zlib = 1
};

struct FileData
{
	UINT64 start; //start pos without header
	UINT64 end; //end pos without header
	UINT64 originSize;
	FileData():start(0), end(0),originSize(0)
	{

	}
	FileData( UINT64 _start, UINT64 _end, UINT64 _originSize)
	{
		start = _start;
		end = _end;
		originSize = _originSize;
	}
};

struct FileDataWithPath:public FileData
{
	std::wstring subPath; //relative path
	FileDataWithPath(std::wstring _subPath, UINT64 _start, UINT64 _end, UINT64 _originSize):FileData(_start, _end, _originSize)
	{
		subPath = _subPath;
	}
};