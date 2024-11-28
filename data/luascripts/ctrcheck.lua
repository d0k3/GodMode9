local version = "5"
local store_log = false

local NAND = "S:/nand.bin"

local status = {
    ["NAND Header"] = false,
    ["NAND Sectors"] = false,
    ["CTRNAND"] = false,
    ["TWLN"] = false,
    ["TWLP"] = false,
    ["FIRM Partitions"] = false
}

local function show_status(current_process)
    local function get_text(requested_process)
        if status[requested_process] then
            return "\n"..requested_process..": DONE"
        elseif current_process == requested_process then
            return "\n"..requested_process..": ~~~"
        else
            return "\n"..requested_process..": ---"
        end
    end

    ui.show_text(
        "ctrcheck v"..version..
        "\nCurrently processing: "..current_process..". Progress:\n "..
        get_text("NAND Header")..
        get_text("NAND Sectors")..
        get_text("CTRNAND")..
        get_text("TWLN")..
        get_text("TWLP")..
        get_text("FIRM Partitions")..
        "\n\ntest"
    )
end

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
    show_status("NAND Header")
    local hdr_hash, part_table_hash, header_magic, success, hdr, stat
    local log = {}
    local is_sighax = false

    -- check whether NAND header signature is sighax, based on hashes
    -- sighax will always be read as valid by boot9 even without cfw, so it's just used to check whether custom partitions should be possible
    success, stat = pcall(fs.stat, "S:/nand_hdr.bin")
    if not success then
        table.insert(log, "Critical: Could not stat S:/nand_hdr.bin!")
        goto fail
    end

    if stat.size ~= 512 then
        table.insert(log, "Error: NAND header is an invalid size (expected 512, got "..tostring(stat.size)..")")
        return
    end

    success, hdr = pcall(fs.read_file, "S:/nand_hdr.bin", 0, 0x200)
    if not success then
        table.insert(log, "Critical: Could not read S:/nand_hdr.bin!")
        goto fail
    end

    hdr_hash = util.bytes_to_hex(fs.hash_data(string.sub(hdr, 1, 0x100)))

    if hdr_hash == "a4ae99b93412e4643e4686987b6cfd59701d5c655ca2ff671ce680b4ddcf0948" then
        table.insert(log, "Information: NAND header's signature is sighax.")
        is_sighax = true
    end

    -- hash-based check of NAND header partition table against retail partition tables
    part_table_hash = util.bytes_to_hex(fs.hash_data(string.sub(hdr, 0x101, 0x160)))

    if part_table_hash == "dfd434b883874d8b585a102f3cf3ae4cef06767801db515fdf694a7e7cd98bc2" then
        table.insert(log, (CONSOLE_TYPE == "N3DS") and "Information: NAND header is stock. (n3DS)" or "Critical: o3DS has an n3DS nand header.")
    elseif part_table_hash == "ae9b6645105f3aec22c2e3ee247715ab302874fca283343c731ca43ea1baa25d" then
        table.insert(log, (CONSOLE_TYPE == "03DS") and "Information: NAND header is stock. (o3DS)" or "Critical: n3DS has an o3DS nand header.")
    else
        -- check for the NCSD magic header, if it's not present then there's definitely something wrong
        header_magic = string.sub(hdr, 0x101, 0x104)
        if header_magic == "NCSD" then
            if is_sighax then
                table.insert(log, "Warning: NAND partition table is modified, but there is sighax in the NAND header.")
            else
                table.insert(log, "Error: NAND partition table is modified, and there is no sighax in the NAND header.")
            end
        else
            -- your NAND has a minor case of serious brain damage
            table.insert(log, "Error: NAND header data is invalid. You've met with a terrible fate, haven't you?")
        end
    end

    ::fail::

    status["NAND Header"] = true

    return table.concat(log, "\n")
end

local function chk_nand_sectors()
    show_status("NAND Sectors")
    local secret_sector, success, secret_sector_hash, sbyte, twlmbr, twlmbr_hash, stage2_byte
    local log = {}

    if CONSOLE_TYPE == "N3DS" then
        success, stat = pcall(fs.stat, "S:/sector0x96.bin")
        if not success then
            table.insert(log, "Critical: Could not stat S:/sector0x96.bin!")
            goto checktwl
        end

        success, secret_sector = pcall(fs.read_file, "S:/sector0x96.bin", 0, 0x200)
        if not success then
            table.insert(log, "Critical: Could not read S:/sector0x96.bin!")
            goto checktwl
        end

        secret_sector_hash = util.bytes_to_hex(fs.hash_data(secret_sector))
        if secret_sector_hash ~= "82f2730d2c2da3f30165f987fdccac5cbab24b4e5f65c981cd7be6f438e6d9d3" then
            table.insert(log, "Warning: Secret Sector data is invalid. a9lh might be installed.")
        end
    else
        success, sbyte = pcall(fs_read_file, NAND, 0x12C00, 2)
        if not success then
            table.insert(log, "Critical: Could not read S:/nand.bin at 0x12C00!")
            goto checktwl
        end
        if string.len(sbyte) ~= 4 then
            table.insert(log, "Critical: NAND is unreadable at offset 0x12C00...?")
        elseif sbyte ~= "0000" and sbyte ~= "ffff" then
            table.insert(log, "Warning: There may be a9lh leftovers in the secret sector.")
        end
    end

    ::checktwl::

    -- verify the TWL MBR exists and is retail (there is no good reason this should ever be modified)
    success, stat = pcall(fs.stat, "S:/twlmbr.bin")
    if not success then
        table.insert(log, "Critical: Could not stat S:/twlmbr.bin!")
        goto checkstage2
    end

    success, twlmbr = pcall(fs.read_file, "S:/twlmbr.bin", 0, 66)
    if not success then
        table.insert(log, "Critical: Could not read S:/twlmbr.bin!")
        goto checkstage2
    end

    twlmbr_hash = util.bytes_to_hex(fs.hash_data(twlmbr))
    if twlmbr_hash ~= "77a98e31f1ff7ec4ef2bfacca5a114a49a70dcf8f1dcd23e7a486973cfd06617" then
        table.insert(log, "Critical: TWL MBR data is invalid.")
    end

    ::checkstage2::

    -- instead of checking the full sector against multiple stage2s, this just checks if the sector is "clean"
    -- (if first byte is not "clean" assume stage2 is there, can be done in a better way)
    -- if stage2 was replaced with trash it would trigger this warning tho
    success, stage2_byte = pcall(fs.read_file, "S:/nand.bin", 0xB800000, 1)
    if not success then
        table.insert(log, "Critical: Could not read S:/nand.bin at 0xB800000!")
        goto checkbonus
    end

    stage2_byte = util.bytes_to_hex(stage2_byte)

    if stage2_byte ~= "00" and stage2_byte ~= "FF" then
        table.insert(log, "Warning: There are likely leftovers from a9lh's stage2 payload.")
    end

    ::checkbonus::

    -- check for presence of bonus drive, just because it might come in handy to know sometimes
    if fs.is_dir("8:/") then
        table.insert(log, "Information: Bonus drive is enabled.")
    end

    ::fail::

    status["NAND Sectors"] = true

    return table.concat(log, "\n")
end

local function chk_ctrnand()
    local log = {}

    table.insert(log, "CTRNAND checks not yet implemented.")

    return table.concat(log, "\n")
end

local function chk_twln()
    local log = {}

    table.insert(log, "TWLN checks not yet implemented.")

    return table.concat(log, "\n")
end

local function chk_twlp()
    local log = {}

    table.insert(log, "TWLP checks not yet implemented.")

    return table.concat(log, "\n")
end

local function nand_checks()
    local nand_log = {}
    
    table.insert(nand_log, chk_nand_header())
    table.insert(nand_log, chk_nand_sectors())
    table.insert(nand_log, chk_ctrnand())
    table.insert(nand_log, chk_twln())
    table.insert(nand_log, chk_twlp())

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
