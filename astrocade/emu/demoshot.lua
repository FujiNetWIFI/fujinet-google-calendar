-- demoshot.lua: screenshot pass over the DEMO=1 mock screens.
--
-- Keypad 1 launches the cart at the OS menu; the splash shows. Keypad =
-- then steps splash -> day -> month -> form, and inside the form's editor
-- = accepts and returns to the splash. A snapshot lands after each step:
-- 0000 splash, 0001 day, 0002 month, 0003 form + grid keyboard.
--
-- Everything is scheduled in FRAMES: manager.machine.time.seconds is an
-- attotime's integer seconds field, and a fractional threshold against it
-- silently turns a tap into a one-second hold.
--
--   mame ... -autoboot_script emu/demoshot.lua -video none -sound none \
--        -seconds_to_run 30 -snapshot_directory build
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
local function press(sec, name, field)         -- ~8 frames: a single tap
    at(sec, function() tap(name, field) end)
    at(sec + 0.13, function() untap(name, field) end)
end
local function shot(sec, label)
    at(sec, function()
        emu.print_info("demoshot.lua: snapshot (" .. label .. ")")
        manager.machine.video:snapshot()
    end)
end

press(3.0, "KEYPAD3", 0x10)                    -- keypad 1: launch from the menu
shot(6.0, "splash")
press(8.0, "KEYPAD0", 0x20)                    -- keypad =: step to the day view
shot(10.0, "day")
press(12.0, "KEYPAD0", 0x20)                   -- month
shot(14.0, "month")
press(16.0, "KEYPAD0", 0x20)                   -- form; the editor is live
shot(18.0, "form")
press(20.0, "KEYPAD0", 0x20)                   -- = accepts: back to the splash
shot(22.0, "splash again")

table.sort(acts, function(a, b) return a[1] < b[1] end)

local i, frame = 1, 0
emu.register_frame(function()
    frame = frame + 1
    while i <= #acts and frame >= acts[i][1] do
        acts[i][2]()
        i = i + 1
    end
end)
