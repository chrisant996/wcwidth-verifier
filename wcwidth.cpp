/*
 * This is an implementation of wcwidth() and wcswidth() (defined in
 * IEEE Std 1002.1-2001) for Unicode.
 *
 * http://www.opengroup.org/onlinepubs/007904975/functions/wcwidth.html
 * http://www.opengroup.org/onlinepubs/007904975/functions/wcswidth.html
 *
 * In fixed-width output devices, Latin characters all occupy a single
 * "cell" position of equal width, whereas ideographic CJK characters
 * occupy two such cells. Interoperability between terminal-line
 * applications and (teletype-style) character terminals using the
 * UTF-8 encoding requires agreement on which character should advance
 * the cursor by how many cell positions. No established formal
 * standards exist at present on which Unicode character shall occupy
 * how many cell positions on character terminals. These routines are
 * a first attempt of defining such behavior based on simple rules
 * applied to data provided by the Unicode Consortium.
 *
 * For some graphical characters, the Unicode standard explicitly
 * defines a character-cell width via the definition of the East Asian
 * FullWidth (F), Wide (W), Half-width (H), and Narrow (Na) classes.
 * In all these cases, there is no ambiguity about which width a
 * terminal shall use. For characters in the East Asian Ambiguous (A)
 * class, the width choice depends purely on a preference of backward
 * compatibility with either historic CJK or Western practice.
 * Choosing single-width for these characters is easy to justify as
 * the appropriate long-term solution, as the CJK practice of
 * displaying these characters as double-width comes from historic
 * implementation simplicity (8-bit encoded characters were displayed
 * single-width and 16-bit ones double-width, even for Greek,
 * Cyrillic, etc.) and not any typographic considerations.
 *
 * Much less clear is the choice of width for the Not East Asian
 * (Neutral) class. Existing practice does not dictate a width for any
 * of these characters. It would nevertheless make sense
 * typographically to allocate two character cells to characters such
 * as for instance EM SPACE or VOLUME INTEGRAL, which cannot be
 * represented adequately with a single-width glyph. The following
 * routines at present merely assign a single-cell width to all
 * neutral characters, in the interest of simplicity. This is not
 * entirely satisfactory and should be reconsidered before
 * establishing a formal standard in this area. At the moment, the
 * decision which Not East Asian (Neutral) characters should be
 * represented by double-width glyphs cannot yet be answered by
 * applying a simple rule from the Unicode database content. Setting
 * up a proper standard for the behavior of UTF-8 character terminals
 * will require a careful analysis not only of each Unicode character,
 * but also of each presentation form, something the author of these
 * routines has avoided to do so far.
 *
 * http://www.unicode.org/unicode/reports/tr11/
 *
 * Markus Kuhn -- 2007-05-26 (Unicode 5.0)
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version: http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */

/*
 *
 * MODIFIED FOR wcwidth-verifier BY https://github.com/chrisant996.
 *
 */

#include "main.h"
#include "wcwidth.h"

static int32 s_combining_mark_width = 0;
static bool s_only_ucs2 = false;
static bool s_win11 = false;

combining_mark_width_scope::combining_mark_width_scope(int32 width)
: m_old(s_combining_mark_width)
{
    s_combining_mark_width = width;
}

combining_mark_width_scope::~combining_mark_width_scope()
{
    s_combining_mark_width = m_old;
}

bool detect_ucs2_limitation(bool force)
{
    static bool s_inited_only_ucs2 = false;
    if (!s_inited_only_ucs2)
    {
#pragma warning(push)
#pragma warning(disable:4996)
        OSVERSIONINFO ver = { sizeof(ver) };
        if (GetVersionEx(&ver))
        {
            s_only_ucs2 = (ver.dwMajorVersion < 10);
            s_win11 = (ver.dwMajorVersion > 10 || (ver.dwMajorVersion == 10 && ver.dwBuildNumber >= 22000));
        }
        s_inited_only_ucs2 = true;
    }
    if (force)
    {
        s_only_ucs2 = true;
        s_inited_only_ucs2 = true;
    }
#pragma warning(pop)
    return s_only_ucs2;
}

static int32 resolve_ambiguous_wcwidth(char32_t ucs);

struct interval {
  char32_t first;
  char32_t last;
};

/* auxiliary function for binary search in interval table */
static int32 bisearch(char32_t ucs, const struct interval *table, int32 max) {
  int32 min = 0;
  int32 mid;

  if (ucs < table[0].first || ucs > table[max].last)
    return 0;
  while (max >= min) {
    mid = (min + max) / 2;
    if (ucs > table[mid].last)
      min = mid + 1;
    else if (ucs < table[mid].first)
      max = mid - 1;
    else
      return 1;
  }

  return 0;
}

#include "emoji-test.i"
#include "assigned-codepoints.i"

const emoji_form_sequence* get_emoji_form_sequence(char32_t ucs) {
  int32 min = 0;
  int32 max = _countof(emoji_forms);
  int32 mid;

  while (max > min) {
    mid = (min + max) / 2;
    if (ucs > emoji_forms[mid].ucs)
      min = mid + 1;
    else
      max = mid;
  }
  if (max == min && min < _countof(emoji_forms)) {
    assert(min < 1 || emoji_forms[min - 1].ucs < ucs);
    const emoji_form_sequence* x = emoji_forms + min;
    if (x->ucs == ucs)
      return x;
  }

  return nullptr;
}

const char* is_assigned(char32_t ucs) {
  const struct codepoint* table = c_assigned;
  int32 min = 0;
  int32 max = _countof(c_assigned) - 1;
  int32 mid;

  while (max >= min) {
    mid = (min + max) / 2;
    if (ucs > table[mid].ucs)
      min = mid + 1;
    else if (ucs < table[mid].ucs)
      max = mid - 1;
    else
      return table[mid].desc;
  }

  const struct codepoint_range* atable = c_assigned_areas;
  min = 0;
  max = _countof(c_assigned_areas) - 1;

  while (max >= min) {
    mid = (min + max) / 2;
    if (ucs > atable[mid].last)
      min = mid + 1;
    else if (ucs < atable[mid].first)
      max = mid - 1;
    else
      return atable[mid].desc;
  }

  return 0;
}

bool is_ideograph(char32_t ucs) {
  const char* name = is_assigned(ucs);
  return name && strstr(name, "Ideograph");
}

bool is_kana(char32_t ucs) {
  static const struct interval kana[] = {
    { 0x1100, 0x115f },     // Hangul Jamo (various characters)
    { 0x302e, 0x302f },     // CJK Symbols and Punctuation (Hangul single/double dot tone marks)
    { 0xa960, 0xa97c },     // Hangul Jamo Extended-A (various characters)
    { 0x18800, 0x18aff },   // Tangut Components
    { 0x18b00, 0x18cff },   // Khitan Small Script
    { 0x1aff0, 0x1aff3 },   // Kana Extended-B (various tones)
    { 0x1aff5, 0x1affb },   // Kana Extended-B (various tones)
    { 0x1affd, 0x1affe },   // Kana Extended-B (various tones)
    { 0x1b000, 0x1b0ff },   // Kana Supplement
    { 0x1b100, 0x1b122 },   // Kana Extended-A
    { 0x1b132, 0x1b132 },   // Small Kana Extension (various letters)
    { 0x1b150, 0x1b152 },   // Small Kana Extension (various letters)
    { 0x1b155, 0x1b155 },   // Small Kana Extension (various letters)
    { 0x1b164, 0x1b167 },   // Small Kana Extension (various letters)
    { 0x1b170, 0x1b2ff },   // Nushu
  };
  return !!bisearch(ucs, kana, _countof(kana) - 1);
}

static bool is_cjk_halfwidth(char32_t ucs) {
  static const struct interval halfwidth_exceptions[] = {
    { 0x303f, 0x303f },     // Ideographic Half Fill Space
    { 0x3248, 0x324f },     // Enclosed CJK Letters and Months (circle number on black square)
    { 0x4dc0, 0x4dff },     // Yijing Hexagram Symbols
  };
  return !!bisearch(ucs, halfwidth_exceptions, _countof(halfwidth_exceptions) - 1);
}

/* sorted list of non-overlapping intervals of non-spacing characters */
/* generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B c" */
static const struct interval combining[] = {
  { 0x0300, 0x036F }, { 0x0483, 0x0486 }, { 0x0488, 0x0489 },
  { 0x0591, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
  { 0x05C4, 0x05C5 }, { 0x05C7, 0x05C7 }, { 0x0600, 0x0603 },
  { 0x0610, 0x0615 }, { 0x064B, 0x065E }, { 0x0670, 0x0670 },
  { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
  { 0x070F, 0x070F }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
  { 0x07A6, 0x07B0 }, { 0x07EB, 0x07F3 }, { 0x0901, 0x0902 },
  { 0x093C, 0x093C }, { 0x0941, 0x0948 }, { 0x094D, 0x094D },
  { 0x0951, 0x0954 }, { 0x0962, 0x0963 }, { 0x0981, 0x0981 },
  { 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD },
  { 0x09E2, 0x09E3 }, { 0x0A01, 0x0A02 }, { 0x0A3C, 0x0A3C },
  { 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D },
  { 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC },
  { 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD },
  { 0x0AE2, 0x0AE3 }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
  { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 }, { 0x0B4D, 0x0B4D },
  { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 }, { 0x0BC0, 0x0BC0 },
  { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 }, { 0x0C46, 0x0C48 },
  { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 }, { 0x0CBC, 0x0CBC },
  { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
  { 0x0CE2, 0x0CE3 }, { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D },
  { 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 },
  { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E },
  { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC },
  { 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 },
  { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E },
  { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 },
  { 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 },
  { 0x1032, 0x1032 }, { 0x1036, 0x1037 }, { 0x1039, 0x1039 },
  { 0x1058, 0x1059 }, { 0x1160, 0x11FF }, { 0x135F, 0x135F },
  { 0x1712, 0x1714 }, { 0x1732, 0x1734 }, { 0x1752, 0x1753 },
  { 0x1772, 0x1773 }, { 0x17B4, 0x17B5 }, { 0x17B7, 0x17BD },
  { 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x17DD, 0x17DD },
  { 0x180B, 0x180D }, { 0x18A9, 0x18A9 }, { 0x1920, 0x1922 },
  { 0x1927, 0x1928 }, { 0x1932, 0x1932 }, { 0x1939, 0x193B },
  { 0x1A17, 0x1A18 }, { 0x1B00, 0x1B03 }, { 0x1B34, 0x1B34 },
  { 0x1B36, 0x1B3A }, { 0x1B3C, 0x1B3C }, { 0x1B42, 0x1B42 },
  { 0x1B6B, 0x1B73 }, { 0x1DC0, 0x1DCA }, { 0x1DFE, 0x1DFF },
  { 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x2060, 0x2063 },
  { 0x206A, 0x206F }, { 0x20D0, 0x20EF }, { 0x302A, 0x302F },
  { 0x3099, 0x309A }, { 0xA806, 0xA806 }, { 0xA80B, 0xA80B },
  { 0xA825, 0xA826 }, { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F },
  { 0xFE20, 0xFE23 }, { 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB },
  { 0x10A01, 0x10A03 }, { 0x10A05, 0x10A06 }, { 0x10A0C, 0x10A0F },
  { 0x10A38, 0x10A3A }, { 0x10A3F, 0x10A3F }, { 0x1D167, 0x1D169 },
  { 0x1D173, 0x1D182 }, { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD },
  { 0x1D242, 0x1D244 }, { 0x1F3FB, 0x1F3FF }, { 0xE0001, 0xE0001 },
  { 0xE0020, 0xE007F }, { 0xE0100, 0xE01EF }
};

/* The following two functions define the column width of an ISO 10646
 * character as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - SOFT HYPHEN (U+00AD) has a column width of 1.
 *
 *    - Other format characters (general category code Cf in the Unicode
 *      database) and ZERO WIDTH SPACE (U+200B) have a column width of 0.
 *
 *    - Hangul Jamo medial vowels and final consonants (U+1160-U+11FF)
 *      have a column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      Full-width (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */

static int32 mk_wcwidth(char32_t ucs)
{
  /* test for 8-bit control characters */
  if (ucs == 0)
    return 0;
  if (ucs < 32)
    return -1;
  if (ucs <= 0x7e)
    return 1;
  if (ucs < 0xa0)
    return -1;

  /* special processing when color emoji support is enabled */
  if (g_color_emoji) {
    /* characters with unqualified forms are width 1 without FE0F/etc */
    if (bisearch(ucs, possible_unqualified_half_width, _countof(possible_unqualified_half_width) - 1))
      return 1;
    /* color emoji are width 2 */
    if (bisearch(ucs, emojis, _countof(emojis) - 1))
      return 2;
  }

  /* binary search in table of non-spacing characters */
  if (bisearch(ucs, combining, _countof(combining) - 1))
    return s_combining_mark_width;

  /* if we arrive here, ucs is not a combining or C0/C1 control character */
  if (ucs < 0x1100 || !g_full_width_available)
    return 1;
  if (ucs <= 0x115f)                      /* Hangul Jamo init. consonants */
    return 2;                                   // (but wcwidth expected 1)
  if (ucs == 0x2329 || ucs == 0x232a)
    return 2;
  if (ucs >= 0x2e80 && ucs <= 0xa4cf)
    return 1 + !is_cjk_halfwidth(ucs);          // (but wcwidth expected 2)
  if (ucs >= 0xac00 && ucs <= 0xd7a3)     /* Hangul Syllables */
    return 1 + !is_cjk_halfwidth(ucs);          // (but wcwidth expected 2)
  if ((ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
      (ucs >= 0xfe10 && ucs <= 0xfe19) || /* Vertical forms */
      (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
      (ucs >= 0xff00 && ucs <= 0xff60) || /* Fullwidth Forms */
      (ucs >= 0xffe0 && ucs <= 0xffe6) ||
      (ucs >= 0x20000 && ucs <= 0x2fffd) ||
      (ucs >= 0x30000 && ucs <= 0x3fffd))
    return 2;
  return 1;
}

static int32 mk_wcwidth_ucs2(char32_t ucs)
{
  /* test for 8-bit control characters */
  if (ucs == 0)
    return 0;
  if (ucs < 32)
    return -1;
  if (ucs <= 0x7e)
    return 1;
  if (ucs < 0xa0)
    return -1;

  /* special processing when color emoji support is enabled */
  if (g_color_emoji) {
    /* characters with unqualified forms are width 1 without FE0F/etc */
    if (bisearch(ucs, possible_unqualified_half_width, _countof(possible_unqualified_half_width) - 1))
      return 1;
    /* color emoji are width 2 */
    if (bisearch(ucs, emojis, _countof(emojis) - 1))
      return 2;
  } else if (s_win11) {
    /* Windows 11 conhost renders some emoji as full width monochrome glyphs */
    if (bisearch(ucs, mono_emojis, _countof(mono_emojis) - 1))
      return 2;
  }

  /* binary search in table of non-spacing characters */
  if (bisearch(ucs, combining, _countof(combining) - 1))
    return s_combining_mark_width;

  /* if we arrive here, ucs is not a combining or C0/C1 control character */
  if (ucs < 0x1100 || !g_full_width_available)
    return 1;
  if (ucs <= 0x115f)                      /* Hangul Jamo init. consonants */
    return 2;                                   // (but wcwidth expected 1)
  if (ucs == 0x2329 || ucs == 0x232a)
    return 2;
  if (ucs >= 0x2e80 && ucs <= 0xa4cf)
    return 1 + !is_cjk_halfwidth(ucs);          // (but wcwidth expected 2)
  if (ucs >= 0xac00 && ucs <= 0xd7a3)     /* Hangul Syllables */
    return 1 + !is_cjk_halfwidth(ucs);          // (but wcwidth expected 2)
  if ((ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
      (ucs >= 0xfe10 && ucs <= 0xfe19) || /* Vertical forms */
      (ucs >= 0xfe30 && ucs <= 0xfe6f))   /* CJK Compatibility Forms */
    return 2;
  if (s_win11 &&                                // ignore unless >= Win11
      (ucs >= 0xff00 && ucs <= 0xff60) || /* Fullwidth Forms */
      (ucs >= 0xffe0 && ucs <= 0xffe6))
    return 2;
  if (ucs >= 0x10000)                     /* UCS2 on Windows 8.1 and lower */
    return 2;
  return 1;
}


// Use wcswidth() or wcwidth_iter instead:  they handle fully qualified color
// emoji, which requires sometimes looking at MULTIPLE codepoints to determine
// the width.
#if 0
static int32 mk_wcswidth(const char32_t *pwcs, size_t n)
{
  int32 w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = mk_wcwidth(*pwcs)) < 0)
      return -1;
    else
      width += w;

  return width;
}
#endif


/* sorted list of non-overlapping intervals of East Asian Ambiguous
 * characters, generated by "uniset +WIDTH-A -cat=Me -cat=Mn -cat=Cf c" */
static const struct interval ambiguous[] = {
  { 0x00A1, 0x00A1 }, { 0x00A4, 0x00A4 }, { 0x00A7, 0x00A8 },
  { 0x00AA, 0x00AA }, { 0x00AE, 0x00AE }, { 0x00B0, 0x00B4 },
  { 0x00B6, 0x00BA }, { 0x00BC, 0x00BF }, { 0x00C6, 0x00C6 },
  { 0x00D0, 0x00D0 }, { 0x00D7, 0x00D8 }, { 0x00DE, 0x00E1 },
  { 0x00E6, 0x00E6 }, { 0x00E8, 0x00EA }, { 0x00EC, 0x00ED },
  { 0x00F0, 0x00F0 }, { 0x00F2, 0x00F3 }, { 0x00F7, 0x00FA },
  { 0x00FC, 0x00FC }, { 0x00FE, 0x00FE }, { 0x0101, 0x0101 },
  { 0x0111, 0x0111 }, { 0x0113, 0x0113 }, { 0x011B, 0x011B },
  { 0x0126, 0x0127 }, { 0x012B, 0x012B }, { 0x0131, 0x0133 },
  { 0x0138, 0x0138 }, { 0x013F, 0x0142 }, { 0x0144, 0x0144 },
  { 0x0148, 0x014B }, { 0x014D, 0x014D }, { 0x0152, 0x0153 },
  { 0x0166, 0x0167 }, { 0x016B, 0x016B }, { 0x01CE, 0x01CE },
  { 0x01D0, 0x01D0 }, { 0x01D2, 0x01D2 }, { 0x01D4, 0x01D4 },
  { 0x01D6, 0x01D6 }, { 0x01D8, 0x01D8 }, { 0x01DA, 0x01DA },
  { 0x01DC, 0x01DC }, { 0x0251, 0x0251 }, { 0x0261, 0x0261 },
  { 0x02C4, 0x02C4 }, { 0x02C7, 0x02C7 }, { 0x02C9, 0x02CB },
  { 0x02CD, 0x02CD }, { 0x02D0, 0x02D0 }, { 0x02D8, 0x02DB },
  { 0x02DD, 0x02DD }, { 0x02DF, 0x02DF }, { 0x0391, 0x03A1 },
  { 0x03A3, 0x03A9 }, { 0x03B1, 0x03C1 }, { 0x03C3, 0x03C9 },
  { 0x0401, 0x0401 }, { 0x0410, 0x044F }, { 0x0451, 0x0451 },
  { 0x2010, 0x2010 }, { 0x2013, 0x2016 }, { 0x2018, 0x2019 },
  { 0x201C, 0x201D }, { 0x2020, 0x2022 }, { 0x2024, 0x2027 },
  { 0x2030, 0x2030 }, { 0x2032, 0x2033 }, { 0x2035, 0x2035 },
  { 0x203B, 0x203B }, { 0x203E, 0x203E }, { 0x2074, 0x2074 },
  { 0x207F, 0x207F }, { 0x2081, 0x2084 }, { 0x20AC, 0x20AC },
  { 0x2103, 0x2103 }, { 0x2105, 0x2105 }, { 0x2109, 0x2109 },
  { 0x2113, 0x2113 }, { 0x2116, 0x2116 }, { 0x2121, 0x2122 },
  { 0x2126, 0x2126 }, { 0x212B, 0x212B }, { 0x2153, 0x2154 },
  { 0x215B, 0x215E }, { 0x2160, 0x216B }, { 0x2170, 0x2179 },
  { 0x2190, 0x2199 }, { 0x21B8, 0x21B9 }, { 0x21D2, 0x21D2 },
  { 0x21D4, 0x21D4 }, { 0x21E7, 0x21E7 }, { 0x2200, 0x2200 },
  { 0x2202, 0x2203 }, { 0x2207, 0x2208 }, { 0x220B, 0x220B },
  { 0x220F, 0x220F }, { 0x2211, 0x2211 }, { 0x2215, 0x2215 },
  { 0x221A, 0x221A }, { 0x221D, 0x2220 }, { 0x2223, 0x2223 },
  { 0x2225, 0x2225 }, { 0x2227, 0x222C }, { 0x222E, 0x222E },
  { 0x2234, 0x2237 }, { 0x223C, 0x223D }, { 0x2248, 0x2248 },
  { 0x224C, 0x224C }, { 0x2252, 0x2252 }, { 0x2260, 0x2261 },
  { 0x2264, 0x2267 }, { 0x226A, 0x226B }, { 0x226E, 0x226F },
  { 0x2282, 0x2283 }, { 0x2286, 0x2287 }, { 0x2295, 0x2295 },
  { 0x2299, 0x2299 }, { 0x22A5, 0x22A5 }, { 0x22BF, 0x22BF },
  { 0x2312, 0x2312 }, { 0x2460, 0x24E9 }, { 0x24EB, 0x254B },
  { 0x2550, 0x2573 }, { 0x2580, 0x258F }, { 0x2592, 0x2595 },
  { 0x25A0, 0x25A1 }, { 0x25A3, 0x25A9 }, { 0x25B2, 0x25B3 },
  { 0x25B6, 0x25B7 }, { 0x25BC, 0x25BD }, { 0x25C0, 0x25C1 },
  { 0x25C6, 0x25C8 }, { 0x25CB, 0x25CB }, { 0x25CE, 0x25D1 },
  { 0x25E2, 0x25E5 }, { 0x25EF, 0x25EF }, { 0x2605, 0x2606 },
  { 0x2609, 0x2609 }, { 0x260E, 0x260F }, { 0x2614, 0x2615 },
  { 0x261C, 0x261C }, { 0x261E, 0x261E }, { 0x2640, 0x2640 },
  { 0x2642, 0x2642 }, { 0x2660, 0x2661 }, { 0x2663, 0x2665 },
  { 0x2667, 0x266A }, { 0x266C, 0x266D }, { 0x266F, 0x266F },
  { 0x273D, 0x273D }, { 0x2776, 0x277F }, { 0xE000, 0xF8FF },
  { 0xFFFD, 0xFFFD }, { 0xF0000, 0xFFFFD }, { 0x100000, 0x10FFFD }
};

bool is_east_asian_ambiguous(char32_t ucs)
{
  return !!bisearch(ucs, ambiguous, _countof(ambiguous) - 1);
}

/*
 * The following functions are the same as mk_wcwidth() and
 * mk_wcswidth(), except that spacing characters in the East Asian
 * Ambiguous (A) category as defined in Unicode Technical Report #11
 * have a column width of 2. This variant might be useful for users of
 * CJK legacy encodings who want to migrate to UCS without changing
 * the traditional terminal character-width behaviour. It is not
 * otherwise recommended for general use.
 */
static int32 mk_wcwidth_cjk(char32_t ucs)
{
  /* binary search in table of ambiguous width chars in CJK codepages */
  if (bisearch(ucs, ambiguous, _countof(ambiguous) - 1))
    return resolve_ambiguous_wcwidth(ucs);

  return mk_wcwidth(ucs);
}

static int32 mk_wcwidth_cjk_ucs2(char32_t ucs)
{
  /* binary search in table of ambiguous width chars in CJK codepages */
  if (bisearch(ucs, ambiguous, _countof(ambiguous) - 1))
    return resolve_ambiguous_wcwidth(ucs);

  return mk_wcwidth_ucs2(ucs);
}



//------------------------------------------------------------------------------
typedef int32 wcwidth_t (char32_t);
wcwidth_t *wcwidth = mk_wcwidth;

#if 0
typedef int32 wcswidth_t (const char32_t*, size_t);
wcswidth_t *wcswidth = mk_wcswidth;
#endif

/*
 * This tests whether the input codepoint is a recognized emoji variant
 * selector.
 */
bool is_variant_selector(char32_t ucs)
{
    assert(g_color_emoji);
    return (ucs == 0xfe0f ||                            // color variant
            ucs >= 0x1f3fb && ucs <= 0x1f3ff);          // skin tone
}

/*
 * Windows Terminal renders some codepoints as half-width unless followed by
 * codepoints that make them fully-qualified.  This is for consistency with
 * behavior of the first few terminals that supported color emoji.
 * https://github.com/microsoft/terminal/issues/17342#issuecomment-2199942912
 */
bool is_possible_unqualified_half_width(char32_t ucs)
{
    assert(g_color_emoji);
    return !!bisearch(ucs, possible_unqualified_half_width, _countof(possible_unqualified_half_width) - 1);
}

bool is_combining(char32_t ucs)
{
    return !!bisearch(ucs, combining, _countof(combining) - 1);
}

/*
 * This tests whether the input codepoint is recognized as an emoji.
 */
bool is_emoji(char32_t ucs)
{
    assert(g_color_emoji);
    return !!bisearch(ucs, emojis, _countof(emojis) - 1);
}

static int32 resolve_ambiguous_wcwidth(char32_t ucs)
{
    return 2;
}

int32 is_CJK_codepage(UINT cp)
{
    return (cp == 932 || cp == 936 || cp == 949 || cp == 950);
}

void reset_wcwidths()
{
    int32 use_cjk = true;

    detect_ucs2_limitation();

    static UINT s_cp = 0; // Static so that it's visible in heap dumps.
    s_cp = GetConsoleOutputCP();
    use_cjk = is_CJK_codepage(s_cp);

    if (use_cjk)
    {
        wcwidth = s_only_ucs2 ? mk_wcwidth_cjk_ucs2 : mk_wcwidth_cjk;
    }
    else
    {
        wcwidth = s_only_ucs2 ? mk_wcwidth_ucs2 : mk_wcwidth;
    }
}

// vim: ts=2 expandtab sw=2
