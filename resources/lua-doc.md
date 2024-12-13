# GodMode9 Lua documentation

> [!IMPORTANT]
> WORK IN PROGRESS! A list of functions is available at the [Lua support pull request](https://github.com/d0k3/GodMode9/pulls).

GodMode9 includes a Lua 5.4.7 implementation.

# Running scripts

There are four ways to run Lua scripts:

* Select it in the file browser and choose "`Execute Lua script`"
* Place it in `0:/gm9/luascripts`, then open the HOME menu, choose "`Lua scripts...`", then choose the script
* Place it in `data/luascripts`, then build GodMode9 from source
    * This takes precedence over `0:/gm9/luascripts`
* Place it at `data/autorun.lua`, then build GodMode9 from source with `SCRIPT_RUNNER=1` to automatically run it at launch

# Packages

Lua scripts can load custom modules. The default search path (defined as `package.path`) is:

```
0:/gm9/luapackages/?.lua;0:/gm9/luapackages/?/init.lua;V:/luapackages/?.lua;V:/luapackages/?/init.lua
```

For example, when a script calls `require("foobar")`, the package searcher will look for the module in this order:

* `0:/gm9/luapackages/foobar.lua`
* `0:/gm9/luapackages/foobar/init.lua`
* `V:/luapackages/foobar.lua`
* `V:/luapackages/foobar/init.lua`

# Comparison with GM9Script

These tables are provided to assist with converting from GM9Script to Lua.

## Commands

GM9 | Lua | Notes
-- | -- | --
goto | http://lua-users.org/wiki/GotoStatement |  
labelsel | ui.ask_selection |  
keychk | ui.check_key |  
echo | ui.echo |  
qr | ui.show_qr |  
ask | ui.ask |  
input | ui.ask_input |  
filesel | fs.ask_select_file |  
dirsel | fs.ask_select_dir |  
set | local var = value | More details on variables and scoping: https://www.lua.org/pil/4.2.html
strsplit | string.find and string.sub |  
strrep | string.gsub | https://www.lua.org/manual/5.4/manual.html#pdf-string.gsub
allow | fs.allow |  
cp | fs.copy |  
mv | fs.move |  
inject | fs.write_file |  
fill | fs.fill_file |  
fdummy | fs.make_dummy_file |  
rm | fs.remove |  
mkdir | fs.mkdir |  
mount | fs.img_mount |  
umount | fs.img_umount |  
find | fs.find |  
findnot | fs.find_not |  
fget | fs.write_file |  
fset | fs.write_file |  
sha | fs.hash_file OR fs.verify_with_sha_file | hash_file simply returns a hash, verify_with_sha_file compares it with a corresponding .sha file
shaget | fs.hash_file |  
dumptxt | fs.write_file | Use "end" for offset to append data
fixcmac | fs.fix_cmacs |  
verify | fs.verify |  
decrypt | title.decrypt |  
encrypt | title.encrypt |  
buildcia | title.build_cia |  
install | title.install |  
extrcode | title.extract_code |  
cmprcode | title.compress_code |  
sdump | fs.key_dump |  
applyips | target.apply_ips |  
applybps | target.apply_bps |  
applybpm | target.apply_bpm |  
textview | fs.show_file_text_viewer | fs.show_text_viewer can be used to show text from a variable
cartdump | fs.cart_dump |  
isdir | fs.is_dir |  
exist | fs.exists |  
boot | sys.boot |  
switchsd | fs.sd_switch |  
nextemu | sys.next_emu |  
reboot | sys.reboot |  
poweroff | sys.power_off |  
bkpt | bkpt |  

## PREVIEW_MODE variable

Unlike the `PREVIEW_MODE` GM9Script variable, this has been split into multiple functions.

Setting | Lua
-- | --
“quick” and “full” | (There is no alternative to view a Lua script as it’s running.)
“off” | ui.clear
text | ui.show_text
png file | ui.show_png
game icon | ui.show_game_info

## Other constants

GM9 | Lua | Notes
-- | -- | --
DATESTAMP | util.get_datestamp() | Formatted like “241202”, equivalent to os.date("%y%m%d")
TIMESTAMP | util.get_timestamp() | Formatted like “010828”, equivalent to os.date("%H%M%S")
SYSID0 | sys.sys_id0 |  
EMUID0 | sys.emu_id0 |  
EMUBASE | sys.emu_base |  
SERIAL | sys.serial |  
REGION | sys.region |  
SDSIZE | fs.stat_fs("0:/").total | int instead of string (use util.format_bytes to format it)
SDFREE | fs.stat_fs("0:/").free | int instead of string (use util.format_bytes to format it)
NANDSIZE | NANDSIZE | int instead of string (use util.format_bytes to format it)
GM9OUT | GM9OUT |  
CURRDIR | CURRDIR | nil instead of “(null)” if it can’t be found
ONTYPE | CONSOLE_TYPE | “O3DS” or “N3DS”
RDTYPE | IS_DEVKIT | boolean instead of a string
HAX | HAX |  
GM9VER | GM9VER |  

# Comparisons with standard Lua

These original Lua 5.4 modules are fully available:
* [Basic functions](https://www.lua.org/manual/5.4/manual.html#6.1)
    * `print` will replace the top screen with an output log. Currently it does not implement some features such as line wrapping.
    * dofile and loadfile don't work yet.
* [coroutine](https://www.lua.org/manual/5.4/manual.html#6.2)
* [debug](https://www.lua.org/manual/5.4/manual.html#6.10)
* [math](https://www.lua.org/manual/5.4/manual.html#6.7)
* [string](https://www.lua.org/manual/5.4/manual.html#6.4)
* [table](https://www.lua.org/manual/5.4/manual.html#6.6)
* [utf8](https://www.lua.org/manual/5.4/manual.html#6.5)

These modules are partially available:
* [os](https://www.lua.org/manual/5.4/manual.html#6.9)
    * Only `os.clock`, `os.time`, `os.date`, `os.difftime`, and `os.remove`
* [io](https://www.lua.org/manual/5.4/manual.html#6.8)
    * Only `io.open`; for open files, all but `file:setvbuf` and `file:lines`
    * This is a custom compatibility module that uses `fs` functions. If there are differences compared to the original `io` implementation, please report them as issues.

These modules work differently:
* [package](https://www.lua.org/manual/5.4/manual.html#6.3)
    * `package.cpath` and `package.loadlib` are nonfunctional due to GM9 having no ability to load dynamic libraries.

# API reference

## Constants

### GM9VER
The version such as `v2.1.1-159-gff2cb913`, the same string that is shown on the main screen.

### SCRIPT
Path to the executed script, such as `0:/gm9/luascripts/myscript.lua`.

### CURRDIR
Directory of the executed script, such as `0:/gm9/luascripts`.

### GM9OUT
The value `0:/gm9/out`.

### HAX
> [!WARNING]
> This needs checking if it's accurate.
One of three values:
* "ntrboot" if started from an ntrboot cart
* "sighax" if booted directly from a FIRM partition
* Empty string otherwise

### NANDSIZE
Total size of SysNAND in bytes.

### CONSOLE_TYPE
The string `"O3DS"` or `"N3DS"`.

### IS_DEVKIT
`true` if the console is a developer unit.

## `ui` module

> [!NOTE]
> This assumes the default build is used, where the bottom screen is the main screen. If GodMode9 is compiled with `SWITCH_SCREENS=1`, then every instance where something appears on the bottom screen will actually be on the top screen and vice versa.

### ui.echo

`void ui.echo(string text)`

Display text on the bottom screen and wait for the user to press A.

* **Arguments**
    * `text` - Text to show the user

### ui.ask

`bool ui.ask(string text)`

Prompt the user with a yes/no question.

* **Arguments**
    * `text` - Text to ask the user
* **Returns:** `true` if the user accepts

### ui.ask_hex

`int ui.ask_hex(string text, int initial, int n_digits)`

Ask the user to input a hex number.

* **Arguments**
    * `text` - Text to ask the user
    * `initial` - Starting value
    * `n_digits` - Amount of hex digits allowed
* **Returns:** the number the user entered, or `nil` if canceled

### ui.ask_number

`int ui.ask_number(string text, int initial)`

Ask the user to input a number.

* **Arguments**
    * `text` - Text to ask the user
    * `initial` - Starting value
* **Returns:** the number the user entered, or `nil` if canceled

### ui.ask_text

`string ui.ask_text(string prompt, string initial, int max_length)`

Ask the user to input text.

* **Arguments**
    * `prompt` - Text to ask the user
    * `initial` - Starting value
    * `max_length` - Maximum length of the string
* **Returns:** the text the user entered, or `nil` if canceled

### ui.ask_selection

`int ui.ask_selection(string prompt, array options)`

Ask the user to choose an option from a list. A maximum of 256 options are allowed.

* **Arguments**
    * `prompt` - Text to ask the user
    * `options` - Table of options
* **Returns:** index of selected option, or `nil` if canceled

### ui.clear

`void ui.clear()`

Clears the top screen.

### ui.show_png

`void ui.show_png(string path)`

Displays a PNG file on the top screen.

The image must not be larger than 400 pixels horizontal or 240 pixels vertical. If `SWITCH_SCREENS=1` is used, it must not be larger than 320 pixels horizontal.

* **Arguments**
    * `path` - Path to PNG file
* **Throws** 
    * `"Could not read (file)"` - file does not exist or there was another read error
    * `"Invalid PNG file"` - file is not a valid PNG
    * `"PNG too large"` - too large horizontal or vertical, or an out-of-memory error

### ui.show_text

`void ui.show_text(string text)`

Displays text on the top screen.

* **Arguments**
    * `text` - Text to show the user

### ui.show_game_info

`void ui.show_game_info(string path)`

Shows game file info. Accepts any files that include an SMDH, a DS game file, or GBA file.

* **Arguments**
    * `path` - Path to game file (CIA, CCI/".3ds", SMDH, TMD, TIE (DSiWare export), Ticket, NDS, GBA)
* **Throws**
    * `"ShowGameFileIcon failed on (path)"` - failed to get game info from path

### ui.show_qr

`void ui.show_qr(string text, string data)`

Displays a QR code on the top screen, and a prompt on the bottom screen, and waits for the user to press A.

* **Arguments**
    * `text` - Text to show the user
    * `data` - Data to encode into the QR code
* **Throws**
    * `"could not allocate memory"` - out-of-memory error when attempting to generate the QR code

### ui.show_text_viewer

`void ui.show_text_viewer(string text)`

Display a scrollable text viewer.

* **Arguments**
    * `text` - Text to display
* **Throws**
    * `"text validation failed"` - given string contains invalid characters
    * `"failed to run MemTextViewer"` - internal memory viewer error

### ui.show_file_text_viewer

`void ui.show_file_text_viewer(string path)`

Display a scrollable text viewer from a text file.

* **Arguments**
    * `path` - Path to text file
* **Throws**
    * `"could not allocate memory"` - out-of-memory error when attempting to create the text buffer
    * `"text validation failed"` - text file contains invalid characters
    * `"failed to run MemTextViewer"` - internal memory viewer error

### ui.format_bytes

`string ui.format_bytes(int bytes)`

Format a number with `Byte`, `kB`, `MB`, or `GB`.

> [!NOTE]
> This is affected by localization and may return different text if the language is not English.

* **Arguments**
    * `bytes` - size to format
* **Returns:** formatted string

### ui.check_key

`bool ui.check_key(string key)`

Checks if the user is holding down a key.

* **Arguments**
    * `key` - a button string: `"A"`, `"B"`, `"SELECT"`, `"START"`, `"RIGHT"`, `"LEFT"`, `"UP"`, `"DOWN"`, `"R"`, `"L"`, `"X"`, `"Y"`
* **Returns:** `true` if currently held, `false` if not
