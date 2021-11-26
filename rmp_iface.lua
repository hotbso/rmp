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

-- empty

-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ end of customizations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

_, _, acf_model = string.find(ipc.readSTR(0x3500, 24), "([%a%d]+)")
ipc.log("ACF model: '" .. acf_model .. "'")

local trim_inc = 0  -- default use trim up/down control
if acf_model == "AC11" then
    trim_inc = 350
elseif acf_model == "Optica" then
    trim_inc = 200
end

local script_directory = debug.getinfo(1, "S").source:sub(2)
script_directory = script_directory:match("(.*[/\\])")

local f = io.open(script_directory .. "rmp_iface.cfg")
local port = f:read("*line")
f:close()

ipc.log("port: " .. port)
local rmp  = com.open(port, 115200, 0)

local ofs_active = 0x05C4
local ofs_stdby = 0x05CC
local ofs_trim = 0x0BC0

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
        -- ipc.log("s = " .. s)
        ipc.writeSD(ofs_stdby, s * 1000)
        return
    end

    if data:sub(1, 2) == "TD" then
        -- ipc.log("TD")
        if trim_inc == 0 then
            ipc.control(65607) -- trim down
        else
            local tpos = ipc.readSW(ofs_trim)
            tpos = tpos - trim_inc
            if tpos < -16383 then tpos = -16383 end
            ipc.writeSW(ofs_trim, tpos)
            ipc.log("tpos = " .. tpos)
        end
        return
    end

    if data:sub(1, 2) == "TU" then
        -- ipc.log("TU")
        if trim_inc == 0 then
            ipc.control(65615) -- trim up
        else
            local tpos = ipc.readSW(ofs_trim)
            tpos = tpos + trim_inc
            if tpos > 16383 then tpos = 16383 end
            ipc.writeSW(ofs_trim, tpos)
            ipc.log("tpos = " .. tpos)
        end
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

event.com(rmp, 150, 1, 10, "rmp_data")
event.timer(5 * 1000, "rmp_heartbeat")


