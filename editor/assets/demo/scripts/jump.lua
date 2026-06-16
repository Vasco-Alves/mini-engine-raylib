local jump_force = 5.0

-- Grounded check via collision callbacks: count touching bodies instead of
-- using a boolean, so sliding off one contact while still on another works.
local contacts = 0

function on_collision_enter(self, other)
    contacts = contacts + 1
end

function on_collision_exit(self, other)
    contacts = contacts - 1
end

function update(entity, dt)
    if contacts > 0 and Input.action_pressed("Jump") then
        entity:set_local_velocity(0.0, jump_force, 0.0)
        entity:play_sound()
        print("Cube Jumped!")
    end
end
