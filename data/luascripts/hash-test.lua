local hash = fs.hash_file("0:/boot.firm", 0, 0x200)
local hash1 = fs.hash_file("0:/boot.firm", 0, 0x200, {sha1=true})

-- https://stackoverflow.com/questions/29419345/convert-string-to-hex-in-lua
local function format_hex(str)
    local data = ''
    for i = 1, #str do
        char = string.sub(str, i, i)
        data = data..string.format("%02X", string.byte(char)).." "
    end
    return data
end

print("Hash 256:           "..format_hex(hash))
print("Hash 1:             "..format_hex(hash1))

local fullfile256 = fs.hash_file("0:/boot.firm", 0, 0)
local fullfile1 = fs.hash_file("0:/boot.firm", 0, 0, {sha1=true})

print("Hash 256 full file: "..format_hex(fullfile256))
print("Hash 1 full file:   "..format_hex(fullfile1))

if ui.ask("Create dummy?") then
    fs.write_file("9:/dummy.bin", 0, "")
end

local nodata256 = fs.hash_file("9:/dummy.bin", 0, 0)
local nodata1 = fs.hash_file("9:/dummy.bin", 0, 0, {sha1=true})

print("Hash 256 no data:   "..format_hex(nodata256))
print("Hash 1 no data:     "..format_hex(nodata1))

ui.echo("Waiting")
