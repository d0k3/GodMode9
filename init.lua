UI.ShowPrompt(false, "math.sin(3): "..math.sin(3))
UI.ShowPrompt(false, "3 ^ 2: "..(3 ^ 2))

local options = {'Dustbowl', 'Granary', 'Gravel Pit', 'Well', '2Fort', 'Hydro'}
local result = UI.ShowSelectPrompt(options, "Choose one...")
if result then
    UI.ShowPrompt(false, "You chose: "..result..", or "..options[result])
else
    UI.ShowPrompt(false,  "Oh you don't like any of them?")
end

local res = UI.ShowPrompt(true, "I am asking you...")
UI.ShowPrompt(false, "I got: "..tostring(res))

local max = 1000000
for i = 0, max do
    if not UI.ShowProgress(i, max, "Pushing the Payload...") then break end
end
local max = 500000
for i = max, 0, -1 do
    if not UI.ShowProgress(i, max, "Un-copying your file...") then break end
end
local max = 1000
for i = 0, max do
    UI.ShowString("Pushing the Payload..."..i)
end

local words = 'black,mesa'
for word in string.gmatch(words, '([^,]+)') do
	UI.ShowPrompt(false, "gmatch test: "..word)
end

-- this is about it... can't even do io yet
