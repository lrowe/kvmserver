local net = require("@lune/net")
local process = require("@lune/process")
local stdio = require("@lune/stdio")
local task = require("@lune/task")

local PORT = 8000
local URL = `http://127.0.0.1:{PORT}`
local RESPONSE = "Hello, World!"

-- Serve should get proper path, query, and other request information

local handle = net.serve(PORT, function(request)
	print("Got a request from", request.ip, "on port", request.port)

	return RESPONSE
end)
