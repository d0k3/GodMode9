for i, v in pairs({"9:/test_1", "9:/test_2", "9:/extradir_3"}) do
    print("Making dir", v)
    fs.mkdir(v)
end

for i, v in pairs({"9:/test_1.bin", "9:/test_2.bin", "9:/extratest_3.bin", "9:/test_2/thingy.txt"}) do
	print("Touching file", v)
    fs.write_file(v, 0, "a")
end

print()
print("Selected:", fs.ask_select_file("Select a file!", "9:/test_*"))
print("Selected:", fs.ask_select_file("Select a file! (explorer)", "9:/test_*", {explorer=true}))

print("Selected:", fs.ask_select_dir("Select a directory!", "9:/"))
print("Selected:", fs.ask_select_dir("Select a directory! (explorer)", "9:/", {explorer=true}))

print("Selected:", fs.ask_select_file("Select a file OR directory!", "9:/test_*", {include_dirs=true}))
print("Selected:", fs.ask_select_file("Select a file OR directory! (explorer)", "9:/test_*", {include_dirs=true, explorer=true}))
print("Done")
ui.echo("Done")
