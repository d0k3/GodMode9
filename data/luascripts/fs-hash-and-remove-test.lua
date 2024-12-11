local function printtable(tbl)
    for k, v in pairs(tbl) do
        print(k, "=", v)
    end
end
printtable(fs.stat("S:/firm0.bin"))
ui.echo("Waiting")
print("Copying")
fs.copy("S:/firm0.bin", "9:/firm0.bin", {calc_sha=true})
print("Hashing")
print("Result:", fs.verify_with_sha_file("9:/firm0.bin"))
print("Removing")
print(os.remove("9:/firm0.bin"))
print(os.remove("9:/firm0.bin"))
ui.echo("Done")
