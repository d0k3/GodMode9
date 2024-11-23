local hash256 = fs.hash_data("a")
local hash1 = fs.hash_data("a", {sha1=true})

print("I hashed the letter a")
print("Hash 256: ", util.bytes_to_hex(hash256))
print("Hash 1: ", util.bytes_to_hex(hash1))
print()
print("Hex to bytes:", util.hex_to_bytes("676179"))

ui.echo("Cool")
