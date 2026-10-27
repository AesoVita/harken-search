-- Aura_InsertMedia.lua
-- REAPER Script: reads file path from ExtState and inserts at cursor on selected track.
-- Install: copy to REAPER/Scripts/, then Actions > Load ReaScript.
-- Aura triggers this via REAPER Web Interface HTTP API.

local section = "AuraSend"
local key = "filepath"

local filepath = reaper.GetExtState(section, key)
if filepath == nil or filepath == "" then
    reaper.ShowConsoleMsg("Aura: No file path in ExtState\n")
    return
end

-- Clear the ExtState so we don't re-insert on accidental re-trigger
reaper.SetExtState(section, key, "", false)

-- Get selected track, or first track if none selected
local track = reaper.GetSelectedTrack(0, 0)
if track == nil then
    -- No track selected, try track 0
    track = reaper.GetTrack(0, 0)
    if track == nil then
        -- No tracks at all, insert one
        reaper.InsertTrackAtIndex(0, true)
        track = reaper.GetTrack(0, 0)
    end
    reaper.SetOnlyTrackSelected(track)
end

-- Insert media at edit cursor position on the selected track
-- Mode 0 = add to current track at cursor
reaper.InsertMedia(filepath, 0)

reaper.UpdateArrange()
