local to = ".build/"..(_ACTION or "nullaction")

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
        files("wcwidth.cpp")
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
                indexed[d] = true
            end
        end
    end
    return indexed
end

--------------------------------------------------------------------------------
local function output_character_ranges(out, tag, indexed, filtered)

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
    local fully_qualified_double_width = load_indexed_emoji_table(fe0f)
    file:close()
    filter:close()
    fe0f:close()

    -- Output ranges of double-width emoji characters.
    local emojis, count_ranges = output_character_ranges(out, "emojis", indexed, filtered)

    -- Output ranges of ambiguous emoji characters.
    local ignored = {
        [0x0023] = true,
        [0x002A] = true,
        [0x0030] = true,
        [0x0031] = true,
        [0x0032] = true,
        [0x0033] = true,
        [0x0034] = true,
        [0x0035] = true,
        [0x0036] = true,
        [0x0037] = true,
        [0x0038] = true,
        [0x0039] = true,
    }
    local ambiguous = output_character_ranges(out, "ambiguous_emojis", filtered, ignored)

    local double_width = output_character_ranges(out, "fully_qualified_double_width", fully_qualified_double_width, nil)

    out:close()

    print("   " .. #emojis .. " emojis; " .. count_ranges .. " ranges")
    print("   " .. #ambiguous .. " ambiguous emojis")
    print("   " .. #double_width .. " fully qualified double width emojis")
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
