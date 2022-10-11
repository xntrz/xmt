#include "WebUtils.hpp"

#include "Shared/Common/Random.hpp"


void WebRemoveCharMask(std::string& String, const char* CharMask)
{
	for (uint32 i = 0; i < std::strlen(CharMask); i++)
	{
		String.erase(
			std::remove(String.begin(), String.end(), CharMask[i]), String.end()
		);
	};	
};


std::string WebRndHexString(int32 Len, bool FlagUpper)
{
	std::string Result;
	Result.reserve(Len + 1);

	static char HexLC[] = "0123456789abcdef";
	static char HexUC[] = "0123456789ABCDEF";
	const char* Hex = (FlagUpper ? HexUC : HexLC);
	
	for (int32 i = 0; i < Len; ++i)
		Result.push_back( Hex[RndInt32(0, (sizeof(Hex) - 1))] );

	return Result;
};


std::string WebUrlEncode(const std::string& Url)
{
	std::string Result;
	Result.reserve(Url.length() * 3 + 1);

	static char Hex[] = "0123456789abcdef";
	
	for (int32 i = 0; i < int32(Url.length()); ++i)
	{
		char Ch = Url[i];

		if ((Ch >= 'a' && Ch <= 'z') ||
			(Ch >= 'A' && Ch <= 'Z') ||
			(Ch >= '0' && Ch <= '9'))
		{
			Result.push_back(Ch);
		}
		else
		{
			Result.push_back('%');
			Result.push_back(Hex[Ch >> 4]);
			Result.push_back(Hex[Ch & 15]);
		};
	};

	return Result;
};


std::string WebUrlExtractProto(const std::string& Url)
{
	auto p0 = Url.find("://");
	if (p0 != std::string::npos)
		return Url.substr(0, p0);
	else
		return{};
};


std::string WebUrlExtractDomain(const std::string& Url)
{
	auto p0 = Url.find("://");
	if (p0 != std::string::npos)
		p0 += 3;
	else
		p0 = 0;

	auto p1 = Url.find_first_of(':', p0);
	if (p1 != std::string::npos)
		return Url.substr(p0, p1 - p0);

	auto p2 = Url.find_first_of('/', p0);
	if (p2 != std::string::npos)
		return Url.substr(p0, p2 - p0);

	return Url.substr(p0, p2);
};


std::string WebUrlExtractPort(const std::string& Url)
{
	auto p0 = Url.find("://");
	if (p0 != std::string::npos)
		p0 += 3;
	else
		p0 = 0;

	auto p1 = Url.find_first_of(':', p0);
	if (p1 != std::string::npos)
	{
		++p1;
		auto p2 = Url.find_first_of('/', p1);
		return Url.substr(p1, (p2 != std::string::npos ? (p2 - p1) : p2));
	};

	return{};
};


std::string WebUrlExtractPath(const std::string& Url)
{
	auto p0 = Url.find("://");
	if (p0 != std::string::npos)
		p0 += 3;
	else
		p0 = 0;

	auto p1 = Url.find_first_of('/', p0);
	if (p1 != std::string::npos)
	{
		++p1;
		auto p2 = Url.find_first_of('?', p1);
		return Url.substr(p1, (p2 != std::string::npos ? (p2 - p1) : p2));
	};

	return{};
};


std::string WebUrlExtractParam(const std::string& Url)
{
	auto p0 = Url.find("://");
	if (p0 != std::string::npos)
		p0 += 3;
	else
		p0 = 0;

	auto p1 = Url.find_first_of('?', p0);
	if (p1 != std::string::npos)
	{
		++p1;
		auto p2 = Url.find_first_of('#', p1);
		return Url.substr(p1, (p2 != std::string::npos ? (p2 - p1) : p2));
	};

	return{};
};


std::string WebUrlExtractFragment(const std::string& Url)
{
	auto p0 = Url.find_first_of('#');
	if (p0 != std::string::npos)
	{
		p0 += 1;
		return Url.substr(p0);
	};

	return{};
};


std::string WebSubstr(const std::string& Src, const std::string& Begin, const std::string& End)
{
	std::string Result;

	if (Src.empty())
		return Result;

	std::size_t PosBegin = Src.find(Begin);
	if (PosBegin == std::string::npos)
		return Result;

	std::size_t PosEnd = Src.find(End, PosBegin + Begin.length());
	if (PosEnd == std::string::npos)
		return Result;

	Result = Src.substr(
		PosBegin + Begin.length(),
		PosEnd - (PosBegin + Begin.length())
	);

	return Result;
};


std::string WebSubstrEnd(const std::string& Src, const std::string& Begin, const std::string& End)
{
	std::size_t PosBegin = Src.find_first_of(Begin);
	if (PosBegin == std::string::npos)
		return {};

	std::size_t PosEnd = Src.find_last_of(End);
	if (PosEnd == std::string::npos)
		return {};

	return Src.substr(PosBegin, (PosEnd - PosBegin));
};


std::string WebSubstrMem(const char* Mem, uint32 MemLen, const std::string& Begin, const std::string& End)
{
	std::string Result;

	if (!Mem || !MemLen)
		return Result;

	const char* PtrBegin = std::strstr(Mem, Begin.c_str());
	if (!PtrBegin)
		return Result;

	const char* PtrEnd = std::strstr(PtrBegin + Begin.length(), End.c_str());
	if (!PtrEnd)
		return Result;

	Result = std::string(
		PtrBegin + Begin.length(),
		PtrEnd - (PtrBegin + Begin.length())
	);

	return Result;
};


void WebStrRep(std::string& Src, const std::string& What, const std::string& To)
{
	for (int32 i = 0; ; i += To.length())
	{
		i = Src.find(What, i);
		
		if (i == std::string::npos)
			break;

		Src.erase(i, What.length());
		Src.insert(i, To);
	};
};