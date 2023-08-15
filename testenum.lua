function PrintTable(tbl)
    for k, v in pairs(tbl) do
        print(k, v)
    end
end

print('-- Enum --')
PrintTable(Enum)
print('-- Enum.FS --')
PrintTable(Enum.FS)
UI.ShowPrompt(false, "done")
