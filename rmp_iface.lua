-- MIT License

-- Copyright (c) 2020 Holger Teutsch

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

local rmp = com.open(port, 115200, 0)

function rmp_data(h, data, len)
    if data:sub(len, len) == "\n" then
        data = data:sub(1, len - 1)
    end
    ipc.log(data)
    if data:sub(1, 1) == "X" then
        local a = tonumber(data:sub(2, 7))
        local s = tonumber(data:sub(8, 13))
  
        ipc.log("a = " .. a .. "  s = " .. s)
        ipc.writeSD(0x05C4, a * 1000)
        ipc.writeSD(0x05CC, s * 1000)
        return
    end
    
    if data:sub(1, 1) == "S" then
        local s = tonumber(data:sub(2, 7))
        ipc.log("s = " .. s)
        ipc.writeSD(0x05CC, s * 1000)
        return
    end
end

function rmp_heartbeat()
    local active = ipc.readSD(0x05C4) / 1000
    local stdby = ipc.readSD(0x05CC) / 1000
    ipc.log("a: " .. active .. " s: " .. stdby)
    msg = string.format("H%03d%03d%03d%03da\n", active / 1000, active % 1000, stdby / 1000, stdby % 1000)
    ipc.log(msg)
    com.write(rmp, msg)
end

function rmp_com1_change(ofs, val)
    ipc.log("com1 " .. val)
    rmp_heartbeat()
end

event.com(rmp, 50, "rmp_data")
event.timer(5 * 1000, "rmp_heartbeat")
event.offset(0x05CC, "SD", "rmp_com1_change")