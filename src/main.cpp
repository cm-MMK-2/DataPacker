#include "Packer.hpp"
#include "Unpacker.hpp"

int main()
{
	//usage
	MPacker packer(L"D:\\data.mpac", PackType::zlib);
	std::wstring path;
	std::cout << "path for pack:" << std::endl;
	std::wcin >> path;
	packer.Pack(path);
	std::cout << "pack finished!" << std::endl;


	MUnpacker unpacker(L"D:\\data.mpac", PackType::zlib);
	unpacker.Unpack();
	std::cout << "unpack finished!" << std::endl;
	getchar();


	return 0;
}