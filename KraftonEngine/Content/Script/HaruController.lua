local AbilitySystem = require("AbilitySystem")

-- Dash
local DASH_SKILL_NAME = "Dash"
local DASH_SKILL_KEY = "LeftShift"
local DASH_SKILL_KEYS = { DASH_SKILL_KEY, "GamepadLeftTrigger" }
local DASH_DISTANCE = 3.0
local DASH_DURATION = 0.3
local DASH_AFTERIMAGE_INTENSITY = 1.0
local DASH_AFTERIMAGE_RADIUS = 32.0
local DASH_AFTERIMAGE_SAMPLES = 16
-- Roll
local ROLL_SKILL_NAME = "Roll"
local ROLL_SKILL_KEY = "LeftControl"
local ROLL_DISTANCE = 6.0
local ROLL_DURATION = 1.2
-- Arrow
local BOW_SKILL_NAME = "AirBowShot"
local BOW_SKILL_KEY = "RightMouseButton"
local BOW_AIM_GRAVITY = 0.1
local BOW_AIM_MAX_DURATION = 60.0
local BOW_CAMERA_ARM_LENGTH = 3.0
local BOW_CAMERA_SOCKET_OFFSET = Vec3(0.0, -0.5, 1.0)
local BOW_CAMERA_FOV = 0.55
local BOW_CAMERA_BLEND_DURATION = 0.22
local BOW_CAMERA_RESTORE_DURATION = 0.18
local BOW_PROJECTILE_SPEED = 15.0
local BOW_PROJECTILE_MUZZLE_OFFSET = 2.0
local BOW_PROJECTILE_SPAWN_HEIGHT = 1.2
local BOW_RELEASE_SLOMO_DURATION = 0.3
local BOW_RELEASE_SLOMO_DILATION = 0.15
local STAFF_STATIC_MESH_PATH = "Content/Data/Staff/Staff_StaticMesh.uasset"
local BOW_STATIC_MESH_PATH = "Content/Data/Bow/Bow_StaticMesh.uasset"
-- Attack
local FIRE_PROJECTILE_KEY = "LeftMouseButton"
local PROJECTILE_SPEED = 3.0
local PROJECTILE_MUZZLE_OFFSET = 2.0
local DASH_ANIM_VAR = "Dash"
local ROLL_ANIM_VAR = "Roll"
local ATTACK_ANIM_VAR = "Attack"
local ARROW_ANIM_VAR = "Arrow"

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

local function lerp_number(a, b, alpha)
    if a == nil then
        return b
    end
    if b == nil then
        return a
    end
    return a + (b - a) * alpha
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

local function set_weapon_mesh(owner, mesh_path, label)
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
    end

    local cam = get_camera(owner)
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

local function unlock_movement(owner, reason)
    movement_locks[reason] = nil
    if not has_movement_locks() then
        set_owner_movement_blocked(owner, false)
    end
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
        fov = ability.saved_fov
    }, BOW_CAMERA_RESTORE_DURATION, "AirBowShot restore")

    log("AirBowShot camera restore requested: armLength=" .. tostring(ability.saved_arm_length)
        .. " socketOffset=" .. format_vec3(ability.saved_socket_offset)
        .. " fov=" .. tostring(ability.saved_fov))
end

local function activate_bow_aim(owner, ability)
    if owner == nil then
        log("AirBowShot activate failed: owner is nil")
        ability.active_remaining = 0.0
        return
    end

    local character_movement = get_character_movement(owner)
    if not is_bow_airborne(character_movement) then
        log("AirBowShot activate failed: character is not airborne; " .. get_movement_debug_state(character_movement))
        ability.active_remaining = 0.0
        return
    end

    ability.saved_gravity = character_movement.GetGravity ~= nil and character_movement:GetGravity() or nil

    local arm = get_spring_arm(owner)
    if arm ~= nil then
        ability.saved_arm_length = arm.GetTargetArmLength ~= nil and arm:GetTargetArmLength() or nil
        ability.saved_socket_offset = arm.GetSocketOffset ~= nil and arm:GetSocketOffset() or nil
        log("AirBowShot spring arm found: savedArmLength=" .. tostring(ability.saved_arm_length)
            .. " savedSocketOffset=" .. format_vec3(ability.saved_socket_offset)
            .. " hasSetTargetArmLength=" .. tostring(arm.SetTargetArmLength ~= nil)
            .. " hasSetSocketOffset=" .. tostring(arm.SetSocketOffset ~= nil))
    else
        log("AirBowShot warning: SpringArmComponent not found")
    end

    local cam = get_camera(owner)
    if cam ~= nil then
        ability.saved_fov = cam.GetFOV ~= nil and cam:GetFOV() or nil
        log("AirBowShot camera found: savedFov=" .. tostring(ability.saved_fov)
            .. " hasSetFOV=" .. tostring(cam.SetFOV ~= nil))
    else
        log("AirBowShot warning: CameraComponent not found")
    end

    stop_owner_movement(owner)
    lock_movement(owner, BOW_SKILL_NAME)
    set_weapon_mesh(owner, BOW_STATIC_MESH_PATH, "Bow")
    set_anim_bool(owner, ARROW_ANIM_VAR, true)
    face_owner_to_camera_yaw(owner, ability)

    if character_movement.SetGravity ~= nil then
        character_movement:SetGravity(BOW_AIM_GRAVITY)
    else
        log("AirBowShot warning: CharacterMovementComponent.SetGravity unavailable")
    end

    start_camera_blend(owner, {
        arm_length = BOW_CAMERA_ARM_LENGTH,
        socket_offset = BOW_CAMERA_SOCKET_OFFSET,
        fov = BOW_CAMERA_FOV
    }, BOW_CAMERA_BLEND_DURATION, "AirBowShot aim")

    log("AirBowShot aim started: gravity=" .. tostring(BOW_AIM_GRAVITY)
        .. " armLength=" .. tostring(BOW_CAMERA_ARM_LENGTH)
        .. " socketOffset=" .. format_vec3(BOW_CAMERA_SOCKET_OFFSET)
        .. " fov=" .. tostring(BOW_CAMERA_FOV))
end

local function tick_bow_aim(owner, ability, dt)
    face_owner_to_camera_yaw(owner, ability)

    local character_movement = get_character_movement(owner)
    if character_movement ~= nil and not is_bow_airborne(character_movement) then
        log("AirBowShot cancelled: landed; " .. get_movement_debug_state(character_movement))
        ability_system:EndAbility(ability)
        return
    end

    if Input ~= nil and Input.GetKeyUp ~= nil and Input.GetKeyUp(BOW_SKILL_KEY) then
        if World ~= nil and World.FireCameraProjectile ~= nil then
            local proj = World.FireCameraProjectile(BOW_PROJECTILE_SPEED, BOW_PROJECTILE_MUZZLE_OFFSET, BOW_PROJECTILE_SPAWN_HEIGHT)
            log("AirBowShot released: projectile=" .. tostring(proj ~= nil))
            if proj ~= nil then
                play_arrow_release_slomo(owner)
            end
        else
            log("AirBowShot release failed: World.FireCameraProjectile unavailable")
        end
        ability_system:EndAbility(ability)
    end
end

local function end_bow_aim(owner, ability)
    restore_bow_aim(owner, ability)
    if owner ~= nil then
        set_anim_bool(owner, ARROW_ANIM_VAR, false)
        set_weapon_mesh(owner, STAFF_STATIC_MESH_PATH, "Staff")
        unlock_movement(owner, BOW_SKILL_NAME)
    end
    log("AirBowShot ended")
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
        Cooldown = 5.0,
        BlockWhileAnyActive = true,
        OnActivate = activate_bow_aim,
        OnTick = tick_bow_aim,
        OnEnd = end_bow_aim
    })

    log("registered Dash on " .. join_keys(DASH_SKILL_KEYS))
    log("registered Roll on " .. ROLL_SKILL_KEY)
    log("registered AirBowShot on " .. BOW_SKILL_KEY)

    set_anim_bool(owner, DASH_ANIM_VAR, false)
    set_anim_bool(owner, ROLL_ANIM_VAR, false)
    set_anim_bool(owner, ATTACK_ANIM_VAR, false)
    set_anim_bool(owner, ARROW_ANIM_VAR, false)
    reset_dash_trail(owner)
end

function BeginPlay()
    setup_abilities()
end

function EndPlay()
    if ability_system ~= nil then
        local ability = ability_system:GetAbility(BOW_SKILL_NAME)
        if ability ~= nil then
            ability_system:EndAbility(ability)
        end
    end

    if actor ~= nil then
        set_anim_bool(actor, DASH_ANIM_VAR, false)
        set_anim_bool(actor, ROLL_ANIM_VAR, false)
        set_anim_bool(actor, ATTACK_ANIM_VAR, false)
        set_anim_bool(actor, ARROW_ANIM_VAR, false)
        set_weapon_mesh(actor, STAFF_STATIC_MESH_PATH, "Staff")
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
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if ability_system == nil then
        setup_abilities()
    end

    if ability_system == nil then
        return
    end

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

    -- ProjectilePool 테스트: 좌클릭 → 카메라 시점으로 발사체 발사
    if Input ~= nil and Input.GetKeyDown ~= nil and Input.GetKeyDown(BOW_SKILL_KEY) then
        log("input pressed: " .. BOW_SKILL_KEY)
        local character_movement = get_character_movement(resolve_actor())
        if not is_bow_airborne(character_movement) then
            log("AirBowShot blocked: character is not airborne; " .. get_movement_debug_state(character_movement))
        else
            local activated, reason = ability_system:TryActivateByKey(BOW_SKILL_KEY)
            if not activated then
                log("AirBowShot blocked: " .. (reason or "unknown"))
            end
        end
    end

    if Input ~= nil and Input.GetKeyDown ~= nil and Input.GetKeyDown(FIRE_PROJECTILE_KEY) then
        if World ~= nil and World.FireCameraProjectile ~= nil then
            local proj = World.FireCameraProjectile(PROJECTILE_SPEED, PROJECTILE_MUZZLE_OFFSET)
            log("FireCameraProjectile -> " .. tostring(proj ~= nil))
        else
            log("FireCameraProjectile unavailable (World 바인딩 미등록?)")
        end
    end

    ability_system:Tick(dt)
    tick_camera_blend(dt)
end
