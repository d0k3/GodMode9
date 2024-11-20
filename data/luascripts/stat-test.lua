local function printtable(tbl)
    for k, v in pairs(tbl) do
        print("-", k, ":", v, "(", ui.format_bytes(v), ")")
    end
end

print("fs.stat_fs 0:/")
printtable(fs.stat_fs("0:/"))
print("fs.stat_fs 1:/")
printtable(fs.stat_fs("1:/"))

print("fs.dir_info 0:/")
printtable(fs.dir_info("0:/"))
print("fs.dir_info 1:/")
printtable(fs.dir_info("1:/"))

ui.echo("done")
