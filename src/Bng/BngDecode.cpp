#include "BngDecode.hpp"

#include "Utils/Base64/Base64.hpp"
#include "Utils/SHA1/SHA1.hpp"


static const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
static const char KEY[] = "01ET1PP19TM3JR0EYMEG4SNX1H";


static inline int32 BngDecodeIndexof(char ch, const char* Alphabet, int32 AlphabetLen)
{
    if ((ch >= 'A') && (ch <= 'Z'))
        return (ch - 'A');

    if ((ch >= 'a') && (ch <= 'z'))
        return (ch - 'a') + 26;

    if ((ch >= '0') && (ch <= '9'))
        return (ch - '0') + 52;

    if (ch == '+')
        return 62;

    if (ch == '/')
        return 63;

    if (ch == '=')
        return 64;

    return 0;
};


static int32 BngDecodeCalcMagic(const std::string& Data)
{
    int32 iResult = 0;

    for (auto ch : Data)
        iResult += BngDecodeIndexof(ch, ALPHABET, sizeof(ALPHABET) - 1);

    return iResult;
};


static std::string BngDecode(const std::string& Data, int32 Magic)
{
    std::string Result;

    std::vector<int32> IndexArray;
    IndexArray.resize(Data.size());
    
    for (int32 i = 0; i < int32(Data.size()); ++i)
        IndexArray[i] = i;

    for (int32 i = int32(Data.size()); --i >= 0; )
    {
        int32 DecodedIndex = (Magic % (i + 1) + i) % Data.size();

        int32 A = IndexArray[DecodedIndex];
        int32 B = IndexArray[i];

        IndexArray[i] = A;
        IndexArray[DecodedIndex] = B;
    };

    Result.resize(Data.size());
    for (int32 i = int32(Data.size()); --i >= 0; )
        Result[IndexArray[i]] = Data[i];

    return Result;
};


std::string BngDecodeInitialState(const std::string& InitialState)
{
    //
    //	hg.js:10100
    //
    if (InitialState.empty())
        return {};

    int32 Magic = BngDecodeCalcMagic(InitialState + KEY);
    std::string Msg = BngDecode(InitialState, Magic);
    return CBase64::Decode(Msg);
};


std::string BngDecodeABSplitGroup(const std::string& ABSplitGroup)
{
    //
    //	hg.js:26865
    //
    if (ABSplitGroup.empty())
        return {};

    std::string Sub = ABSplitGroup.substr(40, 40);
    return Sub + sha1(ABSplitGroup);
};