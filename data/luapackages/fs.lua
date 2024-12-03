local fs = {}

fs.move = _fs.move
fs.remove = _fs.remove
fs.copy = _fs.copy
fs.mkdir = _fs.mkdir
fs.list_dir = _fs.list_dir
fs.stat = _fs.stat
fs.stat_fs = _fs.stat_fs
fs.dir_info = _fs.dir_info
fs.ask_select_file = _fs.ask_select_file
fs.ask_select_dir = _fs.ask_select_dir
fs.exists = _fs.exists
fs.is_dir = _fs.is_dir
fs.is_file = _fs.is_file
fs.read_file = _fs.read_file
fs.fill_file = _fs.fill_file
fs.make_dummy_file = _fs.make_dummy_file
fs.truncate = _fs.truncate
fs.img_mount = _fs.img_mount
fs.img_umount = _fs.img_umount
fs.get_img_mount = _fs.get_img_mount
fs.hash_file = _fs.hash_file
fs.hash_data = _fs.hash_data
fs.verify = _fs.verify
fs.allow = _fs.allow
fs.sd_is_mounted = _fs.sd_is_mounted
fs.sd_switch = _fs.sd_switch
fs.fix_cmacs = _fs.fix_cmacs

function fs.write_file(path, offset, data)
    local success, filestat
    if type(offset) == "string" then
        if offset == "end" then
            success, filestat = pcall(fs.stat, path)
            if success then
                offset = filestat.size
            else
                -- allows for this to work on files that don't yet exist
                offset = 0
            end
        else
            error("offset should be an integer or \"end\"")
        end
    elseif type(offset) ~= "number" then
        error("offset should be an integer or \"end\"")
    end

    return _fs.write_file(path, offset, data)
end

return fs
