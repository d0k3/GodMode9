function RGB(r, g, b)
    return ((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3))
end

white = RGB(0xFF, 0xFF, 0xFF)
black = RGB(0x00, 0x00, 0x00)
grey = RGB(0x80, 0x80, 0x80)
red = RGB(0xFF, 0x00, 0x00)
superfuchsia = RGB(0xFF, 0x00, 0xEF)

list = {
    "Half-Life",
    "Half-Life: Opposing Force",
    "Half-Life: Blue Shift",
    "Half-Life: Source",
    "Half-Life 2",
    "Half-Life 2: Episode One",
    "Half-Life 2: Episode Two",
    "Counter-Strike",
    "Counter-Strike: Condition Zero",
    "Counter-Strike: Condition Zero Deleted Scenes",
    "Counter-Strike: Source",
    "Counter-Strike: Global Offensive",
    "Left 4 Dead",
    "Left 4 Dead 2",
    "Portal",
    "Portal 2",
    "Team Fortress 2",
    "Dota 2",
    "Half-Life: Alyx",
    "Black Mesa",
    "Garry's Mod",
    "Portal Stories: Mel",
    "The Stanley Parable"
}

function drawshadow(which, text, x, y)
    UI.DrawString(which, text, x+1, y+1, grey, black)
    UI.DrawString(which, text, x, y, white, superfuchsia)
end
function drawshadow2(which, text, x, y)
    UI.DrawString(which, text, x+1, y+1, grey, black)
    UI.DrawString(which, text, x, y+1, grey, superfuchsia)
    UI.DrawString(which, text, x+1, y, grey, superfuchsia)
    UI.DrawString(which, text, x, y, white, superfuchsia)
end

for i = 8, 12 do
    UI.ClearScreenF(false, true, red)
    UI.DrawString(1, "Current height: "..i, 0, 0, white, black)
    for k, v in ipairs(list) do
        UI.DrawString(1, k..": "..v, 0, (k) * i, white, black)
    end
    UI.ShowPrompt(false, "Press A")
end
for k, v in ipairs(list) do
    print(k, v)
    UI.ShowPrompt(false, "Press A")
end

print('test1')
UI.ShowPrompt(false, "Press A")
print('test2')
UI.ShowPrompt(false, "Press A")
print('does it work')
UI.ShowPrompt(false, "Press A")
