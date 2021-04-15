-- MIT License

-- Copyright (c) 2021 Holger Teutsch

-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:

-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.

-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-- rmp_iface.lua

-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ start of customizations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

local port = "COM4"

-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ end of customizations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

local rmp  = com.open(port, 115200, 0)

local ofs_active = 0x05C4
local ofs_stdby = 0x05CC

function rmp_data(h, data, len)
    if data:sub(len, len) == "\n" then
        data = data:sub(1, len - 1)
    end
    ipc.log(data)
    if data:sub(1, 1) == "X" then
        local a = tonumber(data:sub(2, 7))
        local s = tonumber(data:sub(8, 13))
  
        ipc.log("a = " .. a .. "  s = " .. s)
        ipc.writeSD(ofs_active, a * 1000)
        ipc.writeSD(ofs_stdby, s * 1000)
        return
    end
    
    if data:sub(1, 1) == "S" then
        local s = tonumber(data:sub(2, 7))
        ipc.log("s = " .. s)
        ipc.writeSD(ofs_stdby, s * 1000)
        return
    end
end

function rmp_heartbeat()
    local active = ipc.readSD(ofs_active) / 1000
    local stdby = ipc.readSD(ofs_stdby) / 1000
    ipc.log("a: " .. active .. " s: " .. stdby)
    msg = string.format("H%06d%06da\n", active, stdby)
    ipc.log(msg)
    if rmp ~= 0 then
        com.write(rmp, msg)
    end
end

function rmp_com1_change(ofs, val)
    if ofs == ofs_active then ofs = "active" else ofs = "stdby" end
    ipc.log("com1 change-> " .. ofs .. "=" .. val)
    rmp_heartbeat()
end

event.offset(ofs_stdby, "SD", "rmp_com1_change")
event.offset(ofs_active, "SD", "rmp_com1_change")

if rmp ~= 0 then
    event.com(rmp, 50, "rmp_data")
    event.timer(5 * 1000, "rmp_heartbeat")
end

