function PrintTable(tbl)
	for k, v in pairs(tbl) do
		print(k, v)
	end
end

print('-- Enum --')
PrintTable(Enum)
print('-- Enum.UI --')
PrintTable(Enum.UI)
UI.ShowPrompt("Done")
