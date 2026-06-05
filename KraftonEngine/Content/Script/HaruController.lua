local AbilitySystem = require("AbilitySystem")

local STEAM_PARTICLE_PATH = "Content/Particle/SteamParticle.uasset"
local STEAM_SKILL_NAME = "SteamSkill"
local STEAM_SKILL_KEY = "LeftMouseButton"
local DASH_SKILL_NAME = "Dash"
local DASH_SKILL_KEY = "LeftShift"
local DASH_DISTANCE = 3.0
local DASH_DURATION = 0.16

local actor = nil
local ability_system = nil
local movement = nil

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

    if World == nil or World.SpawnEmitterAtLocation == nil then
        log("SteamSkill activate failed: World.SpawnEmitterAtLocation is unavailable")
        return
    end

    local spawn_location = get_effect_location(owner)
    log("SteamSkill spawn request location=" .. format_vec3(spawn_location)
        .. " owner location=" .. format_vec3(owner.Location)
        .. " owner forward=" .. format_vec3(owner.Forward))

    local particle_component = World.SpawnEmitterAtLocation(STEAM_PARTICLE_PATH, spawn_location, Vec3(0.0, 0.0, 0.0), true)
    if particle_component == nil then
        log("SteamSkill activate failed: failed to spawn emitter")
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

local function activate_dash(owner, ability)
    if owner == nil then
        log("Dash activate failed: owner is nil")
        ability.active_remaining = 0.0
        return
    end

    local forward = owner.Forward
    if forward == nil then
        forward = Vec3(1.0, 0.0, 0.0)
    end

    if owner.StartCharacterDash ~= nil then
        local ok, started = pcall(function()
            return owner:StartCharacterDash(forward, DASH_DISTANCE, DASH_DURATION)
        end)

        if not ok then
            log("Dash activate failed: StartCharacterDash call error: " .. tostring(started))
            ability.active_remaining = 0.0
            return
        end

        if not started then
            log("Dash activate failed: StartCharacterDash returned false")
            ability.active_remaining = 0.0
            return
        end

        log("Dash activated: distance=" .. tostring(DASH_DISTANCE)
            .. " duration=" .. tostring(DASH_DURATION)
            .. " forward=" .. format_vec3(forward))
        return
    end

    local dash_movement = get_character_movement(owner)
    if dash_movement == nil then
        log("Dash activate failed: UCharacterMovementComponent not found")
        ability.active_remaining = 0.0
        return
    end

    if dash_movement.StartDash == nil then
        local movement_name = "unknown"
        if dash_movement.GetName ~= nil then
            movement_name = dash_movement:GetName()
        end
        log("Dash activate failed: StartDash binding is unavailable on movement=" .. tostring(movement_name))
        ability.active_remaining = 0.0
        return
    end

    local ok, started = pcall(function()
        return dash_movement:StartDash(forward, DASH_DISTANCE, DASH_DURATION)
    end)
    if not ok then
        log("Dash activate failed: StartDash call error: " .. tostring(started))
        ability.active_remaining = 0.0
        return
    end

    if not started then
        log("Dash activate failed: StartDash returned false")
        ability.active_remaining = 0.0
        return
    end

    log("Dash activated: distance=" .. tostring(DASH_DISTANCE)
        .. " duration=" .. tostring(DASH_DURATION)
        .. " forward=" .. format_vec3(forward))
end

local function end_dash(owner, ability)
    log("Dash ended")
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
        OnActivate = spawn_steam_effect,
        OnTick = update_steam_effect,
        OnEnd = end_steam_effect
    })
    ability_system:RegisterAbility({
        Name = DASH_SKILL_NAME,
        Key = DASH_SKILL_KEY,
        Duration = DASH_DURATION,
        Cooldown = 0.0,
        OnActivate = activate_dash,
        OnEnd = end_dash
    })

    log("registered SteamSkill on " .. STEAM_SKILL_KEY)
    log("registered Dash on " .. DASH_SKILL_KEY)
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

    actor = nil
    ability_system = nil
    movement = nil
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

    ability_system:Tick(dt)
end
