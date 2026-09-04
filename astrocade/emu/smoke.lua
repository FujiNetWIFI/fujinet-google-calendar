-- smoke.lua: headless live smoke against a running fujinet-pc.
--
-- Launch, then walk the four views (1 day, 2 week, 3 month, 4 agenda) and
-- open a compose form (5). Snapshots after each. Frames, not attotime
-- seconds.
--
-- Keypad->ioport: KEYn maps to :KEYPADn, bit value 1<<bit (KPTAB order).
--   '1'=KEYPAD3 0x10  '2'=KEYPAD2 0x10  '3'=KEYPAD1 0x10
--   '4'=KEYPAD3 0x08  '5'=KEYPAD2 0x08  'C'=KEYPAD3 0x01
--
--   FUJINET_TCP=127.0.0.1:9998 mame ... -autoboot_script emu/smoke.lua
local FPS = 60

local function port_by_suffix(suffix)
    for tag, port in pairs(manager.machine.ioport.ports) do
        if tag:sub(-#suffix) == suffix then return port end
    end
    return nil
end
local function tap(name, field)
    local p = port_by_suffix(name)
    if p then p:field(field):set_value(1) end
end
local function untap(name, field)
    local p = port_by_suffix(name)
    if p then p:field(field):clear_value() end
end

local acts = {}
local function at(sec, fn) acts[#acts + 1] = {math.floor(sec * FPS), fn} end
local function press(sec, name, field)
    at(sec, function() tap(name, field) end)
    at(sec + 0.13, function() untap(name, field) end)
end
local function shot(sec, label)
    at(sec, function()
        emu.print_info("smoke.lua: snapshot (" .. label .. ")")
        manager.machine.video:snapshot()
    end)
end

press(3.0, "KEYPAD3", 0x10)     -- '1': launch from the menu
shot(14.0, "day live")
press(16.0, "KEYPAD2", 0x10)    -- '2': the week strip
shot(24.0, "week live")
press(26.0, "KEYPAD3", 0x08)    -- '4': agenda
shot(34.0, "agenda live")
press(36.0, "KEYPAD1", 0x10)    -- '3': month
shot(44.0, "month live")
press(46.0, "KEYPAD2", 0x08)    -- '5': compose (blank form)
shot(48.0, "compose form")
press(50.0, "KEYPAD3", 0x01)    -- 'C': leave the form (nothing dirty)
shot(52.0, "back to month")

table.sort(acts, function(a, b) return a[1] < b[1] end)
local i, frame = 1, 0
emu.register_frame(function()
    frame = frame + 1
    while i <= #acts and frame >= acts[i][1] do
        acts[i][2]()
        i = i + 1
    end
end)
