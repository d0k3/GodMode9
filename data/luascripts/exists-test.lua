for i, v in pairs({"0:/boot.firm", "S:/nope.bin"}) do
    print(v, "exists", fs.exists(v))
    print(v, "is_dir", fs.is_dir(v))
    print(v, "is_file", fs.is_file(v))
end
