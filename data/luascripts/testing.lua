ui.echo("a")
print("a")
print("GM9VERSION: "..GM9VERSION)
print("SCRIPT: "..SCRIPT)
ui.echo("b")

local thingy = sys.testtable()
ui.echo(tostring(thingy))
for k, v in pairs(thingy) do
    ui.echo(tostring(k)..": "..tostring(v))
end
