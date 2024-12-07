local num1 = ui.ask_hex("Gimmie a hex number.", 621, 8)
local num2 = ui.ask_number("Gimmie a normal number.", 123)
local text = ui.ask_text("Enter something!", "initial prompt", 20)

print("First number", num1)
print("Second number", num2)
print("And the text:", text)

ui.echo("Cool!!!!")
ui.show_png("0:/ihaveahax.png")
ui.echo("PNG time")
--ui.show_text("testing")
--ui.echo("Text time")
