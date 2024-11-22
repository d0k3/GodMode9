local util = {}

-- https://stackoverflow.com/a/71896879
function util.bytes_to_hex(data)
    local hex = ''
    local char
    for i = 1, #data do
        char = string.sub(data, i, i)
        hex = hex..string.format("%02x", string.byte(char))
    end
    return hex
end

return util
