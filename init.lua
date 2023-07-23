thingy(tostring("a"))
thingy(tostring(math))
index = 1
thingy(tostring(math.sin(3)))
thingy(tostring(3 ^ 2))
for k, v in pairs(math) do
	thingy("math func "..tostring(index).."/"..tostring(#math)..": \n- "..tostring(k).."\n  "..tostring(v))
	index = index + 1
end
