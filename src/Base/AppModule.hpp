#pragma once

void AppModInitialize();
void AppModTerminate();
void AppModSetCurrent(std::size_t id);
std::size_t AppModGetCount();

bool
AppModGetInfo(
    std::size_t     id,
    HINSTANCE&      hInstamce,
    int32&          rcIcoId,
    std::string&    strTitle,
    std::string&    strDescription,
    bool            (*&ui_proc)(struct nkgdi_window*, struct nk_context*, bool)
);