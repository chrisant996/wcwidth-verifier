local to = ".build/"..(_ACTION or "nullaction")

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

