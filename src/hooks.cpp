#include "hooks.h"

#include "executor.h"

namespace bleedout_exec
{

/* ---------------- HOOKS ---------------- */

void ProcessHitHook::thunk(RE::Actor* a_victim, RE::HitData& a_hitData)
{
    // Post-hit trigger: still useful for bleedout execution (NPC is standing still in bleedout).
    // For getup execution, AttackActionHook is the primary trigger.
    if (BleedoutExecutor::getSingleton()->process(a_victim, a_hitData))
        return;
    func(a_victim, a_hitData);
}

bool AttackActionHook::thunk(RE::TESActionData* a_actionData)
{
    // Debug: confirm hook fires
    if (a_actionData && a_actionData->source)
    {
        auto actor = a_actionData->source->As<RE::Actor>();
        if (actor && actor->IsPlayerRef())
        {
            logger::debug("[AttackActionHook] Player attack action fired!");
        }
    }

    // This is the PRIMARY trigger for bleedout/getup execution.
    // It fires BEFORE the weapon swings, so no physics collision has occurred yet.
    // This allows _playPairedIdle to cleanly lock both actors into the animation.
    BleedoutExecutor::getSingleton()->processAttackAction(a_actionData);
    return func(a_actionData);
}

/* ------------- UTILITY FUNC ------------- */

bool isInPairedAnimation(const RE::Actor* actor)
{
    static RE::TESConditionItem cond;
    static std::once_flag       flag;
    std::call_once(flag, [&]() {
        cond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetPairedAnimation;
        cond.data.comparisonValue.f     = 0.0f;
        cond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kNotEqualTo;
        cond.data.object                = RE::CONDITIONITEMOBJECT::kSelf;
    });

    RE::ConditionCheckParams params(const_cast<RE::TESObjectREFR*>(actor->As<RE::TESObjectREFR>()), nullptr);
    return cond(params);
}

bool animPlayable(const RE::Actor* actor)
{
    return actor->Is3DLoaded() &&
        !actor->IsDisabled() &&
        !actor->IsDead() &&
        !isInPairedAnimation(actor) &&
        !actor->IsOnMount() &&
        !actor->IsInRagdollState();
}

bool isLastHostileInRange(const RE::Actor* attacker, const RE::Actor* victim, float range)
{
    auto process_lists = RE::ProcessLists::GetSingleton();
    if (!process_lists)
    {
        logger::error("Failed to get ProcessLists!");
        return false;
    }
    auto n_load_actors = process_lists->numberHighActors;
    if (n_load_actors == 0)
        return true;

    for (auto actor_handle : process_lists->highActorHandles)
    {
        if (!actor_handle || !actor_handle.get())
            continue;

        auto actor = actor_handle.get().get();

        if ((actor == attacker) || (actor == victim) || actor->IsDead() || actor->AsActorState()->IsBleedingOut() || actor->IsDisabled())
            continue;

        float dist = actor->GetPosition().GetDistance(attacker->GetPosition());
        if ((dist < range) && actor->IsHostileToActor(const_cast<RE::Actor*>(attacker)))
        {
            logger::debug("{} in range!", actor->GetName());
            return false;
        }
    }
    // check player
    if (!attacker->IsPlayerRef() && !victim->IsPlayerRef())
        if (RE::Actor* player = RE::PlayerCharacter::GetSingleton(); player)
        {
            float dist = player->GetPosition().GetDistance(attacker->GetPosition());
            if ((dist < range) && const_cast<RE::Actor*>(attacker)->IsHostileToActor(player))
                return false;
        }

    return true;
}


// playPairedIdle wrapper removed — executor.cpp calls _playPairedIdle directly

/* ------------- SKELETON RACE ------------- */

std::string getSkeletonRace(const RE::Actor* actor)
{
    auto skel = actor->GetRace()->skeletonModels[actor->GetActorBase()->IsFemale()].model;
    if (!_stricmp(skel.c_str(), "Actors\\DLC02\\DwarvenBallistaCenturion\\Character Assets\\skeleton.nif")) return "ballista";
    if (!_stricmp(skel.c_str(), "Actors\\Bear\\Character Assets\\skeleton.nif")) return "bear";
    if (!_stricmp(skel.c_str(), "Actors\\DLC02\\BoarRiekling\\Character Assets\\SkeletonBoar.nif")) return "boar";
    if (!_stricmp(skel.c_str(), "Actors\\DwarvenSteamCenturion\\Character Assets\\skeleton.nif")) return "centurion";
    if (!_stricmp(skel.c_str(), "Actors\\DLC01\\ChaurusFlyer\\Character Assets\\skeleton.nif")) return "chaurushunter";
    if (!_stricmp(skel.c_str(), "Actors\\Dragon\\Character Assets\\Skeleton.nif")) return "dragon";
    if (!_stricmp(skel.c_str(), "Actors\\Draugr\\Character Assets\\Skeleton.nif")) return "draugr";
    if (!_stricmp(skel.c_str(), "Actors\\Draugr\\Character Assets\\SkeletonF.nif")) return "draugr";
    if (!_stricmp(skel.c_str(), "Actors\\Draugr\\Character Assets\\SkeletonS.nif")) return "skeleton";
    if (!_stricmp(skel.c_str(), "Actors\\Falmer\\Character Assets\\Skeleton.nif")) return "falmer";
    if (!_stricmp(skel.c_str(), "Actors\\DLC01\\VampireBrute\\Character Assets\\skeleton.nif")) return "gargoyle";
    if (!_stricmp(skel.c_str(), "Actors\\Giant\\Character Assets\\skeleton.nif")) return "giant";
    if (!_stricmp(skel.c_str(), "Actors\\Hagraven\\Character Assets\\skeleton.nif")) return "hagraven";
    if (!_stricmp(skel.c_str(), "Actors\\DLC02\\BenthicLurker\\Character Assets\\skeleton.nif")) return "lurker";
    if (!_stricmp(skel.c_str(), "Actors\\DLC02\\Riekling\\Character Assets\\skeleton.nif")) return "riekling";
    if (!_stricmp(skel.c_str(), "Actors\\SabreCat\\Character Assets\\Skeleton.nif")) return "sabrecat";
    if (!_stricmp(skel.c_str(), "Actors\\DLC02\\Scrib\\Character Assets\\skeleton.nif")) return "ashhopper";
    if (!_stricmp(skel.c_str(), "Actors\\FrostbiteSpider\\Character Assets\\skeleton.nif")) return "spider";
    if (!_stricmp(skel.c_str(), "Actors\\Spriggan\\Character Assets\\skeleton.nif")) return "spriggan";
    if (!_stricmp(skel.c_str(), "Actors\\Troll\\Character Assets\\skeleton.nif")) return "troll";
    if (!_stricmp(skel.c_str(), "Actors\\Canine\\Character Assets Wolf\\skeleton.nif")) return "wolf";
    if (!_stricmp(skel.c_str(), "Actors\\WerewolfBeast\\Character Assets\\skeleton.nif")) return "werewolf";
    if (!_stricmp(skel.c_str(), "Actors\\VampireLord\\Character Assets\\Skeleton.nif")) return "vamplord";
    if (!_stricmp(skel.c_str(), "Actors\\Chaurus\\Character Assets\\skeleton.nif")) return "chaurus";
    if (!_stricmp(skel.c_str(), "Actors\\Deer\\Character Assets\\Skeleton.nif")) return "deer";
    if (!_stricmp(skel.c_str(), "Actors\\Canine\\Character Assets Dog\\skeleton.nif")) return "dog";
    if (!_stricmp(skel.c_str(), "Actors\\DragonPriest\\Character Assets\\skeleton.nif")) return "priest";
    if (!_stricmp(skel.c_str(), "Actors\\DwarvenSphereCenturion\\Character Assets\\skeleton.nif")) return "sphere";
    if (!_stricmp(skel.c_str(), "Actors\\DwarvenSpider\\Character Assets\\skeleton.nif")) return "dwarvenspider";
    if (!_stricmp(skel.c_str(), "Actors\\AtronachFlame\\Character Assets\\skeleton.nif")) return "flameatronach";
    if (!_stricmp(skel.c_str(), "Actors\\AtronachFrost\\Character Assets\\skeleton.nif")) return "frostatronach";
    if (!_stricmp(skel.c_str(), "Actors\\AtronachStorm\\Character Assets\\skeleton.nif")) return "stormatronach";
    if (!_stricmp(skel.c_str(), "Actors\\Goat\\Character Assets\\skeleton.nif")) return "goat";
    if (!_stricmp(skel.c_str(), "Actors\\Horker\\Character Assets\\skeleton.nif")) return "horker";
    if (!_stricmp(skel.c_str(), "Actors\\Horse\\Character Assets\\skeleton.nif")) return "horse";
    if (!_stricmp(skel.c_str(), "Actors\\IceWraith\\Character Assets\\skeleton.nif")) return "wraith";
    if (!_stricmp(skel.c_str(), "Actors\\Mammoth\\Character Assets\\skeleton.nif")) return "mammoth";
    if (!_stricmp(skel.c_str(), "Actors\\Skeever\\Character Assets\\skeleton.nif")) return "skeever";
    if (!_stricmp(skel.c_str(), "Actors\\Slaughterfish\\Character Assets\\skeleton.nif")) return "slaughterfish";
    if (!_stricmp(skel.c_str(), "Actors\\Wisp\\Character Assets\\skeleton.nif")) return "wisp";
    if (!_stricmp(skel.c_str(), "Actors\\Witchlight\\Character Assets\\skeleton.nif")) return "witchlight";
    if (!_stricmp(skel.c_str(), "Actors\\Cow\\Character Assets\\skeleton.nif")) return "cow";
    if (!_stricmp(skel.c_str(), "Actors\\Ambient\\Hare\\Character Assets\\skeleton.nif")) return "rabbit";
    if (!_stricmp(skel.c_str(), "Actors\\Mudcrab\\Character Assets\\skeleton.nif")) return "mudcrab";
    if (!_stricmp(skel.c_str(), "Actors\\DLC02\\HMDaedra\\Character Assets\\Skeleton.nif")) return "seeker";
    if (!_stricmp(skel.c_str(), "Actors\\DLC02\\Netch\\CharacterAssets\\skeleton.nif")) return "netch";
    return {};
}

StrSet getBannedSkels(const RE::Actor* actor, std::string_view prefix)
{
    StrSet      banned = {};
    std::string prefix_str{prefix};
    auto        skel = actor->GetRace()->skeletonModels[actor->GetActorBase()->IsFemale()].model;

    // All known non-human skeletons — if actor's skel doesn't match, ban the tag
    const std::vector<std::pair<const char*, const char*>> skel_map = {
        {"Actors\\DLC02\\DwarvenBallistaCenturion\\Character Assets\\skeleton.nif", "ballista"},
        {"Actors\\Bear\\Character Assets\\skeleton.nif", "bear"},
        {"Actors\\DLC02\\BoarRiekling\\Character Assets\\SkeletonBoar.nif", "boar"},
        {"Actors\\DwarvenSteamCenturion\\Character Assets\\skeleton.nif", "centurion"},
        {"Actors\\DLC01\\ChaurusFlyer\\Character Assets\\skeleton.nif", "chaurushunter"},
        {"Actors\\Dragon\\Character Assets\\Skeleton.nif", "dragon"},
        {"Actors\\Falmer\\Character Assets\\Skeleton.nif", "falmer"},
        {"Actors\\DLC01\\VampireBrute\\Character Assets\\skeleton.nif", "gargoyle"},
        {"Actors\\Giant\\Character Assets\\skeleton.nif", "giant"},
        {"Actors\\Hagraven\\Character Assets\\skeleton.nif", "hagraven"},
        {"Actors\\DLC02\\BenthicLurker\\Character Assets\\skeleton.nif", "lurker"},
        {"Actors\\DLC02\\Riekling\\Character Assets\\skeleton.nif", "riekling"},
        {"Actors\\SabreCat\\Character Assets\\Skeleton.nif", "sabrecat"},
        {"Actors\\DLC02\\Scrib\\Character Assets\\skeleton.nif", "ashhopper"},
        {"Actors\\FrostbiteSpider\\Character Assets\\skeleton.nif", "spider"},
        {"Actors\\Spriggan\\Character Assets\\skeleton.nif", "spriggan"},
        {"Actors\\Troll\\Character Assets\\skeleton.nif", "troll"},
        {"Actors\\Canine\\Character Assets Wolf\\skeleton.nif", "wolf"},
        {"Actors\\WerewolfBeast\\Character Assets\\skeleton.nif", "werewolf"},
        {"Actors\\VampireLord\\Character Assets\\Skeleton.nif", "vamplord"},
        {"Actors\\Chaurus\\Character Assets\\skeleton.nif", "chaurus"},
        {"Actors\\Deer\\Character Assets\\Skeleton.nif", "deer"},
        {"Actors\\Canine\\Character Assets Dog\\skeleton.nif", "dog"},
        {"Actors\\DragonPriest\\Character Assets\\skeleton.nif", "priest"},
        {"Actors\\DwarvenSphereCenturion\\Character Assets\\skeleton.nif", "sphere"},
        {"Actors\\DwarvenSpider\\Character Assets\\skeleton.nif", "dwarvenspider"},
        {"Actors\\AtronachFlame\\Character Assets\\skeleton.nif", "flameatronach"},
        {"Actors\\AtronachFrost\\Character Assets\\skeleton.nif", "frostatronach"},
        {"Actors\\AtronachStorm\\Character Assets\\skeleton.nif", "stormatronach"},
        {"Actors\\Goat\\Character Assets\\skeleton.nif", "goat"},
        {"Actors\\Horker\\Character Assets\\skeleton.nif", "horker"},
        {"Actors\\Horse\\Character Assets\\skeleton.nif", "horse"},
        {"Actors\\IceWraith\\Character Assets\\skeleton.nif", "wraith"},
        {"Actors\\Mammoth\\Character Assets\\skeleton.nif", "mammoth"},
        {"Actors\\Skeever\\Character Assets\\skeleton.nif", "skeever"},
        {"Actors\\Slaughterfish\\Character Assets\\skeleton.nif", "slaughterfish"},
        {"Actors\\Wisp\\Character Assets\\skeleton.nif", "wisp"},
        {"Actors\\Witchlight\\Character Assets\\skeleton.nif", "witchlight"},
        {"Actors\\Cow\\Character Assets\\skeleton.nif", "cow"},
        {"Actors\\Ambient\\Hare\\Character Assets\\skeleton.nif", "rabbit"},
        {"Actors\\Mudcrab\\Character Assets\\skeleton.nif", "mudcrab"},
        {"Actors\\DLC02\\HMDaedra\\Character Assets\\Skeleton.nif", "seeker"},
        {"Actors\\DLC02\\Netch\\CharacterAssets\\skeleton.nif", "netch"},
    };

    // Handle draugr specially (two skeleton variants map to same race)
    if (_stricmp(skel.c_str(), "Actors\\Draugr\\Character Assets\\Skeleton.nif") &&
        _stricmp(skel.c_str(), "Actors\\Draugr\\Character Assets\\SkeletonF.nif"))
        banned.insert(prefix_str + "draugr");

    if (_stricmp(skel.c_str(), "Actors\\Draugr\\Character Assets\\SkeletonS.nif"))
        banned.insert(prefix_str + "skeleton");

    for (const auto& [skel_path, race_name] : skel_map)
    {
        if (_stricmp(skel.c_str(), skel_path))
            banned.insert(prefix_str + race_name);
    }

    return banned;
}


} // namespace bleedout_exec

