local AbilitySystem = require("AbilitySystem")
local HaruUltimateCutscene = require("HaruUltimateCutscene")

-- Dash
local DASH_SKILL_NAME = "Dash"
local DASH_SKILL_KEY = "LeftShift"
local DASH_SKILL_KEYS = { DASH_SKILL_KEY, "GamepadLeftTrigger" }
local DASH_DISTANCE = 5.0
local DASH_DURATION = 0.5
local DASH_AFTERIMAGE_INTENSITY = 1.0
local DASH_AFTERIMAGE_RADIUS = 128.0
local DASH_AFTERIMAGE_SAMPLES = 64
-- Roll
local ROLL_SKILL_NAME = "Roll"
local ROLL_SKILL_KEY = "LeftControl"
local ROLL_DISTANCE = 12.0
local ROLL_DURATION = 1.2
-- Arrow
local BOW_SKILL_NAME = "AirBowShot"
local BOW_SKILL_KEY = "RightMouseButton"
local BOW_AIM_GRAVITY = 0.1
local BOW_AIM_MAX_DURATION = 60.0

local BOW_CAMERA_ARM_LENGTH = 3.0
local BOW_CAMERA_SOCKET_OFFSET = Vec3(0.0, 0.5, 1.0)
local BOW_CAMERA_FOV = 0.55
local BOW_CAMERA_BLEND_DURATION = 0.3
local BOW_CAMERA_RESTORE_DURATION = 0.22

local BOW_RADIAL_BLUR_INTENSITY = 1.0
local BOW_RADIAL_BLUR_RADIUS = 0.06
local BOW_RADIAL_BLUR_SAMPLE_COUNT = 22

local BOW_PROJECTILE_SPEED = 15.0
local BOW_PROJECTILE_OFFSET = Vec3(0.1, 0.15, 0.3)

local BOW_AIM_PARTICLE_PATH = "Content/Particle System/Aim.uasset"
local BOW_AIM_PARTICLE_OFFSET = Vec3(1.5, 0.15, 0.3)

local BOW_RELEASE_SLOMO_DURATION = 0.3
local BOW_RELEASE_SLOMO_DILATION = 0.1

local BOW_ULTIMATE_IGNORE_GAUGE_FOR_TEST = false
local STAFF_STATIC_MESH_PATH = "Content/Data/Staff/Staff_StaticMesh.uasset"
local BOW_STATIC_MESH_PATH = "Content/Data/Bow/Bow_StaticMesh.uasset"
-- Attack
local ATTACK_SKILL_NAME = "SprayAttack"
local FIRE_PROJECTILE_KEY = "LeftMouseButton"
local ATTACK_MAX_DURATION = 60.0
local SPRAY_TARGET_ACTOR_NAME = "Boss"
local SPRAY_FACE_BLEND_DURATION = 0.25
-- Weapon Setting
local STAFF_WEAPON_TRANSFORM = {
    location = Vec3(0.0, 40.0, 0.0),
    rotation = Vec3(-90.0, 0.0, 180.0),
    scale = Vec3(25.0, 25.0, 25.0)
}
local BOW_WEAPON_TRANSFORM = {
    location = Vec3(7.0, 50.0, 25.0),
    rotation = Vec3(-90.0, 0.0, 170.0),
    scale = Vec3(25.0, 25.0, 25.0)
}

local bow_aim_particle_component = nil
local DASH_ANIM_VAR = "Dash"
local ROLL_ANIM_VAR = "Roll"
local ATTACK_ANIM_VAR = "Attack"
local ARROW_ANIM_VAR = "Arrow"
local WALKING_AUDIO_LOOP_NAME = "HaruWalking"
local WALKING_AUDIO_MIN_SPEED = 0.05

local actor = nil
local ability_system = nil
local movement = nil
local anim_instance = nil
local dash_trail_particle = nil
local spring_arm = nil
local camera = nil
local weapon_mesh = nil
local camera_blend = nil
local movement_locks = {}
local walking_audio_playing = false

local function log(message)
    print("[HaruController] " .. message)
end

local function join_keys(keys)
    if keys == nil then
        return ""
    end
    return table.concat(keys, ", ")
end

local function first_pressed_key(keys)
    if Input == nil or Input.GetKeyDown == nil or keys == nil then
        return nil
    end

    for index = 1, #keys do
        local key = keys[index]
        if key ~= nil and Input.GetKeyDown(key) then
            return key
        end
    end

    return nil
end

local function format_vec3(value)
    if value == nil then
        return "nil"
    end

    return string.format("(%.3f, %.3f, %.3f)", value.X or 0.0, value.Y or 0.0, value.Z or 0.0)
end

local function clamp01(value)
    if value == nil or value < 0.0 then
        return 0.0
    end
    if value > 1.0 then
        return 1.0
    end
    return value
end

local function smoothstep(value)
    local t = clamp01(value)
    return t * t * (3.0 - 2.0 * t)
end

local function get_ultimate_gauge(owner)
    if World ~= nil and World.GetPlayerUltimateGauge ~= nil then
        return World.GetPlayerUltimateGauge(owner)
    end
    return 0.0
end

local function get_ultimate_gauge_max(owner)
    if World ~= nil and World.GetPlayerUltimateGaugeMax ~= nil then
        return World.GetPlayerUltimateGaugeMax(owner)
    end
    return 100.0
end

local function is_ultimate_ready(owner)
    if World ~= nil and World.IsPlayerUltimateReady ~= nil then
        return World.IsPlayerUltimateReady(owner)
    end
    return get_ultimate_gauge(owner) >= get_ultimate_gauge_max(owner)
end

local function set_player_damage_enabled(owner, enabled, reason)
    if World ~= nil and World.SetPlayerDamageEnabled ~= nil then
        local ok = World.SetPlayerDamageEnabled(owner, enabled == true)
        log("Player damage " .. (enabled and "enabled" or "disabled")
            .. " reason=" .. tostring(reason)
            .. " ok=" .. tostring(ok))
        return ok
    end

    log("Player damage toggle unavailable reason=" .. tostring(reason))
    return false
end

local function set_bow_radial_blur(enabled, reason)
    if World == nil then
        return false
    end

    if enabled then
        if World.SetCameraRadialBlur ~= nil then
            local ok = World.SetCameraRadialBlur(
                BOW_RADIAL_BLUR_INTENSITY,
                BOW_RADIAL_BLUR_RADIUS,
                BOW_RADIAL_BLUR_SAMPLE_COUNT,
                0.5,
                0.5)
            log("AirBowShot radial blur enabled reason=" .. tostring(reason) .. " ok=" .. tostring(ok))
            return ok
        end
    elseif World.ClearCameraRadialBlur ~= nil then
        local ok = World.ClearCameraRadialBlur()
        log("AirBowShot radial blur cleared reason=" .. tostring(reason) .. " ok=" .. tostring(ok))
        return ok
    end

    log("AirBowShot radial blur unavailable reason=" .. tostring(reason))
    return false
end

local function lerp_number(a, b, alpha)
    if a == nil then
        return b
    end
    if b == nil then
        return a
    end
    return a + (b - a) * alpha
end

local function normalize_angle_delta(angle)
    local result = angle or 0.0
    while result > 180.0 do
        result = result - 360.0
    end
    while result < -180.0 do
        result = result + 360.0
    end
    return result
end

local function lerp_angle(a, b, alpha)
    if a == nil then
        return b
    end
    if b == nil then
        return a
    end
    return a + normalize_angle_delta(b - a) * alpha
end

local function lerp_vec3(a, b, alpha)
    if a == nil then
        return b
    end
    if b == nil then
        return a
    end
    return Vec3(
        lerp_number(a.X or 0.0, b.X or 0.0, alpha),
        lerp_number(a.Y or 0.0, b.Y or 0.0, alpha),
        lerp_number(a.Z or 0.0, b.Z or 0.0, alpha)
    )
end

local function resolve_actor()
    if actor ~= nil then
        return actor
    end

    if obj == nil then
        return nil
    end

    if obj.GetOwner ~= nil then
        actor = obj:GetOwner()
    else
        actor = obj
    end

    return actor
end

local function find_component_by_class(owner, class_name)
    if owner == nil or owner.GetComponents == nil then
        return nil
    end

    local components = owner:GetComponents()
    if components == nil then
        return nil
    end

    for index = 1, #components do
        local component = components[index]
        if component ~= nil and component.IsA ~= nil and component:IsA(class_name) then
            return component
        end
    end

    return nil
end

local function has_character_movement_api(candidate)
    return candidate ~= nil
        and candidate.GetMovementMode ~= nil
        and candidate.IsFalling ~= nil
        and (candidate.GetVelocity ~= nil or candidate.GetVelocityValue ~= nil)
end

local function get_character_movement(owner)
    if has_character_movement_api(movement) then
        return movement
    end

    movement = nil

    if owner ~= nil and owner.GetCharacterMovement ~= nil then
        movement = owner:GetCharacterMovement()
    end

    if not has_character_movement_api(movement) and owner ~= nil and owner.GetCharacterMovementComponent ~= nil then
        movement = owner:GetCharacterMovementComponent()
    end

    if not has_character_movement_api(movement) then
        movement = find_component_by_class(owner, "UCharacterMovementComponent")
    end

    return movement
end

local function get_dash_trail_particle(owner)
    if dash_trail_particle ~= nil then
        return dash_trail_particle
    end

    if owner ~= nil and owner.GetParticleSystemComponent ~= nil then
        dash_trail_particle = owner:GetParticleSystemComponent()
    end

    if dash_trail_particle == nil then
        dash_trail_particle = find_component_by_class(owner, "UParticleSystemComponent")
    end

    return dash_trail_particle
end

local function get_spring_arm(owner)
    if spring_arm ~= nil then
        return spring_arm
    end

    if owner ~= nil and owner.GetSpringArmComponent ~= nil then
        spring_arm = owner:GetSpringArmComponent()
    end

    if spring_arm == nil then
        spring_arm = find_component_by_class(owner, "USpringArmComponent")
    end

    return spring_arm
end

local function get_camera(owner)
    if camera ~= nil then
        return camera
    end

    if owner ~= nil and owner.GetCameraComponent ~= nil then
        camera = owner:GetCameraComponent()
    end

    if camera == nil and owner ~= nil and owner.GetCamera ~= nil then
        camera = owner:GetCamera()
    end

    if camera == nil then
        camera = find_component_by_class(owner, "UCameraComponent")
    end

    return camera
end

local function get_weapon_mesh(owner)
    if weapon_mesh ~= nil then
        return weapon_mesh
    end

    local fallback = nil
    if owner ~= nil and owner.GetStaticMeshComponent ~= nil then
        fallback = owner:GetStaticMeshComponent()
    end

    if owner ~= nil and owner.GetComponents ~= nil then
        local components = owner:GetComponents()
        if components ~= nil then
            for index = 1, #components do
                local component = components[index]
                if component ~= nil and component.IsA ~= nil and component:IsA("UStaticMeshComponent") then
                    if fallback == nil then
                        fallback = component
                    end

                    local mesh_path = ""
                    if component.GetMeshPath ~= nil then
                        mesh_path = tostring(component:GetMeshPath())
                    elseif component.MeshPath ~= nil then
                        mesh_path = tostring(component.MeshPath)
                    end

                    if string.find(mesh_path, "Staff_StaticMesh", 1, true) ~= nil
                        or string.find(mesh_path, "Bow_StaticMesh", 1, true) ~= nil then
                        weapon_mesh = component
                        return weapon_mesh
                    end
                end
            end
        end
    end

    weapon_mesh = fallback
    return weapon_mesh
end

local function apply_weapon_transform(mesh, transform, label)
    if mesh == nil or transform == nil then
        return
    end

    if transform.location ~= nil then
        if mesh.RelativeLocation ~= nil then
            mesh.RelativeLocation = transform.location
        elseif mesh.SetLocation ~= nil then
            mesh:SetLocation(transform.location)
        end
    end

    if transform.rotation ~= nil then
        if mesh.Rotation ~= nil then
            mesh.Rotation = transform.rotation
        elseif mesh.SetRotation ~= nil then
            mesh:SetRotation(transform.rotation)
        end
    end

    if transform.scale ~= nil then
        if mesh.RelativeScale ~= nil then
            mesh.RelativeScale = transform.scale
        end
    end

    log("weapon transform applied: " .. tostring(label)
        .. " location=" .. format_vec3(transform.location)
        .. " rotation=" .. format_vec3(transform.rotation)
        .. " scale=" .. format_vec3(transform.scale))
end

local function set_weapon_mesh(owner, mesh_path, label, transform)
    local mesh = get_weapon_mesh(owner)
    if mesh == nil or mesh.SetStaticMeshByPath == nil then
        log("weapon swap skipped: StaticMeshComponent unavailable label=" .. tostring(label))
        return false
    end

    local ok, result = pcall(function()
        return mesh:SetStaticMeshByPath(mesh_path)
    end)

    if ok and result then
        log("weapon swapped: " .. tostring(label) .. " path=" .. tostring(mesh_path))
        apply_weapon_transform(mesh, transform, label)
        return true
    end

    log("weapon swap failed: " .. tostring(label) .. " result=" .. tostring(result))
    return false
end

local function get_camera_state(owner)
    local arm = get_spring_arm(owner)
    local cam = get_camera(owner)

    return {
        arm_length = arm ~= nil and arm.GetTargetArmLength ~= nil and arm:GetTargetArmLength() or nil,
        socket_offset = arm ~= nil and arm.GetSocketOffset ~= nil and arm:GetSocketOffset() or nil,
        inherit_pitch = arm ~= nil and arm.GetInheritPitch ~= nil and arm:GetInheritPitch() or nil,
        inherit_yaw = arm ~= nil and arm.GetInheritYaw ~= nil and arm:GetInheritYaw() or nil,
        inherit_roll = arm ~= nil and arm.GetInheritRoll ~= nil and arm:GetInheritRoll() or nil,
        camera_rotation = cam ~= nil and cam.GetRotation ~= nil and cam:GetRotation() or nil,
        fov = cam ~= nil and cam.GetFOV ~= nil and cam:GetFOV() or nil
    }
end

local function apply_camera_state(owner, state)
    if state == nil then
        return
    end

    local arm = get_spring_arm(owner)
    if arm ~= nil then
        if state.arm_length ~= nil and arm.SetTargetArmLength ~= nil then
            arm:SetTargetArmLength(state.arm_length)
        end
        if state.socket_offset ~= nil and arm.SetSocketOffset ~= nil then
            arm:SetSocketOffset(state.socket_offset)
        end
        if state.inherit_pitch ~= nil and arm.SetInheritPitch ~= nil then
            arm:SetInheritPitch(state.inherit_pitch)
        end
        if state.inherit_yaw ~= nil and arm.SetInheritYaw ~= nil then
            arm:SetInheritYaw(state.inherit_yaw)
        end
        if state.inherit_roll ~= nil and arm.SetInheritRoll ~= nil then
            arm:SetInheritRoll(state.inherit_roll)
        end
        if arm.ResetLagState ~= nil then
            arm:ResetLagState()
        end
    end

    local cam = get_camera(owner)
    if cam ~= nil and state.camera_rotation ~= nil and cam.SetRotation ~= nil then
        cam:SetRotation(state.camera_rotation)
    end
    if cam ~= nil and state.fov ~= nil and cam.SetFOV ~= nil then
        cam:SetFOV(state.fov)
    end
end

local function start_camera_blend(owner, target_state, duration, reason)
    local from_state = get_camera_state(owner)
    camera_blend = {
        owner = owner,
        elapsed = 0.0,
        duration = duration or 0.0,
        reason = reason,
        from = from_state,
        target = target_state
    }

    log("camera blend started: " .. tostring(reason)
        .. " duration=" .. tostring(camera_blend.duration)
        .. " fromArm=" .. tostring(from_state.arm_length)
        .. " targetArm=" .. tostring(target_state ~= nil and target_state.arm_length or nil))

    if camera_blend.duration <= 0.0 then
        apply_camera_state(owner, target_state)
        camera_blend = nil
    end
end

local function tick_camera_blend(dt)
    if camera_blend == nil then
        return
    end

    camera_blend.elapsed = camera_blend.elapsed + (dt or 0.0)
    local alpha = smoothstep(camera_blend.elapsed / camera_blend.duration)
    local state = {
        arm_length = lerp_number(camera_blend.from.arm_length, camera_blend.target.arm_length, alpha),
        socket_offset = lerp_vec3(camera_blend.from.socket_offset, camera_blend.target.socket_offset, alpha),
        inherit_pitch = camera_blend.target.inherit_pitch,
        inherit_yaw = camera_blend.target.inherit_yaw,
        inherit_roll = camera_blend.target.inherit_roll,
        camera_rotation = camera_blend.target.camera_rotation,
        fov = lerp_number(camera_blend.from.fov, camera_blend.target.fov, alpha)
    }

    apply_camera_state(camera_blend.owner, state)

    if alpha >= 1.0 then
        apply_camera_state(camera_blend.owner, camera_blend.target)
        log("camera blend ended: " .. tostring(camera_blend.reason))
        camera_blend = nil
    end
end

local function reset_dash_trail(owner)
    local particle = get_dash_trail_particle(owner)
    if particle == nil then
        return
    end

    if particle.SetAutoActivate ~= nil then
        particle:SetAutoActivate(false)
    end

    if particle.SetResetOnActivate ~= nil then
        particle:SetResetOnActivate(true)
    end

    if particle.Deactivate ~= nil then
        particle:Deactivate()
    end

    if particle.ResetParticles ~= nil then
        particle:ResetParticles()
    end
end

local function start_dash_trail(owner)
    local particle = get_dash_trail_particle(owner)
    if particle == nil then
        return
    end

    local ok, err = pcall(function()
        if particle.Deactivate ~= nil then
            particle:Deactivate()
        end
        if particle.Activate ~= nil then
            particle:Activate(true)
        elseif particle.ResetParticles ~= nil then
            particle:ResetParticles()
        end
    end)

    if not ok then
        log("Dash trail start failed: " .. tostring(err))
    end
end

local function stop_dash_trail(owner)
    local particle = get_dash_trail_particle(owner)
    if particle ~= nil and particle.Deactivate ~= nil then
        particle:Deactivate()
    end
end

local function set_owner_movement_blocked(owner, blocked)
    if owner ~= nil and owner.SetCharacterMovementInputBlocked ~= nil then
        owner:SetCharacterMovementInputBlocked(blocked)
        return true
    end

    local character_movement = get_character_movement(owner)
    if character_movement ~= nil and character_movement.SetMovementInputBlocked ~= nil then
        character_movement:SetMovementInputBlocked(blocked)
        return true
    end

    return false
end

local function stop_owner_movement(owner)
    if owner ~= nil and owner.StopCharacterMovementImmediately ~= nil then
        owner:StopCharacterMovementImmediately()
        return true
    end

    local character_movement = get_character_movement(owner)
    if character_movement ~= nil and character_movement.StopMovementImmediately ~= nil then
        character_movement:StopMovementImmediately()
        return true
    end

    return false
end

local function get_movement_debug_state(character_movement)
    if character_movement == nil then
        return "movement=nil"
    end

    local name = "unknown"
    if character_movement.GetName ~= nil then
        name = tostring(character_movement:GetName())
    end

    local mode = "nil"
    if character_movement.GetMovementMode ~= nil then
        mode = tostring(character_movement:GetMovementMode())
    end

    local is_falling = "unavailable"
    if character_movement.IsFalling ~= nil then
        is_falling = tostring(character_movement:IsFalling())
    end

    local velocity = nil
    if character_movement.GetVelocity ~= nil then
        velocity = character_movement:GetVelocity()
    elseif character_movement.GetVelocityValue ~= nil then
        velocity = character_movement:GetVelocityValue()
    end

    return "movement=" .. name
        .. " mode=" .. mode
        .. " isFalling=" .. is_falling
        .. " velocity=" .. format_vec3(velocity)
end

local function get_movement_velocity(character_movement)
    if character_movement == nil then
        return nil
    end

    if character_movement.GetVelocity ~= nil then
        return character_movement:GetVelocity()
    end

    if character_movement.GetVelocityValue ~= nil then
        return character_movement:GetVelocityValue()
    end

    return nil
end

local function is_character_walking(character_movement)
    local velocity = get_movement_velocity(character_movement)
    if velocity == nil then
        return false
    end

    local horizontal_speed_sq = (velocity.X or 0.0) * (velocity.X or 0.0)
        + (velocity.Y or 0.0) * (velocity.Y or 0.0)
    if horizontal_speed_sq <= WALKING_AUDIO_MIN_SPEED * WALKING_AUDIO_MIN_SPEED then
        return false
    end

    return character_movement.IsFalling == nil or not character_movement:IsFalling()
end

local function update_walking_audio(owner)
    local character_movement = get_character_movement(owner)
    local should_play = is_character_walking(character_movement)

    if should_play then
        if Audio ~= nil and Audio.PlayLoop ~= nil then
            Audio.PlayLoop("Walking", WALKING_AUDIO_LOOP_NAME, 0.8, 1.0)
            walking_audio_playing = true
        end
        return
    end

    if walking_audio_playing and Audio ~= nil and Audio.StopLoop ~= nil then
        Audio.StopLoop(WALKING_AUDIO_LOOP_NAME)
        walking_audio_playing = false
    end
end

local function stop_walking_audio()
    if Audio ~= nil and Audio.StopLoop ~= nil then
        Audio.StopLoop(WALKING_AUDIO_LOOP_NAME)
    end
    walking_audio_playing = false
end

local function is_bow_airborne(character_movement)
    if character_movement == nil then
        return false
    end

    if character_movement.IsFalling ~= nil and character_movement:IsFalling() then
        return true
    end

    if character_movement.GetMovementMode ~= nil and character_movement:GetMovementMode() == 1 then
        return true
    end

    local velocity = nil
    if character_movement.GetVelocity ~= nil then
        velocity = character_movement:GetVelocity()
    elseif character_movement.GetVelocityValue ~= nil then
        velocity = character_movement:GetVelocityValue()
    end

    return velocity ~= nil and (velocity.Z or 0.0) > 0.05
end

local function start_dash_afterimage(owner, forward, duration)
    if owner == nil or owner.StartAfterImage == nil then
        return
    end

    local ok, started = pcall(function()
        return owner:StartAfterImage(
            forward,
            duration,
            DASH_AFTERIMAGE_INTENSITY,
            DASH_AFTERIMAGE_RADIUS,
            DASH_AFTERIMAGE_SAMPLES
        )
    end)

    if not ok then
        log("Dash afterimage failed: " .. tostring(started))
    end
end

local function has_movement_locks()
    for _, locked in pairs(movement_locks) do
        if locked then
            return true
        end
    end
    return false
end

local function lock_movement(owner, reason)
    movement_locks[reason] = true
    stop_owner_movement(owner)
    if not set_owner_movement_blocked(owner, true) then
        log("movement lock failed: CharacterMovementComponent blocking API unavailable reason=" .. tostring(reason))
    end
end

local function lock_movement_input_only(owner, reason)
    movement_locks[reason] = true
    if not set_owner_movement_blocked(owner, true) then
        log("movement input lock failed: CharacterMovementComponent blocking API unavailable reason=" .. tostring(reason))
    end
end

local function unlock_movement(owner, reason)
    movement_locks[reason] = nil
    if not has_movement_locks() then
        set_owner_movement_blocked(owner, false)
    end
end

local function get_actor_location_safe(target)
    if target == nil then
        return nil
    end

    local ok, location = pcall(function()
        return target.Location
    end)

    if ok then
        return location
    end

    return nil
end

local function set_actor_location_safe(target, location)
    if target == nil or location == nil then
        return false
    end

    local ok, err = pcall(function()
        target.Location = location
    end)

    if not ok then
        log("actor location lock failed: " .. tostring(err))
        return false
    end

    return true
end

local function set_actor_horizontal_location_locked(target, locked_location)
    if target == nil or locked_location == nil then
        return false
    end

    local current_location = get_actor_location_safe(target)
    if current_location == nil then
        return false
    end

    return set_actor_location_safe(target, Vec3(
        locked_location.X or current_location.X or 0.0,
        locked_location.Y or current_location.Y or 0.0,
        current_location.Z or locked_location.Z or 0.0
    ))
end

local function atan2_degrees(y, x)
    local radians = 0.0
    if math.atan2 ~= nil then
        radians = math.atan2(y, x)
    elseif x > 0.0 then
        radians = math.atan(y / x)
    elseif x < 0.0 and y >= 0.0 then
        radians = math.atan(y / x) + math.pi
    elseif x < 0.0 then
        radians = math.atan(y / x) - math.pi
    elseif y > 0.0 then
        radians = math.pi * 0.5
    elseif y < 0.0 then
        radians = -math.pi * 0.5
    end

    return radians * 180.0 / math.pi
end

local function get_owner_to_actor_aim_rotation(owner, target_actor, label)
    local owner_location = get_actor_location_safe(owner)
    local target_location = get_actor_location_safe(target_actor)
    if owner_location == nil or target_location == nil then
        if label ~= nil then
            log(tostring(label) .. " face target skipped: location unavailable")
        end
        return nil
    end

    local dx = (target_location.X or 0.0) - (owner_location.X or 0.0)
    local dy = (target_location.Y or 0.0) - (owner_location.Y or 0.0)
    local dz = (target_location.Z or 0.0) - (owner_location.Z or 0.0)
    if dx * dx + dy * dy <= 0.000001 then
        if label ~= nil then
            log(tostring(label) .. " face target skipped: target too close")
        end
        return nil
    end

    local yaw = atan2_degrees(dy, dx)
    local flat_distance = math.sqrt(dx * dx + dy * dy)
    local pitch = -atan2_degrees(dz, flat_distance)
    return {
        yaw = yaw,
        pitch = pitch,
        owner_location = owner_location,
        target_location = target_location
    }
end

local function face_owner_to_actor_yaw(owner, target_actor, label)
    local aim = get_owner_to_actor_aim_rotation(owner, target_actor, label)
    if aim == nil then
        return false
    end

    local current_rotation = nil
    local ok, rotation = pcall(function()
        return owner.Rotation
    end)
    if ok then
        current_rotation = rotation
    end

    local roll = current_rotation ~= nil and (current_rotation.X or 0.0) or 0.0
    local actor_pitch = current_rotation ~= nil and (current_rotation.Y or 0.0) or 0.0
    local new_rotation = Vec3(roll, actor_pitch, aim.yaw)
    local set_ok, err = pcall(function()
        owner.Rotation = new_rotation
    end)

    if not set_ok then
        log(tostring(label) .. " face target failed: " .. tostring(err))
        return false
    end

    if owner.SetControlRotation ~= nil then
        local control_roll = 0.0
        local control_ok, control_rotation = pcall(function()
            return owner:GetControlRotation()
        end)
        if control_ok and control_rotation ~= nil then
            control_roll = control_rotation.X or 0.0
        end

        local control_set_ok, control_err = pcall(function()
            return owner:SetControlRotation(Vec3(control_roll, aim.pitch, aim.yaw))
        end)
        if not control_set_ok then
            log(tostring(label) .. " control rotation set failed: " .. tostring(control_err))
        end
    end

    local target_name = SPRAY_TARGET_ACTOR_NAME
    if target_actor ~= nil and target_actor.GetName ~= nil then
        target_name = tostring(target_actor:GetName())
    end

    log(tostring(label) .. " faced target=" .. tostring(target_name)
        .. " yaw=" .. tostring(aim.yaw)
        .. " pitch=" .. tostring(aim.pitch)
        .. " ownerLoc=" .. format_vec3(aim.owner_location)
        .. " targetLoc=" .. format_vec3(aim.target_location))
    return true
end

local function start_spray_face_blend(owner, target_actor, ability)
    if owner == nil or target_actor == nil or ability == nil then
        return false
    end

    local aim = get_owner_to_actor_aim_rotation(owner, target_actor, "SprayAttack")
    if aim == nil then
        return false
    end

    local actor_rotation = nil
    local actor_ok, actor_rot = pcall(function()
        return owner.Rotation
    end)
    if actor_ok then
        actor_rotation = actor_rot
    end

    local control_rotation = nil
    if owner.GetControlRotation ~= nil then
        local control_ok, control_rot = pcall(function()
            return owner:GetControlRotation()
        end)
        if control_ok then
            control_rotation = control_rot
        end
    end

    ability.spray_face_blend = {
        elapsed = 0.0,
        duration = SPRAY_FACE_BLEND_DURATION,
        actor_roll = actor_rotation ~= nil and (actor_rotation.X or 0.0) or 0.0,
        actor_pitch = actor_rotation ~= nil and (actor_rotation.Y or 0.0) or 0.0,
        from_actor_yaw = actor_rotation ~= nil and (actor_rotation.Z or 0.0) or 0.0,
        from_control_roll = control_rotation ~= nil and (control_rotation.X or 0.0) or 0.0,
        from_control_pitch = control_rotation ~= nil and (control_rotation.Y or 0.0) or 0.0,
        from_control_yaw = control_rotation ~= nil and (control_rotation.Z or 0.0) or (actor_rotation ~= nil and (actor_rotation.Z or 0.0) or 0.0),
        target_pitch = aim.pitch,
        target_yaw = aim.yaw
    }

    log("SprayAttack face blend started duration=" .. tostring(SPRAY_FACE_BLEND_DURATION)
        .. " targetYaw=" .. tostring(aim.yaw)
        .. " targetPitch=" .. tostring(aim.pitch)
        .. " ownerLoc=" .. format_vec3(aim.owner_location)
        .. " targetLoc=" .. format_vec3(aim.target_location))
    return true
end

local function tick_spray_face_blend(owner, ability, dt)
    if owner == nil or ability == nil or ability.spray_face_blend == nil then
        return false
    end

    local blend = ability.spray_face_blend
    blend.elapsed = blend.elapsed + (dt or 0.0)
    local alpha = smoothstep(blend.duration > 0.0 and (blend.elapsed / blend.duration) or 1.0)
    local yaw = lerp_angle(blend.from_control_yaw, blend.target_yaw, alpha)
    local pitch = lerp_angle(blend.from_control_pitch, blend.target_pitch, alpha)

    local actor_yaw = lerp_angle(blend.from_actor_yaw, blend.target_yaw, alpha)
    local actor_set_ok, actor_err = pcall(function()
        owner.Rotation = Vec3(blend.actor_roll, blend.actor_pitch, actor_yaw)
    end)
    if not actor_set_ok then
        log("SprayAttack face blend actor rotation failed: " .. tostring(actor_err))
    end

    if owner.SetControlRotation ~= nil then
        local control_set_ok, control_err = pcall(function()
            return owner:SetControlRotation(Vec3(blend.from_control_roll, pitch, yaw))
        end)
        if not control_set_ok then
            log("SprayAttack face blend control rotation failed: " .. tostring(control_err))
        end
    end

    if alpha >= 1.0 then
        ability.spray_face_blend = nil
        log("SprayAttack face blend ended")
    end

    return true
end

local function find_spray_target_actor()
    if World == nil or World.FindActorByName == nil then
        return nil
    end

    local ok, target = pcall(function()
        return World.FindActorByName(SPRAY_TARGET_ACTOR_NAME)
    end)

    if ok then
        return target
    end

    log("SprayAttack target lookup failed: " .. tostring(target))
    return nil
end

local function get_anim_instance(owner)
    if anim_instance ~= nil then
        return anim_instance
    end

    local mesh = nil
    if owner ~= nil and owner.GetMesh ~= nil then
        mesh = owner:GetMesh()
    end

    if mesh == nil and owner ~= nil and owner.GetSkeletalMeshComponent ~= nil then
        mesh = owner:GetSkeletalMeshComponent()
    end

    if mesh == nil then
        mesh = find_component_by_class(owner, "USkeletalMeshComponent")
    end

    if mesh ~= nil and mesh.GetAnimInstance ~= nil then
        anim_instance = mesh:GetAnimInstance()
    end

    return anim_instance
end

local function set_anim_bool(owner, variable_name, value)
    local instance = get_anim_instance(owner)
    if instance == nil or instance.SetGraphVariableBool == nil then
        log("AnimGraph bool set skipped: " .. variable_name .. " anim instance unavailable")
        return false
    end

    local ok, result = pcall(function()
        return instance:SetGraphVariableBool(variable_name, value)
    end)

    if not ok then
        log("AnimGraph bool set failed: " .. variable_name .. " error=" .. tostring(result))
        return false
    end

    if not result then
        log("AnimGraph bool set failed: variable not found " .. variable_name)
        return false
    end

    return true
end

local function activate_movement_ability(owner, ability, label, anim_var, distance, duration)
    if owner == nil then
        log(label .. " activate failed: owner is nil")
        ability.active_remaining = 0.0
        return
    end

    local forward = owner.Forward
    if forward == nil then
        forward = Vec3(1.0, 0.0, 0.0)
    end

    set_anim_bool(owner, anim_var, true)
    lock_movement(owner, label)

    if owner.StartCharacterDash ~= nil then
        local ok, started = pcall(function()
            return owner:StartCharacterDash(forward, distance, duration)
        end)

        if not ok then
            log(label .. " activate failed: StartCharacterDash call error: " .. tostring(started))
            set_anim_bool(owner, anim_var, false)
            unlock_movement(owner, label)
            ability.active_remaining = 0.0
            return
        end

        if not started then
            log(label .. " activate failed: StartCharacterDash returned false")
            set_anim_bool(owner, anim_var, false)
            unlock_movement(owner, label)
            ability.active_remaining = 0.0
            return
        end

        if label == "Dash" then
            start_dash_trail(owner)
            start_dash_afterimage(owner, forward, duration)
        end

        log(label .. " activated: distance=" .. tostring(distance)
            .. " duration=" .. tostring(duration)
            .. " forward=" .. format_vec3(forward))
        return
    end

    local dash_movement = get_character_movement(owner)
    if dash_movement == nil then
        log(label .. " activate failed: UCharacterMovementComponent not found")
        set_anim_bool(owner, anim_var, false)
        unlock_movement(owner, label)
        ability.active_remaining = 0.0
        return
    end

    if dash_movement.StartDash == nil then
        local movement_name = "unknown"
        if dash_movement.GetName ~= nil then
            movement_name = dash_movement:GetName()
        end
        log(label .. " activate failed: StartDash binding is unavailable on movement=" .. tostring(movement_name))
        set_anim_bool(owner, anim_var, false)
        unlock_movement(owner, label)
        ability.active_remaining = 0.0
        return
    end

    local ok, started = pcall(function()
        return dash_movement:StartDash(forward, distance, duration)
    end)
    if not ok then
        log(label .. " activate failed: StartDash call error: " .. tostring(started))
        set_anim_bool(owner, anim_var, false)
        unlock_movement(owner, label)
        ability.active_remaining = 0.0
        return
    end

    if not started then
        log(label .. " activate failed: StartDash returned false")
        set_anim_bool(owner, anim_var, false)
        unlock_movement(owner, label)
        ability.active_remaining = 0.0
        return
    end

    if label == "Dash" then
        start_dash_trail(owner)
        start_dash_afterimage(owner, forward, duration)
    end

    log(label .. " activated: distance=" .. tostring(distance)
        .. " duration=" .. tostring(duration)
        .. " forward=" .. format_vec3(forward))
end

local function activate_dash(owner, ability)
    activate_movement_ability(owner, ability, "Dash", DASH_ANIM_VAR, DASH_DISTANCE, DASH_DURATION)
    Audio.Play("Dash", 0.6)
end

local function end_dash(owner, ability)
    if owner ~= nil then
        set_anim_bool(owner, DASH_ANIM_VAR, false)
        unlock_movement(owner, "Dash")
        stop_dash_trail(owner)
    end
    log("Dash ended")
end

local function activate_roll(owner, ability)
    activate_movement_ability(owner, ability, "Roll", ROLL_ANIM_VAR, ROLL_DISTANCE, ROLL_DURATION)
end

local function end_roll(owner, ability)
    if owner ~= nil then
        set_anim_bool(owner, ROLL_ANIM_VAR, false)
        unlock_movement(owner, "Roll")
    end
    log("Roll ended")
end

local function face_owner_to_camera_yaw(owner, ability)
    if owner == nil then
        return false
    end

    if owner.FaceYawToControlRotation ~= nil then
        local ok, result = pcall(function()
            return owner:FaceYawToControlRotation()
        end)
        if ok and result then
            return true
        end
    end

    if ability ~= nil and not ability.face_yaw_warning_logged then
        ability.face_yaw_warning_logged = true
        log("AirBowShot warning: FaceYawToControlRotation unavailable")
    end
    return false
end

local function set_bow_first_person_camera(owner, ability, enabled)
    local arm = get_spring_arm(owner)
    if arm == nil then
        return false
    end

    if enabled then
        if arm.SetInheritPitch ~= nil then
            arm:SetInheritPitch(false)
        elseif ability ~= nil and not ability.inherit_pitch_warning_logged then
            ability.inherit_pitch_warning_logged = true
            log("AirBowShot warning: SpringArmComponent.SetInheritPitch unavailable")
        end
    elseif ability ~= nil and ability.saved_inherit_pitch ~= nil and arm.SetInheritPitch ~= nil then
        arm:SetInheritPitch(ability.saved_inherit_pitch)
    end

    if arm.ResetLagState ~= nil then
        arm:ResetLagState()
    end
    return true
end

local function update_bow_first_person_camera(owner, ability)
    if owner == nil then
        return false
    end

    local cam = get_camera(owner)
    if cam == nil or cam.SetRotation == nil or owner.GetControlRotation == nil then
        return false
    end

    local control = owner:GetControlRotation()
    if control == nil then
        return false
    end

    if owner.Rotation ~= nil then
        local saved = ability ~= nil and ability.saved_actor_rotation or nil
        owner.Rotation = Vec3(saved ~= nil and (saved.X or 0.0) or 0.0, control.Y or 0.0, control.Z or 0.0)
    end

    cam:SetRotation(Vec3(0.0, 0.0, 0.0))
    return true
end

local function restore_bow_actor_rotation(owner, ability)
    if owner == nil or ability == nil or ability.saved_actor_rotation == nil or owner.Rotation == nil then
        return
    end

    local current = owner.Rotation
    local saved = ability.saved_actor_rotation
    owner.Rotation = Vec3(saved.X or 0.0, saved.Y or 0.0, current ~= nil and (current.Z or 0.0) or (saved.Z or 0.0))
end

local function play_arrow_release_slomo(owner)
    if owner == nil then
        log("AirBowShot slomo skipped: owner is nil")
        return false
    end

    if owner.Slomo ~= nil then
        local ok, result = pcall(function()
            return owner:Slomo(BOW_RELEASE_SLOMO_DURATION, BOW_RELEASE_SLOMO_DILATION)
        end)
        if ok and result then
            log("AirBowShot slomo started: duration=" .. tostring(BOW_RELEASE_SLOMO_DURATION)
                .. " dilation=" .. tostring(BOW_RELEASE_SLOMO_DILATION))
            return true
        end
        log("AirBowShot slomo failed: " .. tostring(result))
        return false
    end

    local action = nil
    if owner.GetOrCreateActionComponent ~= nil then
        action = owner:GetOrCreateActionComponent()
    elseif owner.GetActionComponent ~= nil then
        action = owner:GetActionComponent()
    end

    if action ~= nil and action.Slomo ~= nil then
        action:Slomo(BOW_RELEASE_SLOMO_DURATION, BOW_RELEASE_SLOMO_DILATION)
        log("AirBowShot slomo started: duration=" .. tostring(BOW_RELEASE_SLOMO_DURATION)
            .. " dilation=" .. tostring(BOW_RELEASE_SLOMO_DILATION))
        return true
    end

    log("AirBowShot slomo skipped: ActionComponent unavailable")
    return false
end

local function get_bow_aim_particle_location(owner)
    if World ~= nil and World.GetCameraProjectileLocation ~= nil then
        local ok, loc = pcall(function()
            return World.GetCameraProjectileLocation(BOW_AIM_PARTICLE_OFFSET)
        end)
        if ok and loc ~= nil then
            return loc
        end
    end

    if owner ~= nil and owner.GetActorLocation ~= nil then
        return owner:GetActorLocation()
    end
    return Vec3(0.0, 0.0, 0.0)
end

local function update_bow_aim_particle(owner, ability)
    if ability == nil then
        return
    end

    local loc = get_bow_aim_particle_location(owner)
    if bow_aim_particle_component == nil then
        if World == nil or World.SpawnEmitterAtLocation == nil then
            log("AirBowShot Aim particle skipped: World.SpawnEmitterAtLocation unavailable")
            return
        end

        bow_aim_particle_component = World.SpawnEmitterAtLocation(
            BOW_AIM_PARTICLE_PATH,
            loc,
            Vec3(0.0, 0.0, 0.0),
            true)
        ability.aim_particle_active = bow_aim_particle_component ~= nil
        log("AirBowShot Aim particle spawned: component=" .. tostring(bow_aim_particle_component ~= nil)
            .. " path=" .. tostring(BOW_AIM_PARTICLE_PATH)
            .. " offset=" .. format_vec3(BOW_AIM_PARTICLE_OFFSET)
            .. " loc=" .. format_vec3(loc))
        return
    end

    if bow_aim_particle_component.SetLocation ~= nil then
        bow_aim_particle_component:SetLocation(loc)
    end

    if not ability.aim_particle_active then
        if bow_aim_particle_component.ResetParticles ~= nil then
            bow_aim_particle_component:ResetParticles()
        end
        if bow_aim_particle_component.Activate ~= nil then
            bow_aim_particle_component:Activate(true)
        end
        ability.aim_particle_active = true
        log("AirBowShot Aim particle activated: offset=" .. format_vec3(BOW_AIM_PARTICLE_OFFSET)
            .. " loc=" .. format_vec3(loc))
    end
end

local function stop_bow_aim_particle(ability, reason)
    if ability ~= nil then
        ability.aim_particle_active = false
    end

    if bow_aim_particle_component ~= nil and bow_aim_particle_component.Deactivate ~= nil then
        bow_aim_particle_component:Deactivate()
        log("AirBowShot Aim particle deactivated: reason=" .. tostring(reason))
    end
end

local function prepare_held_arrow(owner, ability, reason)
    if ability == nil then
        log("AirBowShot arrow prepare skipped: ability is nil reason=" .. tostring(reason))
        return nil
    end

    if ability.arrow_projectile ~= nil then
        if World ~= nil and World.UpdateCameraArrowProjectile ~= nil then
            World.UpdateCameraArrowProjectile(ability.arrow_projectile, BOW_PROJECTILE_OFFSET)
        end
        log("AirBowShot arrow prepare skipped: already prepared reason=" .. tostring(reason))
        return ability.arrow_projectile
    end

    if owner ~= nil then
        face_owner_to_camera_yaw(owner, ability)
        update_bow_first_person_camera(owner, ability)
    end

    if World ~= nil and World.PrepareCameraArrowProjectile ~= nil then
        ability.arrow_projectile = World.PrepareCameraArrowProjectile(BOW_PROJECTILE_OFFSET)
        log("AirBowShot arrow prepared by " .. tostring(reason)
            .. ": projectile=" .. tostring(ability.arrow_projectile ~= nil)
            .. " offset=" .. format_vec3(BOW_PROJECTILE_OFFSET))
    else
        log("AirBowShot arrow prepare skipped: World.PrepareCameraArrowProjectile unavailable reason=" .. tostring(reason))
    end

    return ability.arrow_projectile
end

local function launch_prepared_arrow(owner, ability, reason)
    if ability == nil then
        log("AirBowShot launch skipped: ability is nil reason=" .. tostring(reason))
        return false
    end

    if ability.arrow_released then
        log("AirBowShot launch skipped: arrow already released reason=" .. tostring(reason))
        return false
    end

    if ability.arrow_projectile == nil or World == nil or World.LaunchCameraArrowProjectile == nil then
        log("AirBowShot launch failed: prepared arrow or World.LaunchCameraArrowProjectile unavailable reason=" .. tostring(reason))
        return false
    end

    if owner ~= nil then
        face_owner_to_camera_yaw(owner, ability)
        update_bow_first_person_camera(owner, ability)
    end

    local launched = World.LaunchCameraArrowProjectile(ability.arrow_projectile, BOW_PROJECTILE_SPEED, BOW_PROJECTILE_OFFSET)
    log("AirBowShot launched by " .. tostring(reason) .. ": launched=" .. tostring(launched))
    if launched then
        ability.arrow_released = true
        ability.arrow_projectile = nil
        if Audio ~= nil and Audio.Play ~= nil then
            Audio.Play("Arrow", 10.0)
        end
        play_arrow_release_slomo(owner)
        return true
    end

    return false
end

local function launch_prepared_arrow_from_stored_aim(owner, ability, reason)
    if ability == nil then
        log("AirBowShot ultimate launch skipped: ability is nil reason=" .. tostring(reason))
        return false
    end

    if ability.arrow_released then
        log("AirBowShot ultimate launch skipped: arrow already released reason=" .. tostring(reason))
        return false
    end

    if ability.arrow_projectile == nil then
        prepare_held_arrow(owner, ability, tostring(reason) .. " fallback prepare")
    end

    if ability.arrow_projectile == nil then
        log("AirBowShot ultimate launch failed: no prepared arrow reason=" .. tostring(reason))
        return false
    end

    local launched = false
    if World ~= nil and World.LaunchArrowProjectileWithDirection ~= nil
        and ability.cutscene_launch_location ~= nil
        and ability.cutscene_launch_forward ~= nil then
        launched = World.LaunchArrowProjectileWithDirection(
            ability.arrow_projectile,
            ability.cutscene_launch_location,
            ability.cutscene_launch_forward,
            BOW_PROJECTILE_SPEED)
    else
        launched = launch_prepared_arrow(owner, ability, reason)
        return launched
    end

    log("AirBowShot ultimate launched by " .. tostring(reason)
        .. ": launched=" .. tostring(launched)
        .. " loc=" .. format_vec3(ability.cutscene_launch_location)
        .. " forward=" .. format_vec3(ability.cutscene_launch_forward))
    if launched then
        ability.arrow_released = true
        ability.arrow_projectile = nil
        if Audio ~= nil and Audio.Play ~= nil then
            Audio.Play("Arrow", 10.0)
        end
        play_arrow_release_slomo(owner)
        return true
    end
    return false
end

local function restore_bow_aim(owner, ability)
    if ability == nil then
        return
    end

    local character_movement = get_character_movement(owner)
    if character_movement ~= nil and ability.saved_gravity ~= nil and character_movement.SetGravity ~= nil then
        character_movement:SetGravity(ability.saved_gravity)
    end

    start_camera_blend(owner, {
        arm_length = ability.saved_arm_length,
        socket_offset = ability.saved_socket_offset,
        inherit_pitch = ability.saved_inherit_pitch,
        inherit_yaw = ability.saved_inherit_yaw,
        inherit_roll = ability.saved_inherit_roll,
        camera_rotation = ability.saved_camera_rotation,
        fov = ability.saved_fov
    }, BOW_CAMERA_RESTORE_DURATION, "AirBowShot restore")

    log("AirBowShot camera restore requested: armLength=" .. tostring(ability.saved_arm_length)
        .. " socketOffset=" .. format_vec3(ability.saved_socket_offset)
        .. " fov=" .. tostring(ability.saved_fov))
end

local function start_bow_ultimate_cutscene(owner, ability)
    if ability == nil or ability.cutscene_active or HaruUltimateCutscene.IsActive() then
        return
    end

    if camera_blend ~= nil and camera_blend.target ~= nil then
        apply_camera_state(owner, camera_blend.target)
        log("AirBowShot ultimate cutscene forced pending camera blend: " .. tostring(camera_blend.reason))
    end
    camera_blend = nil
    stop_bow_aim_particle(ability, "Ultimate cutscene start")

    local cutscene_target_actor = find_spray_target_actor()
    if cutscene_target_actor ~= nil then
        face_owner_to_actor_yaw(owner, cutscene_target_actor, "AirBowShot ultimate cutscene")
    else
        face_owner_to_camera_yaw(owner, ability)
    end
    set_bow_first_person_camera(owner, ability, true)
    apply_camera_state(owner, {
        arm_length = BOW_CAMERA_ARM_LENGTH,
        socket_offset = BOW_CAMERA_SOCKET_OFFSET,
        inherit_pitch = false,
        fov = BOW_CAMERA_FOV
    })
    update_bow_first_person_camera(owner, ability)
    local cutscene_spring_arm = get_spring_arm(owner)
    if cutscene_spring_arm ~= nil and cutscene_spring_arm.RefreshSpringArm ~= nil then
        cutscene_spring_arm:RefreshSpringArm(0.0)
    end

    ability.cutscene_active = true
    ability.cutscene_finished = false
    ability.cutscene_elapsed = 0.0
    ability.cutscene_eye_from = get_camera_state(owner)
    ability.damage_disabled_for_cutscene = set_player_damage_enabled(owner, false, "Ultimate cutscene start")
    ability.cutscene_launch_location = World ~= nil and World.GetCameraProjectileLocation ~= nil
        and World.GetCameraProjectileLocation(BOW_PROJECTILE_OFFSET) or nil
    ability.cutscene_launch_forward = World ~= nil and World.GetCameraProjectileForward ~= nil
        and World.GetCameraProjectileForward() or nil

    if ability.arrow_projectile == nil then
        prepare_held_arrow(owner, ability, "Ultimate release fallback")
    end
    if ability.arrow_projectile ~= nil and World ~= nil and World.UpdateCameraArrowProjectile ~= nil then
        World.UpdateCameraArrowProjectile(ability.arrow_projectile, BOW_PROJECTILE_OFFSET)
    end

    local cutscene_started = HaruUltimateCutscene.Start({
        owner = owner,
        spring_arm = get_spring_arm(owner),
        camera = get_camera(owner),
        log = function(message)
            log("UltimateCutscene " .. tostring(message))
        end,
        on_tick_lock = function()
            stop_owner_movement(owner)
        end,
        on_tick_projectile = function()
            if ability.arrow_projectile ~= nil and World ~= nil and World.UpdateCameraArrowProjectile ~= nil then
                World.UpdateCameraArrowProjectile(ability.arrow_projectile, BOW_PROJECTILE_OFFSET)
            end
        end,
        on_finish = function()
            if Engine ~= nil and Engine.ResumeGame ~= nil then
                Engine.ResumeGame()
            end
            ability.cutscene_finished = true
            ability.cutscene_active = false
            launch_prepared_arrow_from_stored_aim(owner, ability, "Ultimate cutscene end")
            if World ~= nil and World.ResetPlayerUltimateGauge ~= nil then
                World.ResetPlayerUltimateGauge(owner)
            end
            if ability.damage_disabled_for_cutscene then
                set_player_damage_enabled(owner, true, "Ultimate cutscene finish")
                ability.damage_disabled_for_cutscene = false
            end
            log("AirBowShot ultimate cutscene finished; gauge reset requested")
            if ability_system ~= nil then
                ability_system:EndAbility(ability)
            end
        end
    })

    if not cutscene_started then
        if Engine ~= nil and Engine.ResumeGame ~= nil then
            Engine.ResumeGame()
        end
        if ability.damage_disabled_for_cutscene then
            set_player_damage_enabled(owner, true, "Ultimate cutscene start failed")
            ability.damage_disabled_for_cutscene = false
        end
        ability.cutscene_active = false
        return
    end

    if Engine ~= nil and Engine.PauseGame ~= nil then
        Engine.PauseGame()
        log("AirBowShot ultimate cutscene world paused")
    end

    log("AirBowShot ultimate cutscene requested: gauge=" .. tostring(get_ultimate_gauge(owner))
        .. "/" .. tostring(get_ultimate_gauge_max(owner))
        .. " launchLoc=" .. format_vec3(ability.cutscene_launch_location)
        .. " launchForward=" .. format_vec3(ability.cutscene_launch_forward))
end

local function tick_bow_ultimate_cutscene(owner, ability, dt)
    if ability == nil or not ability.cutscene_active then
        return false
    end

    if Engine ~= nil and Engine.IsPaused ~= nil and Engine.IsPaused() then
        return true
    end

    if HaruUltimateCutscene.IsActive() then
        HaruUltimateCutscene.Tick(dt)
        return true
    end

    return false
end

local function can_activate_bow_ultimate(owner, ability)
    if BOW_ULTIMATE_IGNORE_GAUGE_FOR_TEST then
        if ability ~= nil then
            ability.block_reason = nil
        end
        log("AirBowShot ultimate gauge bypass enabled for test: gauge="
            .. tostring(get_ultimate_gauge(owner)) .. "/" .. tostring(get_ultimate_gauge_max(owner)))
        return true
    end

    local ready = is_ultimate_ready(owner)
    if not ready then
        local gauge = get_ultimate_gauge(owner)
        local gauge_max = get_ultimate_gauge_max(owner)
        if ability ~= nil then
            ability.block_reason = string.format("ultimate gauge %.1f/%.1f", gauge, gauge_max)
        end
        log("AirBowShot blocked: ultimate gauge=" .. tostring(gauge) .. "/" .. tostring(gauge_max))
        return false
    end
    if ability ~= nil then
        ability.block_reason = nil
    end
    return true
end

local function activate_bow_aim(owner, ability)
    if owner == nil then
        log("AirBowShot activate failed: owner is nil")
        ability.active_remaining = 0.0
        return
    end

    local character_movement = get_character_movement(owner)
    ability.started_airborne = is_bow_airborne(character_movement)
    log("AirBowShot activate: startedAirborne=" .. tostring(ability.started_airborne)
        .. " " .. get_movement_debug_state(character_movement))

    ability.saved_gravity = ability.started_airborne and character_movement ~= nil and character_movement.GetGravity ~= nil and character_movement:GetGravity() or nil
    ability.saved_actor_rotation = owner.Rotation

    local arm = get_spring_arm(owner)
    if arm ~= nil then
        ability.saved_arm_length = arm.GetTargetArmLength ~= nil and arm:GetTargetArmLength() or nil
        ability.saved_socket_offset = arm.GetSocketOffset ~= nil and arm:GetSocketOffset() or nil
        ability.saved_inherit_pitch = arm.GetInheritPitch ~= nil and arm:GetInheritPitch() or nil
        ability.saved_inherit_yaw = arm.GetInheritYaw ~= nil and arm:GetInheritYaw() or nil
        ability.saved_inherit_roll = arm.GetInheritRoll ~= nil and arm:GetInheritRoll() or nil
        log("AirBowShot spring arm found: savedArmLength=" .. tostring(ability.saved_arm_length)
            .. " savedSocketOffset=" .. format_vec3(ability.saved_socket_offset)
            .. " savedInheritPitch=" .. tostring(ability.saved_inherit_pitch)
            .. " hasSetTargetArmLength=" .. tostring(arm.SetTargetArmLength ~= nil)
            .. " hasSetSocketOffset=" .. tostring(arm.SetSocketOffset ~= nil))
    else
        log("AirBowShot warning: SpringArmComponent not found")
    end

    local cam = get_camera(owner)
    if cam ~= nil then
        ability.saved_fov = cam.GetFOV ~= nil and cam:GetFOV() or nil
        ability.saved_camera_rotation = cam.GetRotation ~= nil and cam:GetRotation() or nil
        log("AirBowShot camera found: savedFov=" .. tostring(ability.saved_fov)
            .. " hasSetFOV=" .. tostring(cam.SetFOV ~= nil))
    else
        log("AirBowShot warning: CameraComponent not found")
    end

    stop_owner_movement(owner)
    lock_movement(owner, BOW_SKILL_NAME)
    set_weapon_mesh(owner, BOW_STATIC_MESH_PATH, "Bow", BOW_WEAPON_TRANSFORM)
    set_anim_bool(owner, ARROW_ANIM_VAR, true)
    face_owner_to_camera_yaw(owner, ability)
    set_bow_first_person_camera(owner, ability, true)
    update_bow_first_person_camera(owner, ability)
    ability.arrow_projectile = nil
    ability.arrow_released = false
    ability.cutscene_active = false
    ability.cutscene_finished = false
    ability.cutscene_elapsed = 0.0
    ability.cutscene_launch_location = nil
    ability.cutscene_launch_forward = nil
    ability.damage_disabled_for_cutscene = false

    if ability.started_airborne and character_movement ~= nil and character_movement.SetGravity ~= nil then
        character_movement:SetGravity(BOW_AIM_GRAVITY)
    elseif ability.started_airborne then
        log("AirBowShot warning: CharacterMovementComponent.SetGravity unavailable")
    else
        log("AirBowShot ground aim: gravity unchanged")
    end

    start_camera_blend(owner, {
        arm_length = BOW_CAMERA_ARM_LENGTH,
        socket_offset = BOW_CAMERA_SOCKET_OFFSET,
        inherit_pitch = false,
        fov = BOW_CAMERA_FOV
    }, BOW_CAMERA_BLEND_DURATION, "AirBowShot aim")
    set_bow_radial_blur(true, "RightMouseButton press")

    log("AirBowShot aim started: gravity=" .. tostring(BOW_AIM_GRAVITY)
        .. " armLength=" .. tostring(BOW_CAMERA_ARM_LENGTH)
        .. " socketOffset=" .. format_vec3(BOW_CAMERA_SOCKET_OFFSET)
        .. " fov=" .. tostring(BOW_CAMERA_FOV))

    update_bow_aim_particle(owner, ability)
    log("AirBowShot aim started; FireArrow AnimNotify will prepare the held arrow")
end

local function tick_bow_aim(owner, ability, dt)
    if tick_bow_ultimate_cutscene(owner, ability, dt) then
        return
    end

    face_owner_to_camera_yaw(owner, ability)
    update_bow_first_person_camera(owner, ability)

    if ability.arrow_projectile ~= nil and World ~= nil and World.UpdateCameraArrowProjectile ~= nil then
        World.UpdateCameraArrowProjectile(ability.arrow_projectile, BOW_PROJECTILE_OFFSET)
    end
    update_bow_aim_particle(owner, ability)

    if Input ~= nil and Input.GetKeyUp ~= nil and Input.GetKeyUp(BOW_SKILL_KEY) then
        if ability.arrow_projectile == nil then
            log("AirBowShot release requested before FireArrow AnimNotify: preparing arrow immediately")
            prepare_held_arrow(owner, ability, "early release fallback")
        end

        stop_bow_aim_particle(ability, "RightMouseButton release")
        set_bow_radial_blur(false, "RightMouseButton release")
        start_bow_ultimate_cutscene(owner, ability)
    end
end

local function end_bow_aim(owner, ability)
    stop_bow_aim_particle(ability, "AirBowShot end")
    set_bow_radial_blur(false, "AirBowShot end")
    HaruUltimateCutscene.Stop()

    if ability ~= nil and ability.arrow_projectile ~= nil and not ability.arrow_released then
        if World ~= nil and World.ReleaseProjectile ~= nil then
            World.ReleaseProjectile(ability.arrow_projectile)
            log("AirBowShot held arrow released back to pool")
        end
        ability.arrow_projectile = nil
    end
    if ability ~= nil then
        if ability.damage_disabled_for_cutscene then
            set_player_damage_enabled(owner, true, "AirBowShot end")
        end
        ability.cutscene_active = false
        ability.cutscene_elapsed = 0.0
        ability.cutscene_launch_location = nil
        ability.cutscene_launch_forward = nil
        ability.damage_disabled_for_cutscene = false
        ability.block_reason = nil
    end

    restore_bow_aim(owner, ability)
    set_bow_first_person_camera(owner, ability, false)
    restore_bow_actor_rotation(owner, ability)
    if owner ~= nil then
        set_anim_bool(owner, ARROW_ANIM_VAR, false)
        set_weapon_mesh(owner, STAFF_STATIC_MESH_PATH, "Staff", STAFF_WEAPON_TRANSFORM)
        unlock_movement(owner, BOW_SKILL_NAME)
    end
    log("AirBowShot ended")
end

local function activate_spray_attack(owner, ability)
    if owner == nil then
        log("SprayAttack activate failed: owner is nil")
        ability.active_remaining = 0.0
        return
    end

    ability.locked_location = get_actor_location_safe(owner)
    ability.spray_target_actor = find_spray_target_actor()
    if ability.spray_target_actor ~= nil then
        start_spray_face_blend(owner, ability.spray_target_actor, ability)
    else
        log("SprayAttack warning: target actor not found name=" .. SPRAY_TARGET_ACTOR_NAME)
    end

    lock_movement_input_only(owner, ATTACK_SKILL_NAME)
    set_anim_bool(owner, ATTACK_ANIM_VAR, true)
    if World ~= nil and World.StartPlayerSprayAttack ~= nil then
        local started = World.StartPlayerSprayAttack(owner)
        log("SprayAttack started: " .. tostring(started))
        if not started then
            unlock_movement(owner, ATTACK_SKILL_NAME)
            set_anim_bool(owner, ATTACK_ANIM_VAR, false)
            ability.active_remaining = 0.0
        end
    else
        log("SprayAttack failed: World.StartPlayerSprayAttack unavailable")
        unlock_movement(owner, ATTACK_SKILL_NAME)
        set_anim_bool(owner, ATTACK_ANIM_VAR, false)
        ability.active_remaining = 0.0
    end
end

local function tick_spray_attack(owner, ability, dt)
    if owner ~= nil and ability ~= nil and ability.locked_location ~= nil then
        set_actor_horizontal_location_locked(owner, ability.locked_location)
    end

    if not tick_spray_face_blend(owner, ability, dt) then
        face_owner_to_camera_yaw(owner, ability)
    end

    if Input ~= nil and Input.GetKeyUp ~= nil and Input.GetKeyUp(FIRE_PROJECTILE_KEY) then
        log("SprayAttack release: " .. FIRE_PROJECTILE_KEY)
        ability_system:EndAbility(ability)
    end
end

local function end_spray_attack(owner, ability)
    if owner ~= nil then
        if World ~= nil and World.StopPlayerSprayAttack ~= nil then
            World.StopPlayerSprayAttack(owner)
        end
        set_anim_bool(owner, ATTACK_ANIM_VAR, false)
        unlock_movement(owner, ATTACK_SKILL_NAME)
    end
    if ability ~= nil then
        ability.locked_location = nil
        ability.spray_target_actor = nil
        ability.spray_face_blend = nil
    end
    log("SprayAttack ended")
end

local function setup_abilities()
    local owner = resolve_actor()
    if owner == nil then
        log("setup failed: owner is nil")
        return
    end

    ability_system = AbilitySystem.new(owner)
    ability_system:RegisterAbility({
        Name = DASH_SKILL_NAME,
        Key = DASH_SKILL_KEY,
        Keys = DASH_SKILL_KEYS,
        Duration = DASH_DURATION,
        Cooldown = 0.0,
        BlockWhileAnyActive = true,
        OnActivate = activate_dash,
        OnEnd = end_dash
    })
    ability_system:RegisterAbility({
        Name = ROLL_SKILL_NAME,
        Key = ROLL_SKILL_KEY,
        Duration = ROLL_DURATION,
        Cooldown = 0.0,
        BlockWhileAnyActive = true,
        OnActivate = activate_roll,
        OnEnd = end_roll
    })
    ability_system:RegisterAbility({
        Name = BOW_SKILL_NAME,
        Key = BOW_SKILL_KEY,
        Duration = BOW_AIM_MAX_DURATION,
        Cooldown = 0.0,
        BlockWhileAnyActive = true,
        CanActivate = can_activate_bow_ultimate,
        OnActivate = activate_bow_aim,
        OnTick = tick_bow_aim,
        OnEnd = end_bow_aim
    })
    ability_system:RegisterAbility({
        Name = ATTACK_SKILL_NAME,
        Key = FIRE_PROJECTILE_KEY,
        Duration = ATTACK_MAX_DURATION,
        Cooldown = 0.0,
        BlockWhileAnyActive = true,
        OnActivate = activate_spray_attack,
        OnTick = tick_spray_attack,
        OnEnd = end_spray_attack
    })

    log("registered Dash on " .. join_keys(DASH_SKILL_KEYS))
    log("registered Roll on " .. ROLL_SKILL_KEY)
    log("registered AirBowShot on " .. BOW_SKILL_KEY)
    log("registered SprayAttack on " .. FIRE_PROJECTILE_KEY)

    set_anim_bool(owner, DASH_ANIM_VAR, false)
    set_anim_bool(owner, ROLL_ANIM_VAR, false)
    set_anim_bool(owner, ATTACK_ANIM_VAR, false)
    set_anim_bool(owner, ARROW_ANIM_VAR, false)
    set_weapon_mesh(owner, STAFF_STATIC_MESH_PATH, "Staff", STAFF_WEAPON_TRANSFORM)
    reset_dash_trail(owner)
end

function BeginPlay()
    if HaruUltimateCutscene.ResetUnpausedTickRegistration ~= nil then
        HaruUltimateCutscene.ResetUnpausedTickRegistration()
    end
    setup_abilities()
end

function EndPlay()
    if ability_system ~= nil then
        local ability = ability_system:GetAbility(BOW_SKILL_NAME)
        if ability ~= nil then
            ability_system:EndAbility(ability)
        end
        ability = ability_system:GetAbility(ATTACK_SKILL_NAME)
        if ability ~= nil then
            ability_system:EndAbility(ability)
        end
    end

    if actor ~= nil then
        stop_walking_audio()
        set_anim_bool(actor, DASH_ANIM_VAR, false)
        set_anim_bool(actor, ROLL_ANIM_VAR, false)
        set_anim_bool(actor, ATTACK_ANIM_VAR, false)
        set_anim_bool(actor, ARROW_ANIM_VAR, false)
        set_weapon_mesh(actor, STAFF_STATIC_MESH_PATH, "Staff", STAFF_WEAPON_TRANSFORM)
        movement_locks = {}
        set_owner_movement_blocked(actor, false)
        reset_dash_trail(actor)
    end

    actor = nil
    ability_system = nil
    movement = nil
    anim_instance = nil
    dash_trail_particle = nil
    spring_arm = nil
    camera = nil
    weapon_mesh = nil
    camera_blend = nil
    walking_audio_playing = false
end

function OnOverlap(OtherActor)
end

function OnAnimNotify(name)
    log("AnimNotify received: " .. tostring(name))
end

function OnAnimNotify_FireArrow()
    log("AnimNotify_FireArrow received")
    if ability_system == nil then
        log("AirBowShot notify ignored: ability system unavailable")
        return
    end

    local ability = ability_system:GetAbility(BOW_SKILL_NAME)
    if ability == nil or not ability.is_active then
        log("AirBowShot notify ignored: ability is not active")
        return
    end

    local owner = resolve_actor()
    prepare_held_arrow(owner, ability, "FireArrow AnimNotify")
end

function Tick(dt)
    if ability_system == nil then
        setup_abilities()
    end

    if ability_system == nil then
        return
    end

    local owner = resolve_actor()
    update_walking_audio(owner)

    local dash_key = first_pressed_key(DASH_SKILL_KEYS)
    if dash_key ~= nil then
        log("input pressed: " .. dash_key)
        local activated, reason = ability_system:TryActivateByKey(dash_key)
        if not activated then
            log("Dash blocked: " .. (reason or "unknown"))
        end
    end

    if Input ~= nil and Input.GetKeyDown ~= nil and Input.GetKeyDown(ROLL_SKILL_KEY) then
        log("input pressed: " .. ROLL_SKILL_KEY)
        local activated, reason = ability_system:TryActivateByKey(ROLL_SKILL_KEY)
        if not activated then
            log("Roll blocked: " .. (reason or "unknown"))
        end
    end

    if Input ~= nil and Input.GetKeyDown ~= nil and Input.GetKeyDown(BOW_SKILL_KEY) then
        log("input pressed: " .. BOW_SKILL_KEY)
        local activated, reason = ability_system:TryActivateByKey(BOW_SKILL_KEY)
        if not activated then
            log("AirBowShot blocked: " .. (reason or "unknown"))
        end
    end

    if Input ~= nil and Input.GetKeyDown ~= nil and Input.GetKeyDown(FIRE_PROJECTILE_KEY) then
        log("input pressed: " .. FIRE_PROJECTILE_KEY)
        local activated, reason = ability_system:TryActivateByKey(FIRE_PROJECTILE_KEY)
        if not activated then
            log("SprayAttack blocked: " .. (reason or "unknown"))
        end
    end

    ability_system:Tick(dt)
    tick_camera_blend(dt)
end
