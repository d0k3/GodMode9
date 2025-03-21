--[[
    GodMode9 "Hello Script"
    Tutorial script - read / run this to learn how it works
    last changed: 2025/03/16
    author: ihaveahax
    based on HelloScript.gm9 by d0k3
]]

-- This script intends to show off the features of Lua within GodMode9.
-- While I hope it works enough to show the syntax, it is not a complete Lua tutorial.

-- For a comparison between Lua and GM9Script, look inside lua-doc.md, inside the release archive, and in "resources" in the repository.

-- Comments begin with a '--'. Multiline comments begin with '--[[' and then end with ']]'.

-- This is an example function, the echo function from the ui module.
-- It is simular to the "echo" command from GM9Script.
ui.echo("'Hello Script'\nby d0k3 and ihaveahax\n \nThis is a sample script to\nshowcase Lua scripting.") -- comments work anywhere

-- QR codes
-- The 'ui.show_qr' function will display text on the bottom screen, and a QR code on the top screen.
ui.show_qr("Scan for cool stuff!", "https://github.com/d0k3/GodMode9")

-- The basic 'print' function is also available.
-- This is useful for debugging. It replaces the top screen with an output log.
print("I used print just now!")
print("I'm on the top screen! (unless you used SWITCH_SCREENS)")
ui.echo("Check the top screen!")

-- Asking the user
-- The 'ui.ask' function will ask the user to answer a prompt with yes or no.
-- It returns a boolean.
local answer = ui.ask("Continue running this script?")
if not answer then
    print("Stopping now!")
    -- Stop executing a script if you return at the top level.
    return
end

print("Continuing execution!")

-- Let's first make sure there's nothing mounted.
-- The tests coming up will write to the ramdrive,
-- but it might be potentially taken over by IMGNAND.
print("Unmounting")
fs.img_umount()

-- Most commands that interact with files accept options.
-- This is a table that's the final argument. Each option accepts a boolean.
-- All the options that functions can take is in the documentation, but these are the most common:
--   no_cancel - Don't allow the user to cancel
--   silent - Don't show progress
--   overwrite - Overwrite files
--   skip_all - Skip existing files
print("Copying M:/otp.mem to 9:/otp_copied_by_lua.mem!")
fs.copy("M:/otp.mem", "9:/otp_copied_by_lua.mem", {overwrite=true})

-- Constants
-- These variables are set by GodMode9 when executing a Lua script.
--   GM9VER - the GodMode9 version number
--   SCRIPT - the executed script, such as "0:/gm9/luascripts/HelloScript.lua"
--   CURRDIR - the directory of the executed script, such as "0:/gm9/luascripts"
--   GM9OUT - the standard output path "0:/gm9/out"
--   HAX - the hax the system is currently running from, whihc can be "ntrboot", "sighax", or an empty string
--   NANDSIZE - total size of SysNAND in bytes
--   CONSOLE_TYPE - the string "O3DS" or "N3DS"
--   IS_DEVKIT - true if the console is a developer unit
--
-- Some that were environmental variables are now part of the sys or util modules.
--   sys.sys_id0 - id0 belonging to SysNAND
--   sys.emu_id0 - id0 belonging to EmuNAND (nil if not available)
--   sys.serial - serial number
--   sys.region - region of the SysNAND ("JPN", "USA", "EUR", "AUS", "CHN", "KOR", "TWN", nil (if it does not exist, or the region byte is invalid)
--   util.get_timestamp() - current time in hhmmss format, equivalent to os.date("%y%m%d")
--   util.get_datestamp() - current date in YYMMDD format, equivalent to os.date("%H%M%S")
--
-- Let's now print a bunch of these out using string concatenation!
local retail_or_devkit = "retail"
if IS_DEVKIT then
    retail_or_devkit = "devkit"
end
ui.echo("Your GodMode9 version is "..GM9VER..
    "\nYour region is "..sys.region..
    "\nYour serial number is "..sys.serial..
    "\nYour std out oath is "..GM9OUT..
    "\nCurrent dir is "..CURRDIR..
    "\nCurrent hax is "..HAX..
    "\nYour system is a "..retail_or_devkit.. 
    "\nCurrent timestamp is: "..util.get_timestamp()..
    "\nYour sys / emu id0 is:\n"..sys.sys_id0.."\n"..tostring(sys.emu_id0)) -- emu_id0 could be nil!

-- Functions can raise errors. If these are not handled, the script stops executing.
-- Uncomment this below to see that in action:
--nonexistant()
-- If you attempt this, the error would be like "attempt to call a nil value (global 'nonexistant')".
-- However you can catch errors and handle them gracefully by using Lua's pcall.
-- Here's an example function that will fail, but it won't stop the script.
-- This would be the same as doing fs.copy("0:/otp.mem", "M:/cantdothis.mem")
local success, result = pcall(fs.copy, "M:/otp.mem", "V:/cantdothis.mem")
if not success then
    print("fs.copy failed:")
    print(result)
    ui.echo("That was close!\nThis fs.copy failed (on purpose)."..
        "\n \nYou could now check the error\nin the result variable.")
end

-- There are a few ways to ask for input.
-- 'ui.ask_text' shows a keyboard. You can specify initial text and the maximum length.
local my_text = ui.ask_text("Gimmie some text up to 16 characters!", "GodMode9!!!", 16)

-- 'ui.ask_number' will ask for a number.
local my_number = ui.ask_number("I want a number now!", 123)

-- 'ui.ask_hex' will ask for a hexadecimal number. You can set a maximum number of hex digits.
local my_hex = ui.ask_hex("Now a hex number.", 0x8badf00d, 8)

-- 'ui.ask_selection' will ask the user to pick from a selection.
-- It returns the index of the selected option, or nil if nothing.
local selections = {
    "64",
    "Melee",
    "Brawl",
    "for Nintendo 3DS",
    "for Wii U",
    "Ultimate"
}
local sel_number = ui.ask_selection("Pick your favorite Smash", selections)
local selection
if sel_number then
    selection = selections[sel_number]
else
    selection = "(None of them)"
end

-- Now let's print them all.
ui.echo("You've put in: "..
    "\nSome text: "..my_text..
    "\nA number: "..my_number..
    "\nA hex number: "..my_hex..
    "\nA selection: "..selection)

-- Now let's play with hashes.
-- 'fs.hash_file' will return a SHA256 hash as a bytestring.
local nand_hdr_sha = fs.hash_file("S:/nand_hdr.bin", 0, 0)
-- Write it out to a sha file. We're doing this in the ramdrive for testing.
fs.write_file("9:/nand_hdr.bin.sha", 0, nand_hdr_sha)
-- Let's copy over nand_hdr.bin so we can use a convenience function to verify it.
fs.copy("S:/nand_hdr.bin", "9:/nand_hdr.bin", {no_cancel=true, overwrite=true})
-- And now we can use 'fs.verify_with_sha_file'!
-- This will check the .sha file next to the file we want.
local is_valid = fs.verify_with_sha_file("9:/nand_hdr.bin")
print("Is 9:/nand_hdr.bin valid?", is_valid)

-- 'fs.copy' is also capable of generating a hash on copy.
-- This will create 9:/nand_cid.mem.sha.
fs.copy("M:/nand_cid.mem", "9:/nand_cid.mem", {no_cancel=true, calc_sha=true, overwrite=true})

-- Files in certain formats can be verified with 'fs.verify'.
-- This will use the hashes within FIRM1 to check its validity.
local firm1_is_valid = fs.verify("S:/firm1.bin")
print("Is FIRM1 valid?", firm1_is_valid)

ui.echo("Check the top screen!")

-- There are loads more functions! Every feature of GM9Script has an equivalent feature within Lua.
-- Check the comprehensive documentation of every GM9 Lua function in lua-doc.md
-- For standard Lua 5.4 functions, check the reference manual: https://www.lua.org/manual/5.4/

local do_reboot = ui.ask("Congratulatins! You have finished the script!\n \nDo you want to reboot now?")
if do_reboot then
    sys.reboot()
end
