UI.ShowPrompt(false, "I am about to mount...")
local res = FS.InitImgFS("0:/finalize.romfs")
UI.ShowPrompt(false, "Did it work? \n"..tostring(res))
