print("fs.verify nand:", fs.verify("S:/nand.bin"))
print("fs.verify title:", fs.verify("1:/title/00040010/00021000/content/00000051.app"))
print("fixing cmacs")
fs.fix_cmacs("1:/")
print("done")

ui.echo("Done")
