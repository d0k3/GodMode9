# GodMode9 but with lua

A simple test... Includes Lua 5.4.6 with a few changes for compile-time warnings

* Makefile.build edited to support a LIBS variable on build

* Math lib (-lm) now needed for lua

* link.ld on arm9 edited to include .ARM.exidx into .rodata, caused by lua's code. Limit of AHBWRAM max size was increased until address 0x80C0000, 744KiB since 0x8006000

Copy init.lua to the SD card and select it. There is now an "Execute Lua script" option.

This has one custom library so far, "UI", that exposes two functions:
* UI.ShowPrompt(ask, text)
* UI.ShowSelectPrompt(optionstable, text)

Before you think about using this: it doesn't even have an API or a proper way to run the code. This is currenetly ONLY a test to see if Lua can be used at all.
