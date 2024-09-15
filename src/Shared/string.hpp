#pragma once


inline void strsplitgrp(std::string& str, char grp = '.', std::size_t pos = 3u)
{
	for (int i = int(str.length() - pos); i > 0; i -= int(pos))
        str.insert(std::size_t(i), 1u, grp);
};


inline void strtolower(std::string& str)
{
    for (auto& it : str)
        it = std::tolower(int(it));
};