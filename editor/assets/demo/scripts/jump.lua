local jump_force = 5.0

function update(entity, dt)
    -- Look how clean this is! 
    if Input.action_pressed("Jump") then 
        entity:set_local_velocity(0.0, jump_force, 0.0) 
        entity:play_sound()
        print("Cube Jumped!")
    end
end