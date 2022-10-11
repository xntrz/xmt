#pragma once


class CBase64
{
public:
	static std::string Encode(const std::string& data);
	static std::string Decode(const std::string& data);
};