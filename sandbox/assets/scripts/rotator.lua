function start(entity)
    print("[Lua] Rotator attached to entity ID: " .. entity)
end

function update(entity, dt)
    local reg = get_registry()
    local transform = reg:get_transform(entity)
    
    if transform ~= nil then
        -- Spin the entity at 45 degrees per second
        -- Notice we use 'roty' here because of how you bound it in BindEngine!
        transform.roty = transform.roty + (45.0 * dt)
    end
end