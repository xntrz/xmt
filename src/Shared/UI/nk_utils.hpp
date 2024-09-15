#pragma once

#include "nk_includes.hpp"


/* converts utf8 string to wide string */
DLLSHARED std::wstring nku_utf8_to_wstr(const std::string& str);

/**
 *  adjusts window size to fit all content on it
 *  so its better to call it at the end of draw proc
 *  NOTE: this changing min size of window
 */
DLLSHARED void nku_adjust_window_size(struct nkgdi_window* wnd, struct nk_context* ctx);


/**
 *  variation of functions that returns current row position in X and Y vars
 */
DLLSHARED void nku_get_row_pos(struct nkgdi_window* wnd, struct nk_context* ctx, float* x, float* y);
DLLSHARED float nku_get_row_pos_x(struct nkgdi_window* wnd, struct nk_context* ctx);
DLLSHARED float nku_get_row_pos_y(struct nkgdi_window* wnd, struct nk_context* ctx);


/**
 *  display nk_label as time formatted text
 */
DLLSHARED void nku_label_time_format(struct nk_context* ctx, uint32 time, nk_flags align);