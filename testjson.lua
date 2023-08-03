local json = require('json')

local mytable = {a=1, b='two'}
UI.ShowPrompt(false, json.encode(mytable))

local myjson = '{"c": 3, "d": "four"}'
UI.ShowPrompt(false, "Decoding: \n"..myjson)
for k, v in pairs(json.decode(myjson)) do
	UI.ShowPrompt(false, "Key:   "..k.."\nValue: "..v)
end
