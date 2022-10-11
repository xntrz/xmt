#pragma once

#include "Shared/Common/Registry.hpp"


//
//  Utility class for simple save/load reg vars
//  Supported types for save or load:
//      int16
//      uint16
//      int32
//      uint32
//      float
//      double
//      bool
//      const char*
//      char[]
//      std::string
//
struct regset final
{
    template<class T, const int32 cnt = std::extent<T>::value>
    struct readers
    {
        static inline void read(int16& value, HOBJ hVar)
        {
            value = RegVarReadInt32(hVar);
        };

        static inline void read(uint16& value, HOBJ hVar)
        {
            value = RegVarReadUInt32(hVar);
        };

        static inline void read(int32& value, HOBJ hVar)
        {
            value = RegVarReadInt32(hVar);
        };

        static inline void read(uint32& value, HOBJ hVar)
        {
            value = RegVarReadUInt32(hVar);
        };

        static inline void read(float& value, HOBJ hVar)
        {
            value = RegVarReadFloat(hVar);
        };

        static inline void read(double& value, HOBJ hVar)
        {
            value = RegArgToDouble(RegVarReadString(hVar));
        };

        static inline void read(bool& value, HOBJ hVar)
        {
            value = RegArgToBool(RegVarReadString(hVar));
        };

        static inline void read(const char*& value, HOBJ hVar)
        {
            value = RegVarReadString(hVar);
        };

        static inline void read(char value[], HOBJ hVar)
		{
		    RegVarReadString(hVar, value, cnt);
		};

        static inline void read(std::string& value, HOBJ hVar)
        {
            char Buffer[256];
            Buffer[0] = '\0';
            RegVarReadString(hVar, Buffer, sizeof(Buffer));
            value = std::string(Buffer);
        };
    };

    template<class T>
    struct writers
    {
        static inline void write(int16& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, std::to_string(value).c_str());
        };

        static inline void write(uint16& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, std::to_string(value).c_str());
        };
        
        static inline void write(int32& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, std::to_string(value).c_str());
        };

        static inline void write(uint32& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, std::to_string(value).c_str());
        };

        static inline void write(float& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, std::to_string(value).c_str());
        };

        static inline void write(double& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, std::to_string(value).c_str());
        };

        static inline void write(bool& value, HOBJ hVar)
        {
            char Tmp[128];
            Tmp[0] = '\0';            
            RegBoolToArg(value, Tmp, sizeof(Tmp));            
            RegVarSetValue(hVar, Tmp);
        };

        static inline void write(const char*& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, value);
        };

        static inline void write(char value[], HOBJ hVar)
        {
            RegVarSetValue(hVar, value);
        };

        static inline void write(std::string& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, value.c_str());            
        };

        static inline void write(const std::string& value, HOBJ hVar)
        {
            RegVarSetValue(hVar, value.c_str());
        };
    };

    template<class T>
    static inline void load(T& value, const std::string& varname)
    {
        HOBJ hVar = RegVarFind(varname.c_str());
        if (hVar)
        {
            readers<T>::read(value, hVar);
        }
#ifdef _DEBUG
        else
        {
            OUTPUTLN("var \"%s\" not found for loading", varname.c_str());
        };
#endif           
    };

    template<class T>
    static inline void load(
        T& value,
        const std::string& varname,
        const std::vector<std::string>& arrmatch,
        const std::vector<T>& arrvals
    )
    {
        ASSERT(arrmatch.size() == arrvals.size());
        ASSERT(varname.length() > 0);

        std::string var;
        load(var, varname);        
        if (!var.empty())
        {
            for (int32 i = 0; i < int32(arrmatch.size()); ++i)
            {
                if (arrmatch[i].compare(var) == 0)
                {
                    value = arrvals[i];
                    return;
                };
            };
        };
    };

    template<class T>
    static inline void save(T& value, const std::string& varname)
    {
        HOBJ hVar = RegVarFind(varname.c_str());
        if (hVar)
        {
            writers<T>::write(value, hVar);
        }
#ifdef _DEBUG
        else
        {
            OUTPUTLN("var \"%s\" not found for saving", varname.c_str());
        };
#endif   
    };

    template<class T>
    static inline void save(
        T& value,
        const std::string& varname,
        const std::vector<std::string>& arrmatch
    )
    {
        static_assert(std::is_integral<T>::value, "not integral type");        
        ASSERT(arrmatch.size() > 0);
        ASSERT(value >= 0 && value < int32(arrmatch.size()));
        
        save(arrmatch[value], varname);
    };
};