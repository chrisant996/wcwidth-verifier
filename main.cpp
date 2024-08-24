#include <windows.h>
#include <stdio.h>
#include <vector>
#include "wcwidth.h"

static HANDLE s_hout = GetStdHandle(STD_OUTPUT_HANDLE);
static WCHAR s_placeholder = ' ';
static bool s_always_clear = true;
static bool s_skip_combining = false;
static bool s_skip_color_emoji = false;
static bool s_skip_eaa = false;

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

static int VerifyWidth(char32_t ucs, const int expected_width)
{
    utf16fromutf32 s(ucs);

    CONSOLE_SCREEN_BUFFER_INFO csbiBefore;
    if (!GetConsoleScreenBufferInfo(s_hout, &csbiBefore))
        return -1;
    if (csbiBefore.dwCursorPosition.X != 0)
        return -1;

    DWORD written = 0;
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

    if (!WriteConsoleW(s_hout, &s_placeholder, 1, &written, nullptr))
        return -1;
    if (written != 1)
        return -1;

    CONSOLE_SCREEN_BUFFER_INFO csbiAfter2;
    if (!GetConsoleScreenBufferInfo(s_hout, &csbiAfter2))
        return -1;
    if (csbiAfter2.dwCursorPosition.X != width + 1)
        return -1;

    const int ok = (width == wcwidth(ucs));

    if (ok || s_always_clear)
    {
        SetConsoleCursorPosition(s_hout, csbiBefore.dwCursorPosition);
        WriteConsoleW(s_hout, L"        ", 8, &written, nullptr);
        SetConsoleCursorPosition(s_hout, csbiBefore.dwCursorPosition);
    }
    else
    {
        printf("%s   0x%X, width %u, expected %u", (s_placeholder == ' ') ? "" : " ", (unsigned int)ucs, width, expected_width);
        const char* desc = is_assigned(ucs);
        if (desc && *desc)
            printf("    %s", desc);
        puts("");
    }

    return ok;
}

static bool IsSkip(char32_t c)
{
    if (s_skip_combining && is_combining(c))
        return true;
    if (s_skip_color_emoji && is_color_emoji(c))
        return true;
    if (s_skip_eaa && is_east_asian_ambiguous(c))
        return true;
    return false;
}

struct interval
{
    char32_t first;
    char32_t last;
};

static bool ParseCodepoint(const char* arg, interval& range, bool end_range=false)
{
    char* end;
    int radix = 10;

    if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X'))
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

int main(int argc, char** argv)
{
    --argc, ++argv;

    DWORD mode;
    if (!GetConsoleMode(s_hout, &mode))
    {
        fputs("This test tool is not compatible with redirected output.\n", stderr);
        return 1;
    }

    static const interval c_ranges[] =
    {
        { 0x20, 0x7e },
        { 0xa0, 0xD7FF },
        { 0xE000, 0xFFFF },
        { 0x10000, 0x1FFFF },
        {}
    };

    std::vector<interval> manual_ranges;

    goto first_arg;
next_arg:
    --argc, ++argv;
first_arg:
    if (argc)
    {
        if (argv[0][0] == '-' && argv[0][1] == '-')
        {
            if (strcmp(argv[0], "--always-clear") == 0)
            {
                s_always_clear = true;
                goto next_arg;
            }
            else if (strcmp(argv[0], "--no-clear-failed") == 0)
            {
                s_always_clear = false;
                goto next_arg;
            }
            else if (strcmp(argv[0], "--color-emoji") == 0)
            {
                g_color_emoji = true;
                goto next_arg;
            }
            else if (strcmp(argv[0], "--no-color-emoji") == 0)
            {
                g_color_emoji = false;
                goto next_arg;
            }
            else if (strcmp(argv[0], "--full-width") == 0)
            {
                g_full_width_available = true;
                goto next_arg;
            }
            else if (strcmp(argv[0], "--no-full-width") == 0)
            {
                g_full_width_available = false;
                goto next_arg;
            }
            else if (strcmp(argv[0], "--skip-combining") == 0)
            {
                s_skip_combining = true;
                goto next_arg;
            }
            else if (strcmp(argv[0], "--skip-color-emoji") == 0)
            {
                s_skip_color_emoji = true;
                goto next_arg;
            }
            else if (strcmp(argv[0], "--skip-eaa") == 0)
            {
                s_skip_eaa = true;
                goto next_arg;
            }
            else
            {
                static const char usage[] =
                "Usage:  wcwv [flags] [codepoint [...]]\n"
                "n"
                "  --help                Display this help.\n"
                "  --always-clear        Always clear codepoints after printing them (default).\n"
                "  --no-clear-failed     Leave failed codepoints on the screen.\n"
                "  --color-emoji         Assume the terminal supports color emoji (default).\n"
                "  --no-color-emoji      Assume the terminal does not support color emoji.\n"
                "  --full-width          Assume Full Width characters are full width (default).\n"
                "  --no-full-width       Assume Full Width characters are half width.\n"
                "  --skip-combining      Skip testing combining marks.\n"
                "  --skip-color-emoji    Skip testing color emoji.\n"
                "  --skip-eaa            Skip testing East Asian Ambiguous characters.\n"
                "\n"
                "  Each \"codepoint\" can be a single codepoint in decimal or hexadecimal (0x...),\n"
                "  or a range such as \"0x300..0x31F\"."
                "\n"
                "Examples:\n"
                "\n"
                "  wcwv                          Run the full tests.\n"
                "  wcwv --no-clear-failed        Run the full tests, and leave failed codepoints\n"
                "                                visible after failing.\n"
                "  wcwv 0x300                    Run the test on codepoint 0x300.\n"
                "  wcwv 0x300 0x301              Run the test on codepoints 0x300 and 0x301.\n"
                "  wcwv 0x300..0x3FF             Run the test on codepoints 0x300 through 0x3FF.\n"
                "  wcwv 0x20..0x2F 0x40..0x5F    Run the test on codepoints 0x20 through 0x2F\n"
                "                                and 0x40 through 0x5F.\n"
                ;
                printf("%s", usage);
                return 0;
            }
        }
        else if (argv[0][0] >= '0' && argv[0][0] <= '9')
        {
            interval interval;
            if (!ParseCodepoint(argv[0], interval))
            {
                fprintf(stderr, "Unable to parse '%s' as a codepoint or range of codepoints.\n", argv[0]);
                return 1;
            }
            manual_ranges.emplace_back(interval);
            goto next_arg;
        }
        else
        {
            fprintf(stderr, "Unrecognized codepoint '%s'.\n", argv[0]);
            return 1;
        }
    }

    if (!manual_ranges.empty())
        manual_ranges.push_back({0, 0});

    const interval* const ranges = manual_ranges.empty() ? c_ranges : &manual_ranges.front();
    const DWORD began = GetTickCount();

    unsigned int tested = 0;
    unsigned int failed = 0;

    for (const interval* range = ranges; range->first; ++range)
    {
        char32_t prev = 0;
        char32_t first_failure = 0;
        char32_t last_failure = 0;

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(s_hout, &csbi);
        SetConsoleTextAttribute(s_hout, csbi.wAttributes | 0xF);

        if (range->first == range->last)
            printf("CODEPOINT 0x%X", (unsigned int)range->first);
        else
            printf("RANGE 0x%X .. 0x%X", (unsigned int)range->first, (unsigned int)range->last);

        SetConsoleTextAttribute(s_hout, csbi.wAttributes);
        puts("");

        for (char32_t c = range->first; c <= range->last; ++c)
        {
            if (IsSkip(c))
                continue;

            if (!is_assigned(c))
                continue;

            if (!prev || (prev >> 12) != (c >> 12))
            {
                printf("0x%X ...\n", (unsigned int)c);
                prev = c;
            }

            const int verified = VerifyWidth(c, wcwidth(c));
            if (verified < 0)
            {
                fprintf(stderr, "INTERNAL FAILURE:  unable to verify 0x%X.\n", (unsigned int)c);
                return 1;
            }

            if (!verified)
            {
                ++failed;
                if (!first_failure)
                    first_failure = c;
                last_failure = c;
            }
            else
            {
                if (first_failure)
                {
                    fprintf(stderr, "FAILED:  0x%X..0x%X do not match the expected width.\n", (unsigned int)first_failure, (unsigned int)last_failure);
                    first_failure = 0;
                    last_failure = 0;
                }
            }

            ++tested;
        }

        if (first_failure)
        {
            fprintf(stderr, "FAILED:  0x%X..0x%X do not match the expected width.\n", (unsigned int)first_failure, (unsigned int)last_failure);
            first_failure = 0;
            last_failure = 0;
        }
    }

    const DWORD elapsed = GetTickCount() - began;
    printf("\nTested %u characters in %u.%03u seconds; %u failed.\n", tested, elapsed / 1000, elapsed % 1000, failed);

    return !!failed;
}
