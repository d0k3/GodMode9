local mf = "0:/luma.firm"

print("Mounted:", fs.get_img_mount())

print("Mounting:", mf)
fs.img_mount(mf)
print("Mounted:", fs.get_img_mount())

ui.echo("There")
