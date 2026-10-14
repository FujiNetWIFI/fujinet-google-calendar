-- nes-shot.lua -- MAME autoboot script for tools/nes-shot.sh.
--
-- Presses pad buttons on a schedule, then prints the screen as text and
-- saves a snapshot. The client draws on two name tables and flips between
-- them (src/nes/screen.c), so the table read is the one _ppu_ctrl's bit 0
-- says is on show -- its zero-page address comes from the map.
--
--   SHOT_PRESS    "secs:Button secs:Button ..."  (Button: A B Select Start
--                 Up Down Left Right, pressed for 6 frames; or K_<key> for a
--                 Family BASIC keyboard key -- K_F3, K_N, K_Esc, K_Enter,
--                 K_Space -- when MAME runs the famicom with -exp fc_keyboard)
--   SHOT_AT       seconds before the capture (default 6)
--   SHOT_CTRL     _ppu_ctrl's address, hex (tools/nes-shot.sh finds it)
--   SHOT_EVERY    also print the screen at each press (debugging a script)

local at = tonumber(os.getenv("SHOT_AT") or "6")
local ctrl = tonumber(os.getenv("SHOT_CTRL") or "0", 16)
local every = os.getenv("SHOT_EVERY")
local presses = {}

for t, b in string.gmatch(os.getenv("SHOT_PRESS") or "", "([%d%.]+):([%w_]+)") do
  presses[#presses + 1] = { t = tonumber(t), b = b, state = 0 }
end

local port = manager.machine.ioport.ports[":ctrl1:joypad:JOYPAD"]
local function field(b)
  local key = b:match("^K_(.+)$")
  if not key then return port.fields["P1 " .. b] end
  for tag, p in pairs(manager.machine.ioport.ports) do
    if tag:find("fc_keyboard", 1, true) and p.fields[key] then return p.fields[key] end
  end
  error("no keyboard key " .. key)
end

local ppu = manager.machine.devices[":ppu"].spaces["videoram"]
local cpu = manager.machine.devices[":maincpu"].spaces["program"]

-- The icon tiles as something readable.
local icons = {
  [0x01] = "-", [0x02] = ":", [0x03] = "#", [0x04] = "<", [0x05] = ">",
  [0x06] = "^", [0x07] = "v", [0x08] = "~", [0x09] = "*", [0x0A] = "!",
  [0x0B] = "@", [0x0C] = "P", [0x0D] = "/", [0x0E] = "C", [0x0F] = ".",
  [0x10] = "A", [0x11] = "B", [0x12] = "(", [0x13] = ")", [0x14] = "+",
  [0x15] = "<", [0x16] = "^", [0x17] = "^", [0x18] = "J", [0x19] = "_",
  [0x1A] = "<", [0x1B] = ">", [0x1C] = ".", [0x1D] = " ", [0x1E] = "_",
  [0x7F] = "#", [0x80] = " ", [0x98] = "<", [0x99] = ">", [0x9A] = ".",
  [0x9B] = " ", [0x9C] = "[", [0x9D] = "]", [0x9E] = "[", [0x9F] = "]",
}

local function ch(v)
  if icons[v] then return icons[v] end
  if v >= 0x81 and v <= 0x83 then return tostring(v - 0x80) end
  if v >= 0x84 and v <= 0x8D then return tostring(v - 0x84) end
  if v >= 0x8E and v <= 0x97 then return tostring(v - 0x8E) end
  local c = v & 0x7F
  if c >= 0x20 and c < 0x7F then return string.char(c) end
  return " "
end

local function screen()
  local base = ((cpu:readv_u8(ctrl) & 1) == 1) and 0x2400 or 0x2000
  local out = {}
  for r = 0, 29 do
    local t, hl = {}, false
    for c = 0, 31 do
      local v = ppu:readv_u8(base + r * 32 + c)
      if v >= 0xA0 then hl = true end
      t[#t + 1] = ch(v)
    end
    out[#out + 1] = string.format("%2d%s|%s|", r, hl and "*" or " ", table.concat(t))
  end
  return table.concat(out, "\n")
end

local done = false
_G.__shot_sub = emu.add_machine_frame_notifier(function()
  if done then return end
  local now = manager.machine.time:as_double()
  for _, p in ipairs(presses) do
    if p.state == 0 and now >= p.t then
      if every then print(string.format("-- %.2fs before %s\n%s", now, p.b, screen())) end
      field(p.b):set_value(1)
      p.state = 1
      p.frames = 6
    elseif p.state == 1 then
      p.frames = p.frames - 1
      if p.frames <= 0 then
        field(p.b):set_value(0)
        p.state = 2
      end
    end
  end
  if now >= at then
    done = true
    print(screen())
    local scr = manager.machine.screens[":screen"]
    if scr then scr:snapshot() end
    manager.machine:exit()
  end
end)
