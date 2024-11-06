local to = ".build/"..(_ACTION or "nullaction")

local bit32 = require("numberlua").bit32

--------------------------------------------------------------------------------
workspace("wcwidth-verifier")
    configurations({"debug"})
    platforms({"x32", "x64"})
    location(to)

    characterset("MBCS")
    flags("NoManifest")
    flags("fatalwarnings")
    staticruntime("on")
    symbols("on")

    filter "debug"
        targetdir(to.."/bin/debug")
        objdir(to.."/obj/")
        optimize("off")
        defines("DEBUG")
        defines("_DEBUG")

    filter "action:vs*"
        defines("_HAS_EXCEPTIONS=0")
        defines("_CRT_SECURE_NO_WARNINGS")
        defines("_CRT_NONSTDC_NO_WARNINGS")

    project("exe")
        targetname("wcwv")
        kind("consoleapp")
        language("c++")
        --flags("OmitDefaultLibrary")
        includedirs(".")
        files("str_iter.cpp")
        files("wcwidth.cpp")
        files("wcwidth_iter.cpp")
        files("main.cpp")

--------------------------------------------------------------------------------
local function escape_cpp(text)
    return text:gsub("([?\"\\])", "\\%1")
end

--------------------------------------------------------------------------------
local function to_symbol_cpp(text)
    return text:gsub("[^A-Za-z0-9_]", "_")
end

--------------------------------------------------------------------------------
local function utf32to8(c)
    if type(c) ~= "number" then
        error(string.format("arg #1 unexpected type %s; expected number", type(c)))
    elseif c < 0 then
        error(string.format("arg #1 cannot be negative"))
    elseif c <= 0x007f then
        return string.format("\\x%02x", c)
    elseif c <= 0x07ff then
        local b2 = 0x80 + bit32.band(c, 0x3f)
        local b1 = 0xc0 + bit32.band(bit32.rshift(c, 6), 0x1f)
        return string.format("\\x%02x\\x%02x", b1, b2)
    elseif c <= 0xffff then
        local b3 = 0x80 + bit32.band(c, 0x3f)
        local b2 = 0x80 + bit32.band(bit32.rshift(c, 6), 0x3f)
        local b1 = 0xe0 + bit32.band(bit32.rshift(c, 12), 0x0f)
        return string.format("\\x%02x\\x%02x\\x%02x", b1, b2, b3)
    elseif c <= 0x10ffff then
        local b4 = 0x80 + bit32.band(c, 0x3f)
        local b3 = 0x80 + bit32.band(bit32.rshift(c, 6), 0x3f)
        local b2 = 0x80 + bit32.band(bit32.rshift(c, 12), 0x3f)
        local b1 = 0xf0 + bit32.band(bit32.rshift(c, 18), 0x07)
        return string.format("\\x%02x\\x%02x\\x%02x\\x%02x", b1, b2, b3, b4)
    else
        error(string.format("arg #1 value 0x%x exceeds 0x10ffff", c))
    end
end

--------------------------------------------------------------------------------
local function load_indexed_emoji_table(file)
    -- Collect the emoji characters.
    --
    -- This uses a simplistic approach of taking the first codepoint from each
    -- line in the input file.
    local indexed = {}
    for line in file:lines() do
        local x = line:match("^([0-9A-Fa-f]+) ")
        if x then
            local d = tonumber(x, 16)
            if d then
                local t = indexed[d]
                if not t then
                    t = {}
                    indexed[d] = t
                end
                local sequence = line:match("^([0-9A-Fa-f ]+)"):gsub("%w+$", "")
                table.insert(t, sequence)
            end
        end
    end
    return indexed
end

--------------------------------------------------------------------------------
local function output_character_ranges(out, tag, indexed, filtered)
    -- Declaration.
    out:write("\nstatic const struct interval " .. tag .. "[] = {\n\n")

    -- Build sorted array of characters.
    local chars = {}
    for d, _ in pairs(indexed) do
        if not (filtered and filtered[d]) then
            table.insert(chars, d)
        end
    end
    table.sort(chars)

    -- Optimize the set of characters into ranges.
    local count_ranges = 0
    local first
    local last
    for _, d in ipairs(chars) do
        if last and last + 1 ~= d then
            count_ranges = count_ranges + 1
            out:write(string.format("{ 0x%X, 0x%X },\n", first, last))
            first = nil
        end
        if not first then
            first = d
        end
        last = d
    end
    if first then
        count_ranges = count_ranges + 1
        out:write(string.format("{ 0x%X, 0x%X },\n", first, last))
    end

    out:write("\n};\n")

    return chars, count_ranges
end

--------------------------------------------------------------------------------
local function output_emoji_forms(out, tag, indexed, possible_unqualified_half_width, filtered)
    local forms = {}

    forms[0xfe0f] = { "FE0F" }
    forms[0x1f3fb] = { "1F3FB" }
    forms[0x1f3fc] = { "1F3FC" }
    forms[0x1f3fd] = { "1F3FD" }
    forms[0x1f3fe] = { "1F3FE" }
    forms[0x1f3ff] = { "1F3FF" }

    for d, v in pairs(indexed) do
        if not filtered[d] or possible_unqualified_half_width[d] then
            forms[d] = v
        end
    end

    forms[0x1f3f4] = nil    -- FUTURE: Windows Terminal doesn't support the E5.0 flag "subdivision-flag" sequences.

    -- Declaration.
    out:write("\nstatic const emoji_form_sequences " .. tag .. "[] = {\n\n")

    for ucs, t in spairs(forms) do
        local sequences = ""
        for _, list in ipairs(t) do
            local seq = {}
            for x in string.gmatch(list, "[0-9A-Fa-f]+") do
                table.insert(seq, utf32to8(tonumber(x, 16)))
            end
            sequences = sequences .. string.format("%s\\x00", table.concat(seq))
        end
        out:write(string.format("{ 0x%X, \"%s\" },\n", ucs, sequences))
    end

    out:write("\n};\n")
end

--------------------------------------------------------------------------------
local function do_emojis()
    local out = "emoji-test.i"

    print("\n"..out)

    local file = io.open("unicode/emoji-test.txt", "r")
    local filter = io.open("unicode/emoji-filter.txt", "r")
    local fe0f = io.open("unicode/emoji-fe0f.txt", "r")
    out = io.open(out, "w")

    local header = {
        "// Generated from emoji-test.txt by 'premake5 tables'.",
    }

    for _,line in ipairs(header) do
        out:write(line)
        out:write("\n")
    end

    -- Collect the emoji characters.
    local indexed = load_indexed_emoji_table(file)
    local filtered = load_indexed_emoji_table(filter)
    local possible_unqualified_half_width = load_indexed_emoji_table(fe0f)
    file:close()
    filter:close()
    fe0f:close()

    -- Output ranges of double-width emoji characters.
    local emojis, count_ranges = output_character_ranges(out, "emojis", indexed, filtered)

    -- Output ranges of emoji characters which may be half-width if unqualified.
    local half_width = output_character_ranges(out, "possible_unqualified_half_width", possible_unqualified_half_width, nil)

    output_emoji_forms(out, "emoji_forms", indexed, possible_unqualified_half_width, filtered)

    out:close()

    print("   " .. #emojis .. " emojis; " .. count_ranges .. " ranges")
    print("   " .. #half_width .. " possible unqualified half width emojis")
end

--------------------------------------------------------------------------------
local function do_assigned()
    local out = "assigned-codepoints.i"

    print("\n"..out)

    local data = io.open("unicode/unicodedata.txt", "r")
    out = io.open(out, "w")

    local header = {
        "// Generated from UnicodeData.txt by 'premake5 tables'.",
        "",
        "struct codepoint {",
        "    char32_t ucs;",
        "    const char* desc;",
        "};",
        "",
        "struct codepoint_range {",
        "    char32_t first;",
        "    char32_t last;",
        "    const char* desc;",
        "};",
        "",
    }

    for _,line in ipairs(header) do
        out:write(line)
        out:write("\n")
    end

    out:write("static const struct codepoint c_assigned[] = {\n\n")

    local lineno = 0
    local assigned = 0
    local area, first
    local first_last_ranges = {}
    for line in data:lines() do
        lineno = lineno + 1
        local codepoint,description = line:match("^(%x+);([^;]*);")
        if not codepoint then
            error("Failed to parse code point in line "..tostring(lineno)..".")
        end
        local first_tag = description:match("^<(.*), First>$")
        if first_tag then
            area = first_tag
            first = tonumber(codepoint, 16)
        elseif area then
            if not description:find("Last>$") then
                error("Unexpected First/Last range at "..tostring(lineno)..".")
            end
            if not area:find("Surrogate") then
                local last = tonumber(codepoint, 16)
                table.insert(first_last_ranges, {first,last,area})
            end
            area = nil
        elseif description ~= "<control>" then
            out:write(string.format("{0x%s,\"%s\"},\n", codepoint, escape_cpp(description)))
            assigned = assigned + 1
        end
    end

    out:write("\n};\n\n")

    for i,r in ipairs(first_last_ranges) do
        out:write(string.format("static const codepoint_range c_%s = {0x%X,0x%X,\"%s\"};\n", to_symbol_cpp(r[3]), r[1], r[2], r[3]))
    end

    out:write("\nstatic const struct codepoint_range c_assigned_areas[] = {\n\n");
    for i,r in ipairs(first_last_ranges) do
        out:write(string.format("{0x%X,0x%X,\"%s\"}, // %s\n", r[1], r[2], escape_cpp(r[3]), r[3]))
    end
    out:write("\n};\n")

    out:close()

    print("   " .. assigned .." assigned codepoints (not including various large First..Last ranges)")
end

--------------------------------------------------------------------------------
local function do_blocks()
    local out = "unicode-blocks.i"

    print("\n"..out)

    local data = io.open("unicode/blocks.txt", "r")
    out = io.open(out, "w")

    local header = {
        "// Generated from Blocks.txt by 'premake5 tables'.",
        "",
        "struct block_range {",
        "    char32_t first;",
        "    char32_t last;",
        "    const char* desc;",
        "};",
        "",
    }

    for _,line in ipairs(header) do
        out:write(line)
        out:write("\n")
    end

    out:write("\nstatic const struct block_range c_blocks[] = {\n\n");

    local lineno = 0
    for line in data:lines() do
        lineno = lineno + 1
        local first,last,description = line:match("^(%x+)..(%x+); *([^;]*) *$")
        if first and not description:find("Surrogates") then
            local as_num = tonumber(first, 16)
            if as_num < 0xE0000 then
                if as_num == 0 then
                    first = 0x20
                end
                out:write(string.format("{0x%s,0x%s,\"%s\"},\n", first, last, escape_cpp(description)))
            end
        end
    end

    out:write("{}\n\n};\n")

    out:close()
end

--------------------------------------------------------------------------------
newaction {
    trigger = "tables",
    description = "Update Unicode tables for wcwidth-verifier",
    execute = function ()
        do_emojis()
        do_assigned()
        do_blocks()
    end
}
