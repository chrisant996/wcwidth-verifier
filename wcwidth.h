// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

//------------------------------------------------------------------------------
typedef int wcwidth_t (char32_t);
extern "C" wcwidth_t *wcwidth;
extern "C" const char* is_assigned(char32_t ucs);
extern "C" int is_combining(char32_t ucs);
extern "C" int is_color_emoji(char32_t ucs);
extern "C" int is_east_asian_ambiguous(char32_t ucs);
extern "C" int is_ideograph(char32_t ucs);
extern "C" int is_CJK_codepage(UINT cp);
#ifdef __cplusplus
extern "C" int test_ambiguous_width_char(char32_t ucs, char32_t* peek=nullptr, unsigned int peek_len=0);
#else
extern "C" int test_ambiguous_width_char(char32_t ucs, char32_t* peek, unsigned int peek_len);
#endif
extern "C" int g_full_width_available;
extern "C" int g_color_emoji;
extern "C" int g_only_ucs2;

