#include "SHA1.hpp"

#include "cppsha1/sha1.hpp"

std::string sha1(const std::string& data)
{
	SHA1 sha;
	sha.update(data);
	return sha.final();
};