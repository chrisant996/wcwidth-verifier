// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include "str_iter.h"

//------------------------------------------------------------------------------
typedef int32 wcwidth_t (char32_t);
extern wcwidth_t *wcwidth;

struct wcwidth_modes
{
    int32 color_emoji = 0;          // 0=no change, 1=force true, -1=force false
    int32 only_ucs2 = 0;            // 0=no change, 1=force true, -1=force false
};

// The first call to this automatically detects the modes.  But the modes can
// be overridden by passing in a wcwidth_modes struct.  Any fields that are 0
// have no effect, but any non-zero field forcibly override the corresponding
// mode (1 forces the mode true, and -1 forces the mode false).
void initialize_wcwidth(const wcwidth_modes* modes=nullptr);
bool get_color_emoji();
bool get_only_ucs2();

bool is_combining(char32_t ucs);
bool is_east_asian_ambiguous(char32_t ucs);
bool is_CJK_codepage(UINT cp);

bool is_variant_selector(char32_t ucs);
bool is_possible_unqualified_half_width(char32_t ucs);
bool is_emoji(char32_t ucs);

//------------------------------------------------------------------------------
class combining_mark_width_scope
{
public:
    combining_mark_width_scope(int32 width);
    ~combining_mark_width_scope();
private:
    const int32 m_old;
};

//------------------------------------------------------------------------------
uint32 wcswidth(const char* s, uint32 len);

//------------------------------------------------------------------------------
class wcwidth_iter
{
public:
    explicit        wcwidth_iter(const char* s, int32 len=-1);
                    wcwidth_iter(const wcwidth_iter& i);
    char32_t        next();
    void            unnext();
    const char*     character_pointer() const { return m_chr_ptr; }
    uint32          character_length() const { return uint32(m_chr_end - m_chr_ptr); }
    int32           character_wcwidth_signed() const { return m_chr_wcwidth; }
    uint32          character_wcwidth_zeroctrl() const { return (m_chr_wcwidth < 0) ? 0 : m_chr_wcwidth; }
    uint32          character_wcwidth_onectrl() const { return (m_chr_wcwidth < 0) ? 1 : m_chr_wcwidth; }
    uint32          character_wcwidth_twoctrl() const { return (m_chr_wcwidth < 0) ? 2 : m_chr_wcwidth; }
    bool            character_is_emoji() const { return m_emoji; }
    const char*     get_pointer() const;
    void            reset_pointer(const char* s);
    bool            more() const;
    uint32          length() const;

private:
    void            consume_emoji_sequence();

private:
    str_iter        m_iter;
    char32_t        m_next;
    const char*     m_chr_ptr;
    const char*     m_chr_end;
    int32           m_chr_wcwidth = 0;
    bool            m_emoji = false;
};
