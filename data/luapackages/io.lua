-- This module has not been fully tested against Lua's built-in module.
-- Functionality might be incorrect.

-- TODO: set up __gc in files so they can clean up properly

local io = {}

local file = {}
file.__index = file

-- readline buffer size
local bufsize = 512

local function debugf(...)
    print("DEBUG:", table.unpack({...}))
end

local function not_impl(fnname)
    return function (...)
        error(fnname.." is not implemented")
    end
end

local function readline(file, keepnewline)
    local result = {}
    local buf, baseseek, chr
    local dobreak = false
    while true do
        baseseek = file:seek()
        buf = file:read(bufsize)
        -- returns nil if nothing was read
        if not buf then break end
        for i = 1, #buf do
            chr = string.sub(buf, i, i)
            if chr == '\n' then
                table.insert(result, string.sub(buf, 1, i - 1))
                file:seek('set', baseseek + i)
                if keepnewline then
                    table.insert(result, '\n')
                end
                dobreak = true
                break
            end
        end
        if dobreak then break end
        table.insert(result, buf)
    end
    if #result == 0 then
        return nil
    end
    return table.concat(result)
end

-- some errors should return nil, an error string, then an error number
-- example:
-- > io.open("/nix/abc.txt", "w")
-- nil     /nix/abc.txt: Permission denied 13
-- > io.open("/nix/abc.txt", "r")
-- nil     /nix/abc.txt: No such file or directory 2
-- > a = io.open("abc.txt", "w")
-- > a:read()
-- nil     Bad file descriptor     9
function file.new(filename, mode)
    -- TODO: make this more accurate (disallow opening dirs as files)
    local success, stat, of, allowed
    if mode == nil then
        mode = "r"
    end
    of = setmetatable({_filename=filename, _mode=mode, _seek=0, _open=true, _readable=false, _writable=false, _append_only=false}, file)
    if string.find(mode, "w", 1, true) then
        -- preemptively allow writing instead of having that prompt at file:write
        allowed = fs.allow(filename)
        if not allowed then return nil, filename..": Permission denied", 13 end
        -- write mode truncates the file to 0 normally
        success = pcall(fs.truncate, filename, 0)
        if not success then return nil, filename..": Cannot truncate virtual files", 2001 end
        of._stat = {}
        of._size = 0
        of._readable = false
        of._writable = true
    elseif string.find(mode, "r+", 1, true) then
        allowed = fs.allow(filename)
        if not allowed then return nil, filename..": Permission denied", 13 end
        success, stat = pcall(fs.stat, filename)
        if success then
            if stat.type == "dir" then return nil end
            of._stat = stat
            of._size = stat.size
        else
            of._stat = {}
            of._size = 0
        end
    elseif string.find(mode, "a", 1, true) then
        allowed = fs.allow(filename)
        if not allowed then return nil, filename..": Permission denied", 13 end
        of._append_only = true
        of._writable = true
        success, stat = pcall(fs.stat, filename)
        if success then
            if stat.type == "dir" then return nil end
            of._stat = stat
            of._size = stat.size
        else
            of._stat = {}
            of._size = 0
        end
        if string.find(mode, "+", 1, true) then
            of._readable = true
        else
            of._readable = false
            of._seek = of._size
        end
    else
        -- check if file exists first
        success, stat = pcall(fs.stat, filename)
        -- lua returns nil if it fails to open for some reason
        if not success then return nil, filename..": No such file or directory", 2 end
        if stat.type == "dir" then return nil end
        of._stat = stat
        -- this is so i can adjust the size when data is written
        of._size = stat.size
        of._readable = true
    end
    return of
end

function file:_closed_check()
    if not self._open then error("attempt to use a closed file") end
end

function file:_bad_desc()
    return nil, "Bad file descriptor", 9
end

function file:close()
    -- yes, this check and error happens even on a closed file
    self:_closed_check()
    self._open = false
    return true
end

function file:flush()
    self:_closed_check()
    -- nothing happens here
end

function file:read(...)
    self:_closed_check()
    if not self._readable then return file:_bad_desc() end
    local to_return = {}
    local args = {...}
    if #args == 0 then
        args = {"l"}
    end
    for i, v in ipairs(args) do
        if v == "n" then
            error('mode "'..v..'" is not implemented')
        elseif v == "l" or v == "L" then
            -- "L" will keep the newline, if present
            return readline(self, v == "L")
        elseif v == "a" then
            local btr = self._size - self._seek
            local data = fs.read_file(self._filename, self._seek, btr)
            self._seek = self._seek + string.len(data)
            table.insert(to_return, data)
        else
            -- assuming this is a number...
            if self._seek >= self._size then
                return nil
            end
            local data = fs.read_file(self._filename, self._seek, v)
            self._seek = self._seek + string.len(data)
            table.insert(to_return, data)
        end
    end
    return table.unpack(to_return)
end

function file:lines(...)
    local args = {...}
    if #args == 0 then
        args = {"l"}
    end

    local function generator()
        return self:read(table.unpack(args))
    end

    return generator
end

function file:seek(whence, offset)
    if whence == nil then
        whence = "cur"
    end
    if offset == nil then
        offset = 0
    end
    if type(offset) ~= "number" then
        error("bad argument #2 to 'seek' (number expected, got "..type(offset)..")")
    end

    if whence == "set" then
        self._seek = offset
    elseif whence == "cur" then
        self._seek = self._seek + offset
    elseif whence == "end" then
        self._seek = self._size + offset
    else
        error("bad argument #1 to 'seek' (invalid option '"..tostring(whence).."')")
    end

    return self._seek
end

function file:write(...)
    if not self._writable then return file:_bad_desc() end
    local to_write = ''
    for i, v in pairs({...}) do
        to_write = to_write..tostring(v)
    end
    local len = string.len(to_write)
    if self._append_only then
        self:seek("end")
    end
    local br = fs.write_file(self._filename, self._seek, to_write)
    self._seek = self._seek + br
    if self._seek > self._size then
        self._size = self._seek
    end
    return self
end

file.setvbuf = not_impl("file:setvbuf")

function io.open(filename, mode)
    return file.new(filename, mode)
end

io.close = not_impl("io.close")
io.flush = not_impl("io.flush")
io.lines = not_impl("io.lines")
io.output = not_impl("io.output")
io.popen = not_impl("io.popen")
io.read = not_impl("io.read")
io.tmpfile = not_impl("io.tmpfile")
io.type = not_impl("io.type")
io.write = not_impl("io.write")

return io
