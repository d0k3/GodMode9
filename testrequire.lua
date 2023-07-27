j = require('json')
UI.ShowPrompt(false, j.encode({a=1, b=2}))
UI.ShowPrompt(false, j.encode({"one", 2, 3.1}))
