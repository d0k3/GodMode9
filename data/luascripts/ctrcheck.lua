local version = "5"
local store_log = false

local NAND_HDR = "S:/nand_hdr.bin"

local function make_selection_text()
    return "Select which parts of the system to check.\n"..
        "Console is a "..sys.region.." "..CONSOLE_TYPE.." "..(IS_DEVKIT and "devkit" or "retail")..".\n"..
        "Current id0 is "..(sys.sys_id0 or "unknown").."\n"..
        "Permanent logging: "..(store_log and "enabled" or "disabled")
end

local options_short = {"all", "nand", "sd", "togglelog", "exit"}
local options = {
    "All",
    "NAND Only",
    "SD Only",
    "Toggle permanent log",
    "Exit"
}

local function chk_nand_header()
    local log = {}
    -- TODO: replace the multiple file reads with fs.hash_data once that's implemented
    local is_sighax = false
    local success, stat = pcall(fs.stat, NAND_HDR)
    if not success then
        table.insert(log, "Error: NAND header not found.")
        return
    end
    if stat.size ~= 512 then
        table.insert(log, "Error: NAND header is an invalid size (expected 512, got "..tostring(stat.size)..")")
        return
    end

    local hdr_hash = util.bytes_to_hex(fs.hash_file(NAND_HDR, 0, 0x100))

    if hdr_hash == "a4ae99b93412e4643e4686987b6cfd59701d5c655ca2ff671ce680b4ddcf0948" then
        table.insert(log, "Information: NAND header's signature is sighax.")
        is_sighax = true
    end

    local part_table_hash = util.bytes_to_hex(fs.hash_file(NAND_HDR, 0x100, 0x60))

    if part_table_hash == "dfd434b883874d8b585a102f3cf3ae4cef06767801db515fdf694a7e7cd98bc2" then
        table.insert(log, (CONSOLE_TYPE == "N3DS") and "Information: NAND header is stock. (n3DS)" or "Critical: o3DS has an n3DS nand header.")
    elseif part_table_hash == "ae9b6645105f3aec22c2e3ee247715ab302874fca283343c731ca43ea1baa25d" then
        table.insert(log, (CONSOLE_TYPE == "03DS") and "Information: NAND header is stock. (o3DS)" or "Critical: n3DS has an o3DS nand header.")
    else
        local header_magic = fs.read_file(NAND_HDR, 0x100, 4)
        if header_magic == "NCSD" then
            if is_sighax then
                table.insert(log, "Warning: NAND partition table is modified, but there is sighax in the NAND header.")
            else
                table.insert(log, "Error: NAND partition table is modified, and there is no sighax in the NAND header.")
            end
        else
            table.insert(log, "Error: NAND header data is invalid. You've met with a terrible fate, haven't you?")
        end
    end

    return table.concat(log, "\n")
end

local function chk_nand_sectors()
    local log = {}

    table.insert(log, "NAND sectors check not implemented.")

    return table.concat(log, "\n")
end

local function nand_checks()
    local nand_log = {}
    
    table.insert(nand_log, chk_nand_header())

    return table.concat(nand_log, "\n\n")
end

local function sd_checks()
    sd_log = {}

    table.insert(sd_log, "SD checks not implemented.")

    return table.concat(sd_log, "\n\n")
end

local function main()
    local final_log = ""
    while true do
        ui.show_text("ctrcheck v"..version)

        local selection = ui.ask_selection(make_selection_text(), options)
        if not selection then return end

        local seltext = options_short[selection]
        if seltext == "exit" then
            return
        elseif seltext == "togglelog" then
            store_log = not store_log
        elseif seltext == "all" then
            final_log = final_log..nand_checks().."\n\n"
            final_log = final_log..sd_checks()
        elseif seltext == "nand" then
            final_log = nand_checks()
        elseif seltext == "sd" then
            final_log = sd_checks()
        end
        if final_log ~= "" then
            final_log = "Date and Time: "..os.date().."\n---\n"..final_log
            ui.show_text_viewer(final_log)

            if store_log then
                -- append mode is not implemented in io yet
                local f = io.open(GM9OUT.."/ctrcheck_log.txt", "r+")
                f:seek("end")
                f:write(final_log, "\n")
                f:close()
                ui.echo("I think I wrote to "..GM9OUT.."/ctrcheck_log.txt")
            end

            final_log = ""
        end
    end
end

main()
