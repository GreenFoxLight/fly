## The fly programming language

This is a small C-like programming language, meant to "replace" C.

### Building on Unix (macOS, linux)

Simply execute `build.sh`.

### Building on Windows

You need Visual Studio installed (tested with VS Community 2015).
If necessary, change the path to vcvarsall.bat inside `shell.bat` and remove the line including
my vim-installation in PATH:

```
    set PATH=%PATH%;"D:Program Files (x86)\Vim\vim80" <-- Remove this
    "D:\Program Files (x86)\Visual Studio 2015\VC\vcvarsall.bat" x64 <-- Change this
```

