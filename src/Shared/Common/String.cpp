#include "String.hpp"


std::string Str_wc2mb(const std::wstring& str)
{
    return std::string(str.begin(), str.end());
};


std::string Str_tc2mb(const std::tstring& str)
{   
	return std::string(str.begin(), str.end());
};


std::wstring Str_mb2wc(const std::string& str)
{
    return std::wstring(str.begin(), str.end());
};


std::wstring Str_tc2wc(const std::tstring& str)
{
	return std::wstring(str.begin(), str.end());
};


void Str_SplitGrp(std::string& str, char grp, int32 pos)
{
    for (int32 i = int32(str.length()) - pos; i > 0; i -= pos)
        str.insert(i, 1, grp);
};