-- Forge M6 gamemode. SA-MP-style natives, no os/io.

function on_init()
    local me = SpawnPlayer(0.0, 1.0, 0.0)
    SetPlayerName(me, "Player")
    SpawnPlayer(5.0, 1.0, 4.0)
    SpawnPlayer(-4.0, 1.0, 6.0)
    SpawnVehicle(7.0, 1.3, 3.0, 210.0)
    SpawnVehicle(-8.0, 1.3, 9.0, 40.0)
    CreateMarker(0.0, 0.2, 12.0, 1.4, 0.15, 0.85, 1.0)
    CreateMarker(7.0, 0.2, 3.0, 1.1, 1.0, 0.35, 0.15)
    Create3DTextLabel("Forge M6", 0.0, 2.6, 0.0, 28.0)
    Create3DTextLabel("Dealership", 7.0, 2.5, 3.0, 32.0)
    Create3DTextLabel("Checkpoint", 0.0, 2.2, 12.0, 24.0)
end

function on_update(dt)
end
