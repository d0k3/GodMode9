json = require('json')
fpath = "0:/test.json"

print("Reading: "..fpath)
data = FS.FileGetData("0:/test.json", -1, 0)
print("Got type "..type(data))
print("Got data of "..string.len(data).." bytes")
UI.ShowPrompt(false, "got data:\n"..data)

print("Parsing...")
parsed = json.decode(data)
for k, v in pairs(parsed) do
	UI.ShowPrompt(false, "Key:   "..k.."\nValue: "..v)
end
