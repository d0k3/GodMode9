print("allow (no table):")
print(fs.allow("1:/"))
print("allow (ask_all=false):")
print(fs.allow("1:/", {ask_all=false}))
print("allow (ask_all=true):")
print(fs.allow("1:/", {ask_all=true}))

ui.echo("Done")
