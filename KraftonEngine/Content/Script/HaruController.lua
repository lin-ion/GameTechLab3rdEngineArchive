local AbilitySystem = require("AbilitySystem")

local STEAM_PARTICLE_PATH = "Content/Particle/SteamParticle.uasset"
local STEAM_SKILL_NAME = "SteamSkill"
local STEAM_SKILL_KEY = "LeftMouseButton"
local DASH_SKILL_NAME = "Dash"
local DASH_SKILL_KEY = "LeftShift"
local DASH_DISTANCE = 3.0
local DASH_DURATION = 0.3
local DASH_AFTERIMAGE_INTENSITY = 1.0
local DASH_AFTERIMAGE_RADIUS = 32.0
local DASH_AFTERIMAGE_SAMPLES = 16
local ROLL_SKILL_NAME = "Roll"
local ROLL_SKILL_KEY = "LeftControl"
local ROLL_DISTANCE = 6.0
local ROLL_DURATION = 1.2
-- ProjectilePool 발사 테스트(좌클릭). 주의: STEAM_SKILL_KEY 도 LeftMouseButton 이라 둘 다 발동됨.
local FIRE_PROJECTILE_KEY = "LeftMouseButton"
local PROJECTILE_SPEED = 3.0            -- m/s (물리 등속)
local PROJECTILE_MUZZLE_OFFSET = 1.0    -- m (카메라 앞 발사 원점, meter 단위)
local DASH_ANIM_VAR = "Dash"
local ROLL_ANIM_VAR = "Roll"
local ATTACK_ANIM_VAR = "Attack"

local actor = nil
local ability_system = nil
local movement = nil
local anim_instance = nil
local movement_locks = {}

local function log(message)
    print("[HaruController] " .. message)
end

local function format_vec3(value)
    if value == nil then
        return "nil"
    end

    return string.format("(%.3f, %.3f, %.3f)", value.X or 0.0, value.Y or 0.0, value.Z or 0.0)
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

local function get_character_movement(owner)
    if movement ~= nil then
        return movement
    end

    if owner ~= nil and owner.GetCharacterMovement ~= nil then
        movement = owner:GetCharacterMovement()
    end

    if movement == nil then
        movement = find_component_by_class(owner, "UCharacterMovementComponent")
    end

    return movement
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

local function get_effect_location(owner)
    local location = owner.Location
    local forward = owner.Forward

    if location == nil then
        return Vec3(0.0, 0.0, 0.0)
    end

    if forward == nil then
        return Vec3(location.X, location.Y, location.Z + 0.5)
    end

    return Vec3(
        location.X + forward.X * 1.0,
        location.Y + forward.Y * 1.0,
        location.Z + 0.5
    )
end

local function spawn_steam_effect(owner, ability)
    if owner == nil then
        log("SteamSkill activate failed: owner is nil")
        return
    end

    lock_movement(owner, STEAM_SKILL_NAME)
    set_anim_bool(owner, ATTACK_ANIM_VAR, true)

    if World == nil or World.SpawnEmitterAtLocation == nil then
        log("SteamSkill activate failed: World.SpawnEmitterAtLocation is unavailable")
        set_anim_bool(owner, ATTACK_ANIM_VAR, false)
        unlock_movement(owner, STEAM_SKILL_NAME)
        ability.active_remaining = 0.0
        return
    end

    local spawn_location = get_effect_location(owner)
    log("SteamSkill spawn request location=" .. format_vec3(spawn_location)
        .. " owner location=" .. format_vec3(owner.Location)
        .. " owner forward=" .. format_vec3(owner.Forward))

    local particle_component = World.SpawnEmitterAtLocation(STEAM_PARTICLE_PATH, spawn_location, Vec3(0.0, 0.0, 0.0), true)
    if particle_component == nil then
        log("SteamSkill activate failed: failed to spawn emitter")
        set_anim_bool(owner, ATTACK_ANIM_VAR, false)
        unlock_movement(owner, STEAM_SKILL_NAME)
        ability.active_remaining = 0.0
        return
    end

    local effect_actor = nil
    if particle_component.GetOwner ~= nil then
        effect_actor = particle_component:GetOwner()
    end

    if effect_actor == nil then
        log("SteamSkill warning: spawned emitter has no owner actor")
    else
        log("SteamSkill spawned actor location=" .. format_vec3(effect_actor.Location))
    end

    local template_path = "unknown"
    if particle_component.GetTemplatePath ~= nil then
        template_path = particle_component:GetTemplatePath()
    end

    local is_active = "unknown"
    if particle_component.IsActive ~= nil then
        is_active = tostring(particle_component:IsActive())
    end

    log("SteamSkill particle component template=" .. tostring(template_path)
        .. " active=" .. tostring(is_active)
        .. " actor location after activate=" .. format_vec3(effect_actor ~= nil and effect_actor.Location or nil))

    ability.effect_actor = effect_actor
    ability.particle_component = particle_component
    log("SteamSkill activated: spawned SteamParticle")
end

local function update_steam_effect(owner, ability, dt)
    if owner == nil then
        return
    end

    local effect_actor = ability.effect_actor
    if effect_actor == nil then
        return
    end

    if effect_actor.IsValid ~= nil and not effect_actor:IsValid() then
        ability.effect_actor = nil
        ability.particle_component = nil
        log("SteamSkill effect actor became invalid")
        return
    end

    local next_location = get_effect_location(owner)
    effect_actor.Location = next_location
end

local function end_steam_effect(owner, ability)
    if owner ~= nil then
        set_anim_bool(owner, ATTACK_ANIM_VAR, false)
        unlock_movement(owner, STEAM_SKILL_NAME)
    end

    local effect_actor = ability.effect_actor
    ability.effect_actor = nil
    ability.particle_component = nil

    if effect_actor == nil then
        log("SteamSkill ended: no effect actor to destroy")
        return
    end

    if effect_actor.IsValid ~= nil and not effect_actor:IsValid() then
        log("SteamSkill ended: effect actor was already invalid")
        return
    end

    local particle_component = effect_actor:GetParticleSystemComponent()
    if particle_component ~= nil then
        particle_component:Deactivate()
    end

    effect_actor:Destroy()
    log("SteamSkill ended: effect actor destroyed, cooldown started")
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

local function setup_abilities()
    local owner = resolve_actor()
    if owner == nil then
        log("setup failed: owner is nil")
        return
    end

    ability_system = AbilitySystem.new(owner)
    ability_system:RegisterAbility({
        Name = STEAM_SKILL_NAME,
        Key = STEAM_SKILL_KEY,
        Duration = 2.0,
        Cooldown = 2.0,
        BlockWhileAnyActive = true,
        OnActivate = spawn_steam_effect,
        OnTick = update_steam_effect,
        OnEnd = end_steam_effect
    })
    ability_system:RegisterAbility({
        Name = DASH_SKILL_NAME,
        Key = DASH_SKILL_KEY,
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

    log("registered SteamSkill on " .. STEAM_SKILL_KEY)
    log("registered Dash on " .. DASH_SKILL_KEY)
    log("registered Roll on " .. ROLL_SKILL_KEY)

    set_anim_bool(owner, DASH_ANIM_VAR, false)
    set_anim_bool(owner, ROLL_ANIM_VAR, false)
    set_anim_bool(owner, ATTACK_ANIM_VAR, false)
end

function BeginPlay()
    setup_abilities()
end

function EndPlay()
    if ability_system ~= nil then
        local ability = ability_system:GetAbility(STEAM_SKILL_NAME)
        if ability ~= nil then
            ability_system:EndAbility(ability)
        end
    end

    if actor ~= nil then
        set_anim_bool(actor, DASH_ANIM_VAR, false)
        set_anim_bool(actor, ROLL_ANIM_VAR, false)
        set_anim_bool(actor, ATTACK_ANIM_VAR, false)
        movement_locks = {}
        set_owner_movement_blocked(actor, false)
    end

    actor = nil
    ability_system = nil
    movement = nil
    anim_instance = nil
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

    if Input ~= nil and Input.GetKeyDown ~= nil and Input.GetKeyDown(STEAM_SKILL_KEY) then
        log("input pressed: " .. STEAM_SKILL_KEY)
        local activated, reason = ability_system:TryActivateByKey(STEAM_SKILL_KEY)
        if not activated then
            log("SteamSkill blocked: " .. (reason or "unknown"))
        end
    end

    if Input ~= nil and Input.GetKeyDown ~= nil and Input.GetKeyDown(DASH_SKILL_KEY) then
        log("input pressed: " .. DASH_SKILL_KEY)
        local activated, reason = ability_system:TryActivateByKey(DASH_SKILL_KEY)
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
    if Input ~= nil and Input.GetKeyDown ~= nil and Input.GetKeyDown(FIRE_PROJECTILE_KEY) then
        if World ~= nil and World.FireCameraProjectile ~= nil then
            local proj = World.FireCameraProjectile(PROJECTILE_SPEED, PROJECTILE_MUZZLE_OFFSET)
            log("FireCameraProjectile -> " .. tostring(proj ~= nil))
        else
            log("FireCameraProjectile unavailable (World 바인딩 미등록?)")
        end
    end

    ability_system:Tick(dt)
end
