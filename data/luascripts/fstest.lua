function printtable(t)
    for ik, iv in pairs(t) do
        print("", ik, ":", iv)
    end
end

print("Listing V:/")
local vfiles = fs.list_dir("V:/")
print("I have gotten the V:/:", vfiles)
for k, v in pairs(vfiles) do
    print("File", k)
    printtable(v)
end
ui.echo("Look up there!")

local bootstat = fs.stat("0:/boot.firm")
print("For that boot firm:")
printtable(bootstat)
