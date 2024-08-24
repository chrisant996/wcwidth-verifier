// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

//------------------------------------------------------------------------------
typedef int wcwidth_t (char32_t);
extern "C" wcwidth_t *wcwidth;
extern "C" int g_full_width_available;
extern "C" int g_color_emoji;

