function PrintTable(tbl)
  for k, v in pairs(tbl) do
    print(k, v)
  end
end
print("First clock " .. string.format("%.8f", os.clock()))
print(os.time())
print(os.time({10, 10, 10, 10, 10, 10}))
print(os.date())
print(os.date("%x - %I:%M%p"))

print("Second clock " .. string.format("%.8f", os.clock()))


PrintTable(os.date("*t"))
print("")
PrintTable(os.date("*t", 1286705410))
print("")
print(os.date("%c hehe", 1286705410))
UI.ShowPrompt("Press A")
