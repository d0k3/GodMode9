local sd_stat = fs.stat_fs("0:/")
print("Total size:", ui.format_bytes(sd_stat.total))
print("Free space:", ui.format_bytes(sd_stat.free))
