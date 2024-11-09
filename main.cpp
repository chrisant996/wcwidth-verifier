#include "main.h"
#include "wcwidth.h"

#include <locale.h>

static HANDLE s_hout = GetStdHandle(STD_OUTPUT_HANDLE);
static char32_t s_prefix = '\0';
static char32_t s_suffix = ' ';
static bool s_verbose = false;
static bool s_group_headers = true;
static bool s_show_width = false;
static bool s_force_only_ucs2 = false;
static bool s_decimal = false;

bool g_full_width_available = true;
bool g_color_emoji = false;
bool g_only_ucs2 = false;

#include "unicode-blocks.i"

class utf16fromutf32
{
public:
    utf16fromutf32(char32_t ucs)
    {
        if (ucs >= 0x10000)
        {
            ucs -= 0x10000;
            m_buffer[0] = 0xD800 + (ucs >> 10);
            m_buffer[1] = 0xDC00 + (ucs & 0x03FF);
            m_length = 2;
        }
        else if (ucs > 0)
        {
            m_buffer[0] = LOWORD(ucs);
            m_length = 1;
        }
        else
        {
            m_length = 0;
        }
        m_buffer[m_length] = '\0';
    }

    const WCHAR* c_str() const { return m_buffer; }
    WORD length() const { return m_length; }

private:
    WCHAR m_buffer[3];
    WORD m_length;
};

static int32 VerifyWidth(char32_t ucs)
{
    utf16fromutf32 s(ucs);
    char utf8[64];
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, utf8, _countof(utf8), nullptr, nullptr);
    const uint32 expected_width = wcswidth(utf8, uint32(strlen(utf8)));

    DWORD written = 0;
    CONSOLE_SCREEN_BUFFER_INFO csbiBefore;
    if (!GetConsoleScreenBufferInfo(s_hout, &csbiBefore))
        return -1;
    if (csbiBefore.dwCursorPosition.X != 0)
        return -1;

    if (s_prefix)
    {
        utf16fromutf32 pre(s_prefix);
        if (!WriteConsoleW(s_hout, pre.c_str(), pre.length(), &written, nullptr))
            return -1;
        if (written != pre.length())
            return -1;
        if (!GetConsoleScreenBufferInfo(s_hout, &csbiBefore))
            return -1;
    }

    if (!WriteConsoleW(s_hout, s.c_str(), s.length(), &written, nullptr))
        return -1;
    if (written != s.length())
        return -1;

    CONSOLE_SCREEN_BUFFER_INFO csbiAfter1;
    if (!GetConsoleScreenBufferInfo(s_hout, &csbiAfter1))
        return -1;
    if (csbiAfter1.dwCursorPosition.Y != csbiBefore.dwCursorPosition.Y)
        return -1;

    const SHORT width = csbiAfter1.dwCursorPosition.X - csbiBefore.dwCursorPosition.X;
    if (width < 0 || width > 2)
        return -1;

    bool suffix_effect = false;
    if (s_suffix)
    {
        utf16fromutf32 suf(s_suffix);
        if (!WriteConsoleW(s_hout, suf.c_str(), suf.length(), &written, nullptr))
            return -1;
        if (written != suf.length())
            return -1;

        CONSOLE_SCREEN_BUFFER_INFO csbiAfter2;
        if (!GetConsoleScreenBufferInfo(s_hout, &csbiAfter2))
            return -1;
        suffix_effect = (csbiAfter2.dwCursorPosition.X != csbiBefore.dwCursorPosition.X + width + 1);
    }

    const int32 ok = (width == expected_width) && !suffix_effect;

    if (!s_show_width && (ok || !s_verbose))
    {
        SetConsoleCursorPosition(s_hout, csbiBefore.dwCursorPosition);
        WriteConsoleW(s_hout, L"        ", 8, &written, nullptr);
        SetConsoleCursorPosition(s_hout, csbiBefore.dwCursorPosition);
    }
    else
    {
        printf("%s   %04X, width %u, expected %u", (s_suffix == ' ') ? "" : " ", (uint32)ucs, width, expected_width);
        const char* desc = is_assigned(ucs);
        if (desc && *desc)
            printf("    %s", desc);
        puts("");
        if (suffix_effect)
            printf("        WARNING:  Suffix codepoint affected the width after measurement!\n");
    }

    return ok;
}

static int32 VerifyWidth(const emoji_form_sequence* sequence)
{
    wchar_t s[64];
    MultiByteToWideChar(CP_UTF8, 0, sequence->seq, -1, s, _countof(s));

    DWORD written = 0;
    CONSOLE_SCREEN_BUFFER_INFO csbiBefore;
    if (!GetConsoleScreenBufferInfo(s_hout, &csbiBefore))
        return -1;
    if (csbiBefore.dwCursorPosition.X != 0)
        return -1;

    if (s_prefix)
    {
        utf16fromutf32 pre(s_prefix);
        if (!WriteConsoleW(s_hout, pre.c_str(), pre.length(), &written, nullptr))
            return -1;
        if (written != pre.length())
            return -1;
        if (!GetConsoleScreenBufferInfo(s_hout, &csbiBefore))
            return -1;
    }

    const uint32 s_len = uint32(wcslen(s));
    if (!WriteConsoleW(s_hout, s, s_len, &written, nullptr))
        return -1;
    if (written != s_len)
        return -1;

    CONSOLE_SCREEN_BUFFER_INFO csbiAfter1;
    if (!GetConsoleScreenBufferInfo(s_hout, &csbiAfter1))
        return -1;
    if (csbiAfter1.dwCursorPosition.Y != csbiBefore.dwCursorPosition.Y)
        return -1;

    const SHORT width = csbiAfter1.dwCursorPosition.X - csbiBefore.dwCursorPosition.X;

    bool suffix_effect = false;
    if (s_suffix)
    {
        utf16fromutf32 suf(s_suffix);
        if (!WriteConsoleW(s_hout, suf.c_str(), suf.length(), &written, nullptr))
            return -1;
        if (written != suf.length())
            return -1;

        CONSOLE_SCREEN_BUFFER_INFO csbiAfter2;
        if (!GetConsoleScreenBufferInfo(s_hout, &csbiAfter2))
            return -1;
        suffix_effect = (csbiAfter2.dwCursorPosition.X != csbiBefore.dwCursorPosition.X + width + 1);
    }

    const int32 expected_width = wcswidth(sequence->seq, uint32(strlen(sequence->seq)));
    const int32 ok = (width == expected_width) && !suffix_effect;

    if (!s_show_width && (ok || !s_verbose))
    {
        SetConsoleCursorPosition(s_hout, csbiBefore.dwCursorPosition);
        WriteConsoleW(s_hout, L"        ", 8, &written, nullptr);
        SetConsoleCursorPosition(s_hout, csbiBefore.dwCursorPosition);
    }
    else
    {
        printf("%s   ", (s_suffix == ' ') ? "" : " ");
        str_iter iter(sequence->seq);
        while (iter.more())
        {
            if (iter.get_pointer() > sequence->seq)
                printf(" ");
            const int32 c = iter.next();
            printf("%04X", c);
        }
        printf(", width %u, expected %u", width, expected_width);
        const char* desc = sequence->desc;
        if (desc && *desc)
            printf("    %s", desc);
        puts("");
        if (suffix_effect)
            printf("        WARNING:  Suffix codepoint affected the width after measurement!\n");
    }

    return ok;
}

static bool s_skip_all = false;
static bool s_skip_combining = false;
static bool s_skip_emoji = false;
static bool s_skip_eaa = false;
static bool s_skip_ideographs = true;
static bool s_skip_kana = true;

static void SetSkipAll(bool skip)
{
    s_skip_all = skip;
    s_skip_combining = skip;
    s_skip_emoji = skip;
    s_skip_eaa = skip;
    s_skip_ideographs = skip;
    s_skip_kana = skip;
}

static bool IsSkip(char32_t c)
{
    if (s_skip_all)
    {
        if (!s_skip_combining && is_combining(c))
            return false;
        if (!s_skip_emoji && is_emoji(c))
            return false;
        if (!s_skip_eaa && is_east_asian_ambiguous(c))
            return false;
        if (!s_skip_ideographs && is_ideograph(c))
            return false;
        if (!s_skip_kana && is_kana(c))
            return false;
        return true;
    }
    else
    {
        if (s_skip_combining && is_combining(c))
            return true;
        if (s_skip_emoji && is_emoji(c))
            return true;
        if (s_skip_eaa && is_east_asian_ambiguous(c))
            return true;
        if (s_skip_ideographs && is_ideograph(c))
            return true;
        if (s_skip_kana && is_kana(c))
            return true;
        return false;
    }
}

static bool IsSequenceSupported(const char* seq)
{
    if (g_only_ucs2)
    {
        // Conhost on Win 8.1 and Win 10 behave oddly for many emoji
        // sequences.  It isn't a goal at this time to try to accurately
        // predict the odd behaviors.
        if (strlen(seq) > 6)
            return false;
        if (strstr(seq, "\xef\xb8\x8f"))
            return false;
    }
    return true;
}

struct interval
{
    char32_t first;
    char32_t last;
};

static bool ParseCodepoint(const char* arg, interval& range, bool end_range=false)
{
    char* end;
    int32 radix = s_decimal ? 10 : 16;

    if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X'))
    {
        radix = 16;
        arg += 2;
    }
    else if ((arg[0] == 'U' || arg[0] == 'u') && arg[1] == '+')
    {
        radix = 16;
        arg += 2;
    }

    const char32_t x = strtoul(arg, &end, radix);
    if (!x)
        return false;

    if (end_range)
    {
        range.last = x;
        if (*end && *end != ' ')
            return false;
    }
    else
    {
        range.first = range.last = x;
        if (end[0] == '.' && end[1] == '.')
            return ParseCodepoint(end + 2, range, true);
        if (*end && *end != ' ')
            return false;
    }

    return true;
}

enum class option_type { boolean, codepoint };

struct option_definition
{
    const char*     name;
    option_type     type;
    void*           value;
};

static bool parse_options(int32& argc, char**& argv, const option_definition* options)
{
    int32 keep_argc = 0;

    for (int32 i = 0; i < argc; ++i)
    {
        if (argv[i][0] != '-' || argv[i][1] != '-')
        {
            argv[keep_argc++] = argv[i];
            continue;
        }

        bool found = false;
        for (const option_definition* o = options; o->name; ++o)
        {
            const char* arg = argv[i] + 2;
            const bool no = (o->type == option_type::boolean && strncmp(arg, "no-", 3) == 0);
            if (no)
                arg += 3;
            if (strcmp(arg, o->name) == 0)
            {
                found = true;
                switch (o->type)
                {
                case option_type::boolean:
                    if (strcmp(o->name, "skip-all") == 0)
                        SetSkipAll(!no);
                    else
                        *static_cast<bool*>(o->value) = !no;
                    break;
                case option_type::codepoint:
                    {
                        interval interval;
                        if (i + 1 >= argc)
                        {
                            fprintf(stderr, "Missing argument for %s.\n", argv[i]);
                            exit(1);
                            return false;
                        }
                        ++i;
                        if (!ParseCodepoint(argv[i], interval) || interval.first != interval.last)
                        {
                            fprintf(stderr, "Unable to parse '%s' as a codepoint.\n", argv[i]);
                            exit(1);
                            return false;
                        }
                        *static_cast<char32_t*>(o->value) = interval.first;
                    }
                    break;
                default:
                    fprintf(stderr, "Unknown option type %d.\n", o->type);
                    exit(1);
                    return false;
                }
                break;
            }
        }

        if (!found)
        {
            // Unrecognized option; return false to indicate help is requested.
            return false;
        }
    }

    argc = keep_argc;
    return true;
}

static const option_definition c_options[] =
{
    { "verbose",                option_type::boolean,     &s_verbose },
    { "prefix",                 option_type::codepoint,   &s_prefix },
    { "suffix",                 option_type::codepoint,   &s_suffix },
    { "decimal",                option_type::boolean,     &s_decimal },
    { "color-emoji",            option_type::boolean,     &g_color_emoji },
    { "full-width",             option_type::boolean,     &g_full_width_available },
    { "only-ucs2",              option_type::boolean,     &s_force_only_ucs2 },
    { "group-headers",          option_type::boolean,     &s_group_headers },
    { "skip-combining",         option_type::boolean,     &s_skip_combining },
    { "skip-emoji",             option_type::boolean,     &s_skip_emoji },
    { "skip-eaa",               option_type::boolean,     &s_skip_eaa },
    { "skip-ideographs",        option_type::boolean,     &s_skip_ideographs },
    { "skip-kana",              option_type::boolean,     &s_skip_kana },
    { "skip-all",               option_type::boolean,     &s_skip_all },
    { "show-width",             option_type::boolean,     &s_show_width },
    {}
};

int main(int argc, char** argv)
{
    --argc, ++argv;

    DWORD mode;
    if (!GetConsoleMode(s_hout, &mode))
    {
        fputs("This test tool is not compatible with redirected output.\n", stderr);
        return 1;
    }

    if (!parse_options(argc, argv, c_options))
    {
        static const char usage[] =
        "Usage:  wcwv [flags] [codepoint [...]]\n"
        "\n"
        "  Each \"codepoint\" can be a single value, or a range of values denoted by two\n"
        "  values separated by '..' (such as '0x300..0x31F').  By default, values are\n"
        "  assumed to be in hexadecimal unless --decimal is used.  The '0x' or 'U+'\n"
        "  prefixes specify hexadecimal even when --decimal is used.\n"
        "\n"
        "Options:\n"
        "  --help                Display this help.\n"
        "  --prefix codepoint    Set codepoint for prefix character.\n"
        "  --suffix codepoint    Set codepoint for suffix character (default is U+20,\n"
        "                        which is the space character).\n"
        "\n"
        "  NOTE:  the --prefix and --suffix options are experimental, and can be used to\n"
        "  help manually analyze how combining marks affect grapheme widths.\n"
        "\n"
        "On/off options:\n"
        "  --verbose             Verbose output; don't erase failed codepoints.\n"
        "  --color-emoji         Assume the terminal supports color emoji (default).\n"
        "  --decimal             Use decimal for input numbers (default is hexadecimal).\n"
        "  --full-width          Assume Full Width characters are full width (default).\n"
        "  --only-ucs2           Assume only UCS2 support.\n"
        "  --group-headers       Shows names of groups of codepoints (default).\n"
        "  --show-width          Shows expected and actual width for each character.\n"
        "  --skip-combining      Skip testing combining marks.\n"
        "  --skip-emoji          Skip testing emojis.\n"
        "  --skip-eaa            Skip testing East Asian Ambiguous characters.\n"
        "  --skip-ideographs     Skip testing ideograph ranges (default).\n"
        "  --skip-kana           Skip testing various kana ranges (default).\n"
        "  --skip-all            Skip testing all ranges (use --no-skip-whatever to add\n"
        "                        back specific ranges).\n"
        "\n"
        "  NOTE:  On/off options can be enabled by --name or disabled by --no-name.\n"
        "  NOTE:  Windows 8.1 and lower seem to behave like --no-full-width.\n"
        "\n"
        "Examples:\n"
        "\n"
        "  wcwv                          Run the full tests.\n"
        "  wcwv --no-clear-failed        Run the full tests, and leave failed codepoints\n"
        "                                visible after failing.\n"
        "  wcwv 300                      Run the test on codepoint U+300.\n"
        "  wcwv 300 301                  Run the test on codepoints U+300 and U+301.\n"
        "  wcwv 300..3FF                 Run the test on codepoints U+300 through U+3FF.\n"
        "  wcwv 20..2F 40..5F            Run the test on codepoints U+20 through U+2F\n"
        "                                and U+40 through U+5F.\n"
        ;
        printf("%s", usage);
        return 0;
    }

    setlocale(LC_ALL, ".utf8");

    g_color_emoji = !!getenv("WT_SESSION");
    g_only_ucs2 = detect_ucs2_limitation(s_force_only_ucs2 || !g_color_emoji);
    reset_wcwidths();

    std::vector<block_range> manual_ranges;

    if (argc)
    {
        for (int32 i = 0; i < argc; ++i)
        {
            interval interval;
            if (!ParseCodepoint(argv[i], interval))
            {
                fprintf(stderr, "Unable to parse '%s' as a codepoint or range of codepoints.\n", argv[i]);
                return 1;
            }
            manual_ranges.push_back({interval.first, interval.last});
        }

        if (!manual_ranges.empty())
            manual_ranges.push_back({0, 0});
    }

#if 0
    {
        // This performs a test of how Combining Marks are handled by the
        // console API subsystem.

        DWORD written;
        WCHAR buffer[400];
        CONSOLE_SCREEN_BUFFER_INFO csbi, csbi2;

        WriteConsoleW(s_hout, L"e\u0300", 2, &written, nullptr);
        GetConsoleScreenBufferInfo(s_hout, &csbi);
        _swprintf(buffer, L"\x1b[%uG", csbi.dwCursorPosition.X + 1);
        WriteConsoleW(s_hout, buffer, DWORD(wcslen(buffer)), &written, nullptr);
        GetConsoleScreenBufferInfo(s_hout, &csbi2);
        printf("ONE CALL:   cursor X is %d (escape code coordinates%s match)\n", csbi.dwCursorPosition.X, (csbi.dwCursorPosition.X == csbi2.dwCursorPosition.X) ? "" : " DO NOT");

        WriteConsoleW(s_hout, L"e", 1, &written, nullptr);
        WriteConsoleW(s_hout, L"\u0300", 1, &written, nullptr);
        GetConsoleScreenBufferInfo(s_hout, &csbi);
        _swprintf(buffer, L"\x1b[%uG", csbi.dwCursorPosition.X + 1);
        WriteConsoleW(s_hout, buffer, DWORD(wcslen(buffer)), &written, nullptr);
        GetConsoleScreenBufferInfo(s_hout, &csbi2);
        printf("TWO CALLS:  cursor X is %d (escape code coordinates%s match)\n", csbi.dwCursorPosition.X, (csbi.dwCursorPosition.X == csbi2.dwCursorPosition.X) ? "" : " DO NOT");

        for (size_t i = 0; i < _countof(buffer) - 1; ++i)
            buffer[i] = 'x';
        buffer[csbi.dwSize.X] = '\0';
        buffer[0] = 0x0065;
        buffer[1] = 0x0300;
        WriteConsoleW(s_hout, buffer, DWORD(wcslen(buffer)), &written, nullptr);
        GetConsoleScreenBufferInfo(s_hout, &csbi2);
        printf("\nAT END:  cursor X is %d\n", csbi2.dwCursorPosition.X);
    }
#endif

    if (s_verbose)
    {
        printf("only-ucs                = %d\n", g_only_ucs2);
        printf("color-emoji             = %d\n", g_color_emoji);
        printf("full-width              = %d\n", g_full_width_available);
        printf("\n");
    }

    const block_range* const ranges = manual_ranges.empty() ? c_blocks : &manual_ranges.front();
    const DWORD began = GetTickCount();

    uint32 tested = 0;
    uint32 failed = 0;

    for (const block_range* range = ranges; range->first; ++range)
    {
        if (manual_ranges.empty() && range->first >= 0x10000)
            break;

        if (s_skip_ideographs && range->desc && strstr(range->desc, "Ideograph"))
            continue;

        // char32_t prev = 0;

        char32_t first_failure = 0;
        char32_t last_failure = 0;

        if (s_group_headers)
        {
            // CONSOLE_SCREEN_BUFFER_INFO csbi;
            // GetConsoleScreenBufferInfo(s_hout, &csbi);
            // SetConsoleTextAttribute(s_hout, csbi.wAttributes | 0xF);

            if (range->first == range->last)
                printf("CODEPOINT %04X", uint32(range->first));
            else if (range->desc)
                printf("%04X .. %04X -- %s", uint32(range->first), uint32(range->last), range->desc);
            else
                printf("%04X .. %04X", uint32(range->first), uint32(range->last));

            // SetConsoleTextAttribute(s_hout, csbi.wAttributes);
            puts("");
        }

        auto maybe_report_failure_range = [&](){
            if (first_failure)
            {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                HANDLE herr = GetStdHandle(STD_ERROR_HANDLE);
                GetConsoleScreenBufferInfo(herr, &csbi);
                SetConsoleTextAttribute(herr, (csbi.wAttributes & 0xF0) | 0x0C);

                fprintf(stderr, "FAILED:  %04X..%04X do not match the expected width (%u codepoints).", uint32(first_failure), uint32(last_failure), uint32(last_failure + 1 - first_failure));

                SetConsoleTextAttribute(herr, csbi.wAttributes);
                fputs("\n", stderr);

                first_failure = 0;
                last_failure = 0;
            }
        };

        for (char32_t c = range->first; c <= range->last; ++c)
        {
            const bool single_codepoint = (range->first == range->last);
            if (!single_codepoint && IsSkip(c))
            {
                maybe_report_failure_range();
                continue;
            }

            const bool assigned = is_assigned(c);
            if (!assigned && !single_codepoint)
            {
                maybe_report_failure_range();
                continue;
            }

            // if (!prev || (prev >> 12) != (c >> 12))
            // {
            //     if (!prev)
            //         printf("%04X ...\n", uint32(c));
            //     prev = c;
            // }

            if (!assigned && single_codepoint)
                printf("NOTE:  %04X is not an assigned codepoint.\n", uint32(c));

            int32 verified = true;
            const emoji_form_sequence* sequence = get_emoji_form_sequence(c);
            if (sequence)
            {
                for (int32 n = 0; sequence->ucs == c; ++n)
                {
                    if (IsSequenceSupported(sequence->seq))
                    {
                        const int32 v = VerifyWidth(sequence);
                        if (v < 0)
                        {
                            fprintf(stderr, "INTERNAL FAILURE:  unable to verify sequence #%d for %04X.\n", n, uint32(c));
                            return 1;
                        }

                        if (!v)
                        {
                            ++failed;
                            if (!first_failure)
                                first_failure = c;
                            last_failure = c;
                            verified = false;
                        }

                        ++tested;
                    }

                    ++sequence;
                }
            }
            else
            {
                verified = VerifyWidth(c);
                if (verified < 0)
                {
                    fprintf(stderr, "INTERNAL FAILURE:  unable to verify %04X.\n", uint32(c));
                    return 1;
                }

                if (!verified)
                {
                    ++failed;
                    if (!first_failure)
                        first_failure = c;
                    last_failure = c;
                }

                ++tested;
            }

            if (verified)
                maybe_report_failure_range();
        }

        maybe_report_failure_range();
    }

    CONSOLE_SCREEN_BUFFER_INFO csbiAttr;
    GetConsoleScreenBufferInfo(s_hout, &csbiAttr);

    const float ratio = tested ? (float(failed) / float(tested)) : 0;
    const WORD attr = (!failed ? 0x0A :
                       (failed > 200 || ratio > 0.01f) ? 0x0C :
                       0x0E);

    const DWORD elapsed = GetTickCount() - began;
    printf("\nTested %u codepoints in %u.%03u seconds; ", tested, elapsed / 1000, elapsed % 1000);
    SetConsoleTextAttribute(s_hout, (csbiAttr.wAttributes & ~0xF) | attr);
    printf("%u", failed);
    SetConsoleTextAttribute(s_hout, csbiAttr.wAttributes);
    printf(" failed.\n");

    return !!failed;
}
