-- M7 listen-server demo. Bots sit far apart so stream distance is visible.

function on_init()
    local a = SpawnPlayer(40.0, 1.0, 0.0)
    SetPlayerName(a, "BotA")
    local b = SpawnPlayer(-35.0, 1.0, 25.0)
    SetPlayerName(b, "BotB")
    local c = SpawnPlayer(0.0, 1.0, 80.0)
    SetPlayerName(c, "BotC")
    SpawnVehicle(8.0, 1.2, 4.0, 90.0)
    SpawnVehicle(42.0, 1.2, 2.0, 0.0)
    SpawnVehicle(0.0, 1.2, 78.0, 180.0)
    CreateMarker(0.0, 0.2, 0.0, 1.3, 0.2, 0.9, 1.0)
    CreateMarker(40.0, 0.2, 0.0, 1.2, 1.0, 0.4, 0.15)
    CreateMarker(0.0, 0.2, 80.0, 1.2, 0.4, 1.0, 0.3)
    Create3DTextLabel("Spawn", 0.0, 2.5, 0.0, 40.0)
    Create3DTextLabel("East camp", 40.0, 2.5, 0.0, 40.0)
    Create3DTextLabel("North camp", 0.0, 2.5, 80.0, 40.0)
end

function on_update(dt)
end
