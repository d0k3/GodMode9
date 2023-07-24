UI.ShowPrompt(false, "math.sin(3): "..tostring(math.sin(3)))
UI.ShowPrompt(false, "3 ^ 2: "..tostring(3 ^ 2))

options = {'Dustbowl', 'Granary', 'Gravel Pit', 'Well', '2Fort', 'Hydro'}
result = UI.ShowSelectPrompt(options, "Choose one...")
UI.ShowPrompt(false, "You chose: "..tostring(result)..", or "..options[result])

res = UI.ShowPrompt(true, "I am asking you...")
UI.ShowPrompt(false, "I got: "..tostring(res))

words = 'black,mesa'
for word in string.gmatch(words, '([^,]+)') do
	UI.ShowPrompt(false, "gmatch test: "..word)
end

-- this is about it... can't even do io yet
