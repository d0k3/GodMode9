function PrintTable(tbl)
  for k, v in pairs(tbl) do
    print(k, v)
  end
end
print("First clock" .. os.clock())
print(os.time())
print(os.time({10, 10, 10, 10, 10, 10}))
print(os.date())
print(os.date("%x - %I:%M%p"))

print("Second clock" .. os.clock())


PrintTable(os.date("*t"))
PrintTable(os.date("*t", 1286705410))
print(os.date("%c hehe", 1286705410))
UI.ShowPrompt(false, "Press A")
