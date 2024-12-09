local options = {
    "0:/boot.firm",
    "0:/luma.firm",
    "1:/boot.firm",
    "1:/luma.firm"
}

local sel = ui.ask_selection("Choose one to boot.", options)
if sel then
    local path = options[sel]
    local doboot = ui.ask("Boot "..path.."?")
    if doboot then
        sys.boot(path)
    else
        ui.echo("I won't boot it then!")
    end
end
