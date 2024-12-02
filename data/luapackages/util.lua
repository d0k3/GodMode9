local util = {}

-- https://stackoverflow.com/a/71896879
function util.bytes_to_hex(data)
    local hex = {}
    local char
    for i = 1, #data do
        char = string.sub(data, i, i)
        --hex = hex..string.format("%02x", string.byte(char))
        table.insert(hex, string.format("%02x", string.byte(char)))
    end
    return table.concat(hex)
end

-- https://stackoverflow.com/a/9140231
function util.hex_to_bytes(hexstring)
    return (string.gsub(hexstring, '..', function (cc)
        return string.char(tonumber(cc, 16))
    end))
end

-- returns something like "241202"
function util.get_datestamp()
    return os.date("%y%m%d")
end

-- returns something like "010828"
function util.get_timestamp()
    return os.date("%H%M%S")
end

return util
