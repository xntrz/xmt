#pragma once


//
//  Converts Widechar string to Multibyte string
//
DLLSHARED std::string Str_wc2mb(const std::wstring& str);

//
//  Converts TCHAR string to Multibyte string
//
DLLSHARED std::string Str_tc2mb(const std::tstring& str);

//
//  Converts Multibyte string to Widechar string
//
DLLSHARED std::wstring Str_mb2wc(const std::string& str);

//
//  Converts TCHAR string to Widechar string
//
DLLSHARED std::wstring Str_tc2wc(const std::tstring& str);

//
//  Split string to group
//
DLLSHARED void Str_SplitGrp(std::string& str, char grp = '.', int32 pos = 3);