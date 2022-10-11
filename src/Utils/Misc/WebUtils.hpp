#pragma once


//
//  Removes chars specified in CharMask from String
//
void WebRemoveCharMask(std::string& String, const char* CharMask);

//
//  Generates random hex string
//  By default generates in lowercase style, if FlagUpper specified generates in uppercase
//
std::string WebRndHexString(int32 Len, bool FlagUpper = false);

//
//  Encodes url 
//
std::string WebUrlEncode(const std::string& Url);

//
//  Extracts proto part from url
//
std::string WebUrlExtractProto(const std::string& Url);

//
//  Extracts domain part from url
//
std::string WebUrlExtractDomain(const std::string& Url);

//
//  Extracts port part from url
//
std::string WebUrlExtractPort(const std::string& Url);

//
//  Extracts path part from url
//
std::string WebUrlExtractPath(const std::string& Url);

//
//  Extracts query param part from url
//
std::string WebUrlExtractParam(const std::string& Url);

//
//  Extracts fragment part from url
//
std::string WebUrlExtractFragment(const std::string& Url);

//
//  Substr from "Src" with "Begin" and "End" boundary
//  "End" is FIRST encountered match after "Begin" in "Src"
//
std::string WebSubstr(const std::string& Src, const std::string& Begin, const std::string& End);

//
//  Substr from "Src" with "Begin" and "End" boundary
//  "End" is LAST encountered match from end of the "Src"
//
std::string WebSubstrEnd(const std::string& Src, const std::string& Begin, const std::string& End);

//
//  Doing same thing as WebSubstr but without allocating temporary "Src" that may be large
//
std::string WebSubstrMem(const char* Mem, uint32 MemLen, const std::string& Begin, const std::string& End);

//
//  Replaces "What" to "To" in "Src"
//
void WebStrRep(std::string& Src, const std::string& What, const std::string& To);