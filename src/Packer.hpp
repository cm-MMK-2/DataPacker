#pragma once
#include <fstream>  
#include <iostream>
#include <filesystem>
#include <vector>
#include <stdio.h>
#include <sstream>
#include <assert.h>
#define ZLIB_WINAPI
#include <zlib/zlib.h>
#include "data.hpp"

namespace __stdfs = std::experimental::filesystem;

//#define HEADER_MAX_SIZE 1048576

struct recursive_directory_range
{
	recursive_directory_range(__stdfs::path p) : p_(p) {}

	auto begin() { return __stdfs::recursive_directory_iterator(p_); }
	auto end() { return __stdfs::recursive_directory_iterator(); }

	__stdfs::path p_;
};

class MPacker
{
public:
	//constructor
	MPacker(const __stdfs::path outPath, PackType type = PackType::raw)
	{
		outFile = outPath;
		packType = type;
	}

	//destructor
	~MPacker()
	{

	}

	/*
	 * select directory for packing

	 * output file
	 *   >>header
	 *     >>header size(uInt)
	 *		>>origin(before compressed) header size(uInt)
	 *      >>header content
	 *        >>per file header length(uInt)
	 *        >>file path (MAX_PATH 260 characters for windows from msdn)
	 *        >>file start pos(in output file regardless of header)
	 *        >>file end pos(in output file regardless of header)
	 *   >>file contents
	 *
	 */
	void Pack(const __stdfs::path dirPath)
	{
		using namespace std;
		if (!__stdfs::is_directory(dirPath))
		{
			wcout << L"Path:[" << dirPath << L"] is not directory!"<< endl;
			return;
		}

#pragma region pack files data into temp file
		__stdfs::path tempFile = __stdfs::path(outFile);
		tempFile.concat(temp_suffix);
		fstream temp_ofs(tempFile, ios::binary |ios::in |ios::out| ios::trunc);
		if (temp_ofs.bad())
		{
			wcout << L"fstream error for temp file:" << tempFile << endl;
			return;
		}

		//data input buffer
		char buf[BUFFER_SIZE];

		__stdfs::path dirAbsolute = __stdfs::absolute(dirPath);
		vector<FileDataWithPath> files;
		//iterate files
		for (auto& it : recursive_directory_range(dirPath))
		{
			if (!__stdfs::is_directory(it))
			{
				cout << "start packing file:" << it << endl;
				fpos_t start = temp_ofs.tellp();
				UINT64 size = WriteFileData(it, temp_ofs, buf);
				fpos_t end = temp_ofs.tellp();
				__stdfs::path relativePath = UncompletePath(__stdfs::absolute(it), dirAbsolute);
				files.push_back(FileDataWithPath(relativePath.generic_wstring(), start, end, size));
				cout << "finish packing file:" << it << endl;
			}
		}
#pragma endregion

#pragma region write indexes in the output file
		ofstream ofs(outFile, ios::binary | ios::out | ios::trunc);
		if (ofs.bad())
		{
			wcout << L"ofstream error for file:" << outFile << endl;
			return;
		}

		WriteIndexes(ofs, files);
		cout << "finished writing header" << endl;
#pragma endregion
		
#pragma region concat 2 files
		
		ofs.seekp(0, ofs.end);
		temp_ofs.seekp(0, temp_ofs.beg);
		ofs << temp_ofs.rdbuf();
		
		ofs.close();
#pragma endregion

#pragma region delete temp file
		temp_ofs.close();
		
		if (remove(tempFile) != 0)
		{
			wcout << L"remove temp file failed!" << endl;
			return;
		}
		
#pragma endregion
	}


private:
	//single file
	UINT64 WriteFileData(const __stdfs::path& inFilePath, std::ostream & os,char *buffer)
	{
		using namespace std;
		ifstream ifs(inFilePath, ios::binary | ios::in);
		if (ifs.bad())
		{
			wcout << L"ifstream error for file:" << inFilePath << endl;
			return -1;
		}

		UINT64 size = WriteFileData(ifs, os, buffer);
		ifs.close();
		return size;
	}

	UINT64 WriteFileData(std::istream& is, std::ostream & os, char *buffer) {
		using namespace std;
		UINT64 fileSize = 0;

		is.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);
		if (packType == PackType::raw)
		{
			while (!is.eof()) {
				streamsize size = is.read(buffer, BUFFER_SIZE).gcount();
				os.write(buffer, size);
				fileSize += size;
			}
		}
		else if (packType == PackType::zlib)
		{
			if (ZlibDeflate(is, os,(unsigned char*) buffer, fileSize) != Z_OK)
			{
				cout << "zlib deflate error!" << endl;
			}
		}
		return fileSize;
		//return os.tellp().seekpos;
	}

	//get relative path
	const static __stdfs::path UncompletePath(__stdfs::path const path, __stdfs::path const base) {
		if (path.has_root_path()) {
			if (path.root_path() != base.root_path()) {
				return path;
			}
			else {
				return UncompletePath(path.relative_path(), base.relative_path());
			}
		}
		else {
			if (base.has_root_path()) {
				throw "cannot uncomplete a path relative path from a rooted base";
			}
			else {
				typedef __stdfs::path::const_iterator path_iterator;
				path_iterator path_it = path.begin();
				path_iterator base_it = base.begin();
				while (path_it != path.end() && base_it != base.end()) {
					if (*path_it != *base_it) break;
					++path_it; ++base_it;
				}
				__stdfs::path result;
				for (; base_it != base.end(); ++base_it) {
					result /= "..";
				}
				for (; path_it != path.end(); ++path_it) {
					result /= *path_it;
				}
				return result;
			}
		}
	}

	//REF: https://stackoverflow.com/questions/4538586/how-to-compress-a-buffer-with-zlib

	int ZlibDeflate(std::istream& is, std::ostream & os, int level = Z_BEST_COMPRESSION)
	{
		int ret;
		z_stream strm;
		unsigned char in[BUFFER_SIZE];
		unsigned char out[BUFFER_SIZE];

		/* allocate deflate state */
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit(&strm, level);
		if (ret != Z_OK)
			return ret;
			

		strm.next_out = out;
		strm.avail_out = BUFFER_SIZE;

		/* compress until end of file */
		while (!is.eof()) {
			strm.avail_in = (uInt)is.read((char *)in, BUFFER_SIZE).gcount();
			if (strm.avail_in == 0)
			{
				break;
			}
			strm.next_in = in;
			int res = deflate(&strm, Z_NO_FLUSH);
			assert(res == Z_OK);
			if (strm.avail_out == 0)
			{
				os.write((char *)out, BUFFER_SIZE);
				strm.next_out = out;
				strm.avail_out = BUFFER_SIZE;
			}

		/* done when last data in file processed */
		}


		int deflate_res = Z_OK;
		while (deflate_res == Z_OK)
		{
			if (strm.avail_out == 0)
			{
				os.write((char *)out, BUFFER_SIZE);
				strm.next_out = out;
				strm.avail_out = BUFFER_SIZE;
			}
			deflate_res = deflate(&strm, Z_FINISH);
		}

		assert(deflate_res == Z_STREAM_END);        /* stream will be complete */

		os.write((char *)out, BUFFER_SIZE - strm.avail_out);
											/* clean up and return */
		deflateEnd(&strm);
		return Z_OK;
	}


	//use zlib to compress data
	int ZlibDeflate(std::istream& is, std::ostream & os, unsigned char* in, UINT64& oSize, int level = Z_BEST_COMPRESSION)
	{
		int ret;
		z_stream strm;
		//unsigned char in[BUFFER_SIZE];
		unsigned char out[BUFFER_SIZE];
		oSize = 0;
		/* allocate deflate state */
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit(&strm, level);
		if (ret != Z_OK)
			return ret;


		strm.next_out = out;
		strm.avail_out = BUFFER_SIZE;

		/* compress until end of file */
		while (!is.eof()) {
			strm.avail_in = (uInt)is.read((char *) in, BUFFER_SIZE).gcount();
			
			if (strm.avail_in == 0)
			{
				break;
			}

			oSize += strm.avail_in;

			strm.next_in = in;
			int res = deflate(&strm, Z_NO_FLUSH);
			assert(res == Z_OK);
			if (strm.avail_out == 0)
			{
				os.write((char *)out, BUFFER_SIZE);
				strm.next_out = out;
				strm.avail_out = BUFFER_SIZE;
			}

			/* done when last data in file processed */
		}


		int deflate_res = Z_OK;
		while (deflate_res == Z_OK)
		{
			if (strm.avail_out == 0)
			{
				os.write((char *)out, BUFFER_SIZE);
				strm.next_out = out;
				strm.avail_out = BUFFER_SIZE;
			}
			deflate_res = deflate(&strm, Z_FINISH);
		}

		assert(deflate_res == Z_STREAM_END);        /* stream will be complete */

		os.write((char *)out, BUFFER_SIZE - strm.avail_out);

		std::streampos end = os.tellp();
		/* clean up and return */
		deflateEnd(&strm);
		return Z_OK;
	}


	void IssueHeaderOutput(std::ostream & os, std::istream & is)
	{
		char buffer[BUFFER_SIZE];
		if (packType == PackType::raw)
		{
			os.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);
			os << is.rdbuf();
		}
		else if (packType == PackType::zlib)
		{
			UINT64 outSize = 0;
			ZlibDeflate(is, os);
		}
	}

	//insert header at beginning to output file
	void WriteIndexes(std::ostream & os, const std::vector<FileDataWithPath> files) {
		using namespace std;
		stringstream sstream;
		size_t offset = 0;
		os.seekp(8, os.beg);
		//data_length(size_t) path(path_size) start(ft_pos) end(ft_pos)
		for (auto const& fd: files)
		{
			size_t path_size = fd.subPath.size() * sizeof(wchar_t);
			size_t data_length = path_size + 28;
			sstream.write(reinterpret_cast<const char*>(&data_length), 4);
			sstream.write(reinterpret_cast<const char*>(fd.subPath.c_str()), path_size);
			sstream.write(reinterpret_cast<const char*>(&fd.start), 8);
			sstream.write(reinterpret_cast<const char*>(&fd.end), 8);
			sstream.write(reinterpret_cast<const char*>(&fd.originSize), 8);
			offset += data_length;
		}
		IssueHeaderOutput(os, sstream);
		size_t output_size = (size_t)os.tellp() + 4;
		os.seekp(os.beg);
		os.write((const char*)&output_size, 4); 
		os.seekp(4, os.beg);
		os.write((const char*)&offset, 4);
		os.flush();
		////write endian data
		//os.seekp(os.beg);
		//char endianChar = is_big_endian ? 1 : 0;
		//os.write(&endianChar, 1);
	}

	PackType packType = PackType::raw;
	__stdfs::path outFile;
	std::wstring temp_suffix = L".temp";
	//const static bool is_big_endian;
	//const static bool IsBigEndian()
	//{
	//	union {
	//		uint32_t i;
	//		char c[4];
	//	} bint = { 0x01020304 };

	//	return bint.c[0] == 1;
	//}
};

//const bool Packer::is_big_endian = Packer::IsBigEndian();