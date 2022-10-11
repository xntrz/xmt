#pragma once

enum FileSeek_t
{
    FileSeek_Begin = 0,
    FileSeek_Current,
    FileSeek_End,
};

enum FileStat_t
{
    FileStat_Ready = 0,
    FileStat_Busy,
    FileStat_Error,
};