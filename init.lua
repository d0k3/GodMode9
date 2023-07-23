thingy("A test of your reflexes!")
thingy("math.sin(3): "..tostring(math.sin(3)))
thingy("3 ^ 2: "..tostring(3 ^ 2))

thingy(string.sub("freeman", 5))

words = 'black,mesa'
for word in string.gmatch(words, '([^,]+)') do
	thingy("gmatch test: "..word)
end

-- this is about it... can't even do io yet
