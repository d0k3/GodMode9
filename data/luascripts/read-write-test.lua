local stuff = fs.read_file("0:/boot.firm", 0, 4)
print("I got:", stuff)

local written = fs.write_file("9:/test.txt", 0, "Yeah!!!")
print("I wrote:", written)

ui.echo("Waiting...")
