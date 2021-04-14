local port = "COM4"
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