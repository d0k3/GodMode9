print("Testing")
-- create file normally
fs.write_file("9:/test1.txt", 0, "test")
-- append to end of it
fs.write_file("9:/test1.txt", "end", "ing")
-- try to append, but should create if it doesn't exist
fs.write_file("9:/test2.txt", "end", "stuff")
ui.echo("Done")
