local f = io.open("9:/test.txt", "w")
f:write("test")
f:close()

local f2 = io.open("9:/test.txt", "a")
f2:write("in")
f2:close()

local f3 = io.open("9:/test.txt", "a+")
ui.echo("Reading 3 bytes: "..f3:read(3))
f3:write("g")
f3:close()

local f4 = io.open("9:/test2.txt", "a")
f4:write("TESTING")
f4:close()

ui.echo("Done, check 9:/test.txt")
