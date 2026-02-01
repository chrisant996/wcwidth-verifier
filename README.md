### Overview

wcwidth-verifier is a tool for testing console column widths of Unicode codepoints.

It compares [wcwidth](https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c) predictions versus what the Windows Console Screen Buffer APIs report as actually used by the Windows Console subsystem.

The motivation was to help [Clink](https://github.com/chrisant996/clink) more accurately predict how terminal programs on Windows will render text.  Hopefully it will help lead to an improved customized version of wcwidth on Windows that more accurately predicts column widths of codepoints on Windows.

### Usage

1. Either build the tool, or download the .zip file from [Releases](https://github.com/chrisant996/wcwidth-verifier/releases) and extract files into a local directory.
2. Run `wcwv.exe --help` in the local directory to display usage help.

This tool is meant for use by programmers familiar with Unicode, wcwidth, graphemes, terminal programs, console APIs, etc.  It isn't useful to other people.

### Building the Tool

The tool uses [Premake](http://premake.github.io) to generate Visual Studio solutions.  Note that Premake >= 5.0.0-beta4 is required.

You might be able to generate MinGW makefiles with `premake5 gmake`, but I won't support MinGW for this tool.

1. `cd` to your clone of the wcwidth-verifier repo.
2. Run <code>premake5.exe <em>toolchain</em></code> (where <em>toolchain</em> is one of Premake's actions - see `premake5.exe --help`)
3. Build scripts will be generated in <code>.build\\<em>toolchain</em></code>. For example `.build\vs2022\wcwidth-verifier.sln`.
4. Call your toolchain of choice (VS, msbuild.exe, etc).

### Updating the Unicode Data Files

To update the Unicode data files:
1. Download new Unicode data files from the addresses in the README.md file in the `unicode\` subdirectory, and save the files into that directory.
2. Run `premake5.exe tables` to generate updated *.i data files.
3. See [Building the Tool](#building-the-tool).

### License

Source code here uses either the MIT license or unless otherwise stated in a file.

The .txt files in the `unicode\` subdirectory were downloaded from the Unicode site; refer to the README.md file in that directory for links.
