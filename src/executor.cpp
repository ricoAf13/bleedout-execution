#include "executor.h"

#include "hooks.h"
#include "settings.h"

#include <filesystem>
namespace fs = std::filesystem;

namespace bleedout_exec
{

bool BleedoutExecutor::init()
{
    logger::info("Initializing BleedoutExecution...");

    auto settings = Settings::getSingleton();
    if (!settings->load(def_config_path))
        logger::warn("Config not loaded, using defaults.");

    if (settings->enable_debug_log)
    {
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
    }

    bool all_ok = true;

    all_ok &= loadAnims();
    all_ok &= loadKaputtRoot();

    // Disable vanilla killmove system completely.
    // KaputtVanillaKillmoves.esp defines globals that control vanilla killmove conditions.
    // Kaputt sets ALL of these to 0 by default (disable_vanilla = true).
    // Without this, the game's built-in killmove system detects the paired idle and 
    // immediately kills the victim.
    const char* globals_to_disable[] = {"Killmove", "KapVanillaSneak", "KapVanillaDragonBite"};
    for (auto global_name : globals_to_disable)
    {
        auto global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(global_name);
        if (global)
        {
            global->value = 0;
            logger::info("Set '{}' global to 0.", global_name);
        }
        else
        {
            logger::info("'{}' global not found (optional).", global_name);
        }
    }

    ready.store(true);
    logger::info("BleedoutExecution initialized. {} animations registered. KaputtRoot: {}",
                 anim_tags_map.size(), idle_kaputt_root ? "found" : "NOT FOUND");

    return all_ok;
}

bool BleedoutExecutor::loadKaputtRoot()
{
    idle_kaputt_root = RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KaputtRoot");
    if (!idle_kaputt_root)
    {
        logger::warn("KaputtRoot idle form not found! Make sure KaputtVanillaKillmoves.esp is loaded.");
        logger::warn("Weapon-based animation filtering will NOT work without KaputtRoot.");
        return false;
    }

    // Count child idles for logging
    int child_count = 0;
    if (idle_kaputt_root->childIdles)
    {
        for ([[maybe_unused]] auto const form : *idle_kaputt_root->childIdles)
            child_count++;
    }
    logger::info("KaputtRoot found with {} child tagger idles.", child_count);

    return true;
}

bool BleedoutExecutor::loadAnims()
{
    auto settings  = Settings::getSingleton();
    auto anim_path = settings->anim_dir;

    logger::info("Loading animation entries from {}...", anim_path);

    bool all_ok = true;

    if (!fs::exists(anim_path))
    {
        logger::warn("Animation folder '{}' doesn't exist.", anim_path);
        return false;
    }

    for (auto const& dir_entry : fs::directory_iterator{anim_path})
    {
        if (!dir_entry.is_regular_file())
            continue;

        auto file_path = dir_entry.path();
        if (file_path.extension() != ".json")
            continue;

        logger::info("Reading {}", file_path.string());

        std::ifstream istream{file_path};
        if (!istream.is_open())
        {
            logger::warn("Failed to open {}", file_path.filename().string());
            all_ok = false;
            continue;
        }

        json j;
        try
        {
            j = json::parse(istream);
        }
        catch (json::parse_error& e)
        {
            logParseError(e);
            all_ok = false;
            continue;
        }

        StrMap<StrSet> new_tags = {};
        try
        {
            new_tags = j;
        }
        catch (json::exception& e)
        {
            logJsonException("StrMap<StrSet>", e);
            all_ok = false;
            continue;
        }

        // Validate each EditorID exists as a TESIdleForm in the game
        std::erase_if(new_tags, [&](const auto& item) {
            auto const& [edid, tags] = item;
            auto form                = RE::TESForm::LookupByEditorID<RE::TESIdleForm>(edid);
            if (!form)
            {
                logger::warn("Cannot find IdleForm {}! Skipping.", edid);
                all_ok = false;
                return true;
            }
            return false;
        });

        auto anim_count = new_tags.size();
        anim_tags_map.merge(new_tags);

        logger::info("Successfully registered {} animations from {}", anim_count, file_path.filename().string());
    }

    logger::info("All animation entries loaded. Total: {}", anim_tags_map.size());
    return all_ok;
}

bool BleedoutExecutor::process(RE::Actor* victim, RE::HitData& hit_data)
{
    auto settings = Settings::getSingleton();

    if (!settings->enabled)
        return false;

    if (!ready.load())
        return false;

    auto attacker = hit_data.aggressor.get().get();
    if (!attacker || !victim)
        return false;

    // Bleedout check
    bool is_bleedout = victim->AsActorState()->IsBleedingOut();
    auto knock_state = victim->AsActorState()->GetKnockState();
    bool is_getup    = (knock_state == RE::KNOCK_STATE_ENUM::kGetUp) ||
        (knock_state == RE::KNOCK_STATE_ENUM::kQueued);

    uint8_t do_trigger = (settings->enable_bleedout_execution && is_bleedout) ||
        (settings->enable_getup_execution && is_getup);

    if (!do_trigger)
        return false;

    logger::debug(">>> Bleedout Execution trigger! Attacker: {} | Victim: {} | bleedout={} getup={} knockState={}",
                 attacker->GetName(), victim->GetName(),
                 is_bleedout, is_getup, static_cast<int>(knock_state));

    if (!precondition(attacker, victim))
    {
        logger::debug("<<< Precondition failed.");
        return false;
    }

    if (!lottery())
    {
        logger::debug("<<< Lottery failed.");
        return false;
    }

    return submit(attacker, victim);
}

void BleedoutExecutor::processAttackAction(RE::TESActionData* action_data)
{
    if (!Settings::getSingleton()->enabled) return;
    if (!ready.load()) return;

    if (!(action_data && action_data->source)) return;
    auto attacker = action_data->source->As<RE::Actor>();
    if (!attacker) return;

    // Get victim: try combat target first, then crosshair target as fallback.
    // Combat target may be null when player uses spells like Telekinesis 
    // (which don't establish melee combat). Kaputt's VanillaTrigger::process()
    // uses crosshair target in its no-argument overload for the same reason.
    RE::Actor* victim = nullptr;
    auto victim_ptr = attacker->GetActorRuntimeData().currentCombatTarget.get();
    if (victim_ptr)
    {
        victim = victim_ptr.get();
    }
    else if (attacker->IsPlayerRef())
    {
        // Fallback: use crosshair target (only for player)
        auto target_ref = RE::CrosshairPickData::GetSingleton()->targetActor;
        if (target_ref && target_ref.get())
            victim = target_ref.get().get()->As<RE::Actor>();
    }
    if (!victim) return;

    // Distance fix (same as Kaputt — only apply to non-player attackers)
    if (!attacker->IsPlayerRef() && (attacker->GetPosition().GetDistance(victim->GetPosition()) > 192))
        return;

    // Check trigger conditions
    auto& settings   = *Settings::getSingleton();
    bool  is_bleedout = settings.enable_bleedout_execution && victim->AsActorState()->IsBleedingOut();
    bool  getting_up  = (victim->AsActorState()->GetKnockState() == RE::KNOCK_STATE_ENUM::kGetUp) ||
                       (victim->AsActorState()->GetKnockState() == RE::KNOCK_STATE_ENUM::kQueued);
    bool  is_getup    = settings.enable_getup_execution && getting_up;

    if (!is_bleedout && !is_getup) return;

    logger::debug(">>> AttackAction Execution trigger! Attacker: {} | Victim: {} | bleedout={} getup={} knockState={}",
                 attacker->GetName(), victim->GetName(),
                 is_bleedout, is_getup,
                 static_cast<int>(victim->AsActorState()->GetKnockState()));

    if (!precondition(attacker, victim))
    {
        logger::debug("<<< Precondition failed.");
        return;
    }

    if (!lottery())
    {
        logger::debug("<<< Lottery failed.");
        return;
    }

    submit(attacker, victim);
}

bool BleedoutExecutor::precondition(const RE::Actor* attacker, const RE::Actor* victim)
{
    auto settings = Settings::getSingleton();

    // Playable check
    if (!animPlayable(attacker))
    {
        logger::debug("  precondition: attacker not animPlayable (3D={} dead={} paired={} mount={} ragdoll={})",
                     attacker->Is3DLoaded(), attacker->IsDead(), isInPairedAnimation(attacker),
                     attacker->IsOnMount(), attacker->IsInRagdollState());
        return false;
    }
    if (!animPlayable(victim))
    {
        logger::debug("  precondition: victim not animPlayable (3D={} dead={} paired={} mount={} ragdoll={})",
                     victim->Is3DLoaded(), victim->IsDead(), isInPairedAnimation(victim),
                     victim->IsOnMount(), victim->IsInRagdollState());
        return false;
    }

    // Essential check
    if (settings->essential_protection && victim->IsEssential() && !attacker->IsPlayerRef())
    {
        logger::debug("  precondition: victim is essential, attacker is not player");
        return false;
    }

    // Protected check
    if (settings->protected_protection && victim->IsProtected() && !attacker->IsPlayerRef())
    {
        logger::debug("  precondition: victim is protected, attacker is not player");
        return false;
    }

    // Height diff check
    auto height_diff = victim->GetPositionZ() - attacker->GetPositionZ();
    if ((height_diff < settings->height_diff_range[0]) || (height_diff > settings->height_diff_range[1]))
    {
        logger::debug("  precondition: height diff {} out of range [{}, {}]",
                     height_diff, settings->height_diff_range[0], settings->height_diff_range[1]);
        return false;
    }

    // Last hostile check
    if (settings->last_hostile_check && !isLastHostileInRange(attacker, victim, settings->last_hostile_range))
    {
        logger::debug("  precondition: other hostiles in range");
        return false;
    }

    // Race filters
    if (std::ranges::any_of(std::array{attacker, victim}, [&](auto actor) {
            return settings->skipped_race.contains(actor->GetRace()->GetFormEditorID());
        }))
    {
        logger::debug("  precondition: race skipped");
        return false;
    }

    logger::debug("  precondition: PASSED");
    return true;
}

bool BleedoutExecutor::submit(RE::Actor* attacker, RE::Actor* victim)
{
    auto settings = Settings::getSingleton();

    logger::debug("  submit: starting animation filtering...");

    // Tag expansion rules (recursive)
    static const std::map<std::string_view, std::vector<std::string_view>> exp_rules = {
        {"a_all", {"a_all_r", "a_all_l"}},
        {"v_all", {"v_all_r", "v_all_l"}},
        {"a_all_r", {"a_1h_r", "a_2h", "a_fist_r", "a_staff_r", "a_spell_r"}},
        {"v_all_r", {"v_1h_r", "v_2h", "v_fist_r", "v_staff_r", "v_spell_r"}},
        {"a_all_l", {"a_1h_l", "a_shield", "a_torch", "a_fist_l", "a_staff_l", "a_spell_l"}},
        {"v_all_l", {"v_1h_l", "v_shield", "v_torch", "v_fist_l", "v_staff_l", "v_spell_l"}},
        {"a_1h_r", {"a_dagger_r", "a_sword_r", "a_axe_r", "a_mace_r"}},
        {"v_1h_r", {"v_dagger_r", "v_sword_r", "v_axe_r", "v_mace_r"}},
        {"a_1h_l", {"a_dagger_l", "a_sword_l", "a_axe_l", "a_mace_l"}},
        {"v_1h_l", {"v_dagger_l", "v_sword_l", "v_axe_l", "v_mace_l"}},
        {"a_2h", {"a_sword2h", "a_axe2h", "a_mace2h"}},
        {"v_2h", {"v_sword2h", "v_axe2h", "v_mace2h"}}
    };

    // Build list of all animation EditorIDs + expanded tags
    std::vector<std::string_view> anims;
    StrMap<StrSet>                exp_tags_map;
    for (auto const& [edid, orig_tags] : anim_tags_map)
    {
        anims.push_back(edid);
        
        StrSet tags = orig_tags;
        bool added = true;
        while (added)
        {
            added = false;
            StrSet new_tags;
            for (const auto& tag : tags)
            {
                auto it = exp_rules.find(tag);
                if (it != exp_rules.end())
                {
                    for (const auto& expanded : it->second)
                    {
                        if (!tags.contains(expanded))
                        {
                            new_tags.insert(std::string{expanded});
                        }
                    }
                }
            }
            if (!new_tags.empty())
            {
                added = true;
                for (const auto& t : new_tags) tags.insert(t);
            }
        }
        exp_tags_map.emplace(edid, tags);
    }

    logger::debug("  submit: {} total animations loaded", anims.size());

    if (anims.empty())
    {
        logger::warn("  submit: No animations registered!");
        return false;
    }

    // Filter by banned tags
    auto pre_count = anims.size();
    std::erase_if(anims, [&](auto& edid) {
        auto it = exp_tags_map.find(edid);
        if (it == exp_tags_map.end())
            return true;
        const auto& tags = it->second;
        return std::ranges::any_of(settings->banned_tags, [&](auto& tag) { return tags.contains(tag); });
    });
    logger::debug("  submit: {} -> {} after banned tag filter", pre_count, anims.size());

    if (anims.empty())
    {
        logger::debug("  submit: No animations left after banned tag filter.");
        return false;
    }

    // Filter by skeleton race compatibility
    pre_count        = anims.size();
    auto att_banned_race = getBannedSkels(attacker, "a_");
    auto vic_banned_race = getBannedSkels(victim, "v_");

    logger::debug("  submit: attacker skeleton race: {}, victim skeleton race: {}",
                  getSkeletonRace(attacker).empty() ? "human" : getSkeletonRace(attacker),
                  getSkeletonRace(victim).empty() ? "human" : getSkeletonRace(victim));

    std::erase_if(anims, [&](auto& edid) {
        auto it = exp_tags_map.find(edid);
        if (it == exp_tags_map.end())
            return true;
        const auto& tags = it->second;
        return std::ranges::any_of(att_banned_race, [&](auto& tag) { return tags.contains(tag); }) ||
            std::ranges::any_of(vic_banned_race, [&](auto& tag) { return tags.contains(tag); });
    });
    logger::debug("  submit: {} -> {} after skeleton filter", pre_count, anims.size());

    if (anims.empty())
    {
        logger::debug("  submit: No animations left after skeleton filter.");
        return false;
    }

    // IdleTagger — evaluate KaputtRoot child idle conditions for weapon/state filtering
    pre_count = anims.size();
    applyIdleTagger(attacker, victim, anims, exp_tags_map);
    logger::debug("  submit: {} -> {} after IdleTagger filter", pre_count, anims.size());

    if (anims.empty())
    {
        logger::debug("  submit: No animations left after IdleTagger filter.");
        return false;
    }

    logger::debug("  submit: {} eligible animations for final selection", anims.size());

    // Random pick
    std::uniform_int_distribution<size_t> pick_dist(0, anims.size() - 1);
    auto                                  edid = anims[pick_dist(rng)];

    auto idle = RE::TESForm::LookupByEditorID<RE::TESIdleForm>(edid);
    if (!idle)
    {
        logger::warn("  submit: Registered animation {} has no corresponding IdleForm!", edid);
        return false;
    }

    // Log victim state before pre-processing
    auto knock_state = victim->AsActorState()->GetKnockState();
    logger::debug("  submit: pre-process | victim knockState={} ragdoll={} bleedout={} dead={}",
                 static_cast<int>(knock_state),
                 victim->IsInRagdollState(),
                 victim->AsActorState()->IsBleedingOut(),
                 victim->IsDead());

    // Pre-process: stop current animations
    attacker->NotifyAnimationGraph("attackStop");
    victim->NotifyAnimationGraph("attackStop");
    attacker->NotifyAnimationGraph("staggerStop");
    victim->NotifyAnimationGraph("staggerStop");

    // Handle getup state
    if ((knock_state == RE::KNOCK_STATE_ENUM::kGetUp) ||
        (knock_state == RE::KNOCK_STATE_ENUM::kQueued))
    {
        victim->AsActorState()->actorState1.knockState = RE::KNOCK_STATE_ENUM::kNormal;
        victim->NotifyAnimationGraph("GetUpEnd");
        logger::debug("  submit: reset victim knockState to Normal + GetUpEnd");
    }

    // Queue the paired idle to play on the NEXT game frame via SKSE task interface.
    // This is CRITICAL: calling _playPairedIdle directly from inside ProcessHitHook
    // causes sync failures because the engine's internal state is mid-processing.
    // Valhalla Combat uses the same approach (SKSE::GetTaskInterface()->AddTask()).
    logger::debug(">>> Queueing killmove {} between {} and {}", edid, attacker->GetName(), victim->GetName());
    auto task = SKSE::GetTaskInterface();
    if (!task)
    {
        logger::error("  submit: SKSE TaskInterface not available!");
        return false;
    }

    std::string edid_str{edid}; // copy to owned string for lambda capture
    task->AddTask([attacker, victim, idle, edid_str]() {
        logger::debug(">>> [TaskQueue] Playing killmove {} now", edid_str);
        bool play_result = _playPairedIdle(
            attacker->GetActorRuntimeData().currentProcess,
            attacker,
            RE::DEFAULT_OBJECT::kActionIdle,
            idle,
            true,
            false,
            victim);

        logger::debug("<<< _playPairedIdle returned: {}", play_result);

        if (!play_result)
        {
            logger::warn("  submit: _playPairedIdle FAILED! The engine rejected the paired idle.");
        }
    });

    return true; // queued successfully
}

void BleedoutExecutor::applyIdleTagger(RE::Actor* attacker, RE::Actor* victim,
                                       std::vector<std::string_view>& anims,
                                       const StrMap<StrSet>& exp_tags_map)
{
    if (!idle_kaputt_root || !idle_kaputt_root->childIdles)
    {
        logger::debug("  IdleTagger: KaputtRoot not available, skipping weapon filter.");
        return;
    }

    logger::debug("  IdleTagger: evaluating {} child tagger conditions...", 
                 idle_kaputt_root->childIdles->size());

    StrMap<bool>             item_results = {};
    RE::ConditionCheckParams params(attacker->As<RE::TESObjectREFR>(), victim->As<RE::TESObjectREFR>());

    for (auto const form : *idle_kaputt_root->childIdles)
    {
        if (anims.empty())
            break;

        auto             idle_form = form->As<RE::TESIdleForm>();
        if (!idle_form)
            continue;

        std::string_view idle_edid = idle_form->GetFormEditorID();
        auto&            flags     = idle_form->data.flags;

        // Evaluate condition
        bool result = true;
        if (flags.all(RE::IDLE_DATA::Flag::kSequence)) // check each individually
        {
            bool or_cache = false;
            for (auto cond_item = idle_form->conditions.head; cond_item != nullptr; cond_item = cond_item->next)
            {
                auto& cond_data = cond_item->data;

                bool single_result;
                if (cond_data.flags.swapTarget && (cond_data.functionData.function == RE::FUNCTION_DATA::FunctionID::kGetGraphVariableInt)) // reference checked item
                {
                    std::string_view ref_item = static_cast<RE::BSString*>(cond_data.functionData.params[0])->c_str();
                    if (item_results.contains(ref_item))
                    {
                        single_result = item_results.find(ref_item)->second;
                        single_result = single_result == (bool)(cond_data.comparisonValue.f);
                        single_result = single_result == (cond_data.flags.opCode == RE::CONDITION_ITEM_DATA::OpCode::kEqualTo);
                    }
                    else
                    {
                        single_result = false;
                    }
                }
                else
                    single_result = cond_item->IsTrue(params);

                or_cache |= single_result;
                if (!cond_item->next || !cond_data.flags.isOR)
                {
                    result &= or_cache;
                    or_cache = false;
                }
            }
        }
        else
            result = idle_form->conditions.IsTrue(attacker->As<RE::TESObjectREFR>(), victim->As<RE::TESObjectREFR>());

        // Extract tag from EditorID (everything after first '_')
        if (auto tag_idx = idle_edid.find_first_of('_'); tag_idx != std::string::npos)
        {
            std::string_view tag{&idle_edid[tag_idx + 1]};
            std::string_view req_tag = result ? tag : std::string_view{};
            std::string_view ban_tag = (!result && flags.all(RE::IDLE_DATA::Flag::kNoAttacking)) ? tag : std::string_view{};

            if (!(req_tag.empty() && ban_tag.empty()))
            {
                if (flags.all(RE::IDLE_DATA::Flag::kBlocking))
                    std::swap(req_tag, ban_tag);

                auto before = anims.size();
                std::erase_if(anims, [&](auto edid) {
                    auto it = exp_tags_map.find(edid);
                    if (it == exp_tags_map.end())
                        return true;
                    return (!req_tag.empty() && !it->second.contains(req_tag)) ||
                        (!ban_tag.empty() && it->second.contains(ban_tag));
                });

                // Always log tag evaluation results at info level
                logger::debug("  IdleTagger: [{}] cond={} | {} tag='{}' | {} -> {} anims",
                             idle_edid, result,
                             req_tag.empty() ? "BAN" : "REQ",
                             req_tag.empty() ? ban_tag : req_tag,
                             before, anims.size());
            }
            else
            {
                // result=false but kNoAttacking not set, so no ban either — log it
                logger::debug("  IdleTagger: [{}] cond={} | no action (no require, no ban)", idle_edid, result);
            }
        }
        else
        {
            logger::debug("  IdleTagger: [{}] cond={} | no tag in EditorID", idle_edid, result);
        }

        item_results.emplace(idle_form->GetFormEditorID(), result);
    }
}

bool BleedoutExecutor::lottery()
{
    auto prob = Settings::getSingleton()->execution_probability;
    if (prob >= 100.f)
        return true;
    if (prob <= 0.f)
        return false;
    return dist(rng) < prob;
}

} // namespace bleedout_exec
