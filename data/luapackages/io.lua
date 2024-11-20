-- This module has not been fully tested against Lua's built-in module.
-- Functionality might be incorrect.

local io = {}

local file = {}
file.__index = file

local function debugf(...)
    print("DEBUG:", table.unpack({...}))
end

local function not_impl(fnname)
    return function (...)
        error(fnname.." is not implemented")
    end
end

function file.new(filename, mode)
    -- TODO: make this more accurate (disallow opening dirs as files)
    local success, stat, of, allowed
    if mode == nil then
        mode = "r"
    end
    debugf("opening", filename, mode)
    of = setmetatable({_filename=filename, _mode=mode, _seek=0, _open=true, _readonly=true}, file)
    if string.find(mode, "w") then
        debugf("opening", filename, "for writing")
        -- preemptively allow writing instead of having that prompt at file:write
        allowed = fs.allow(filename)
        debugf("allowed:", allowed)
        if not allowed then return nil end
        of._stat = {}
        of._size = 0
        of._readonly = false
    elseif string.find(mode, "r+") then
        debugf("opening", filename, "for updating")
        allowed = fs.allow(filename)
        debugf("allowed:", allowed)
        if not allowed then return nil end
        success, stat = fs.stat(filename)
        debugf("stat success:", success)
        if success then
            debugf("type:", stat.type)
            if stat.type == "dir" then return nil end
            of._stat = stat
            of._size = stat.size
        else
            of._stat = {}
            of._size = 0
        end
    else
        debugf("opening", filename, "for reading")
        -- check if file exists first
        success, stat = fs.stat(filename)
        debugf("stat success:", success)
        -- lua returns nil if it fails to open for some reason
        if not success then return nil end
        debugf("type:", stat.type)
        if stat.type == "dir" then return nil end
        of._stat = stat
        -- this is so i can adjust the size when data is written
        of._size = stat.size
    end
    debugf("returning of")
    return of
end

function file:_closed_check()
    if not self._open then error("attempt to use a closed file") end
end

function file:close()
    self._open = false
    return true
end

function file:flush()
    self:_closed_check()
    -- nothing happens here
end

function file:read(...)
    self:_closed_check()
    local to_return = {}
    for i, v in ipairs({...}) do
        if v == "n" or v == "l" or v == "L" then
            error('mode "'..v..'" is not implemented')
        elseif v == "a" then
            local btr = self._size - self._seek
            local data = fs.read_file(self._filename, self._seek, btr)
            self._seek = self._seek + string.len(data)
            table.insert(to_return, data)
        else
            -- assuming this is a number...
            local data = fs.read_file(self._filename, self._seek, v)
            self._seek = self._seek + string.len(data)
            table.insert(to_return, data)
        end
    end
    return table.unpack(to_return)
end

function file:seek(whence, offset)
    if whence == nil then
        whence = "cur"
    end
    if offset == nil then
        offset = 0
    end
    if type(offset) ~= "number" then
        error("bad argument #2 to 'seek' (number expected, got '..type(offset)..')")
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
    local to_write = ''
    for i, v in pairs({...}) do
        to_write = to_write..tostring(v)
    end
    local len = string.len(to_write)
    local br = fs.write_file(self._filename, self._seek, to_write)
    self._seek = self._seek + br
    if self._seek > self._size then
        self._size = self._seek
    end
    return self
end

file.lines = not_impl("file:lines")
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
