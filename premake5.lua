local to = ".build/"..(_ACTION or "nullaction")

--------------------------------------------------------------------------------
workspace("wcwidth-verifier")
    configurations({"debug"})
    platforms({"x64"})
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
    }

    for _,line in ipairs(header) do
        out:write(line)
        out:write("\n")
    end

    out:write("static const struct codepoint c_assigned[] = {\n\n")

    local lineno = 0
    local assigned = 0
    for line in data:lines() do
        lineno = lineno + 1
        local codepoint,description = line:match("^(%x+);([^;]*);")
        if not codepoint then
            error("Failed to parse code point in line "..tostring(lineno)..".")
        end
-- TODO: Some of the First..Last ranges should be treated as assigned codepoints (but e.g. not Surrogate Pair ranges).
        if description:find("First>$") or description:find("Last>$") then
            out:write(string.format("// 0x%s - %s\n", codepoint, description))
        else
            out:write(string.format("{0x%s,\"%s\"},\n", codepoint, escape_cpp(description)))
            assigned = assigned + 1
        end
    end

    out:write("\n};\n")
    out:close()

    print("   " .. assigned .." assigned codepoints (not including various large First..Last ranges)")
end

--------------------------------------------------------------------------------
newaction {
    trigger = "tables",
    description = "Update Unicode tables for wcwidth-verifier",
    execute = function ()
        do_emojis()
        do_assigned()
    end
}
