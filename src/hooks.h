#pragma once

namespace bleedout_exec
{

/* ---------------- HOOKS ---------------- */

struct ProcessHitHook
{
    static void                                    thunk(RE::Actor* a_victim, RE::HitData& a_hitData);
    static inline REL::Relocation<decltype(thunk)> func;

    static constexpr auto id     = RELOCATION_ID(37673, 38627);
    static constexpr auto offset = REL::VariantOffset(0x3c0, 0x4a8, 0x0);
};

// AttackActionHook — intercepts the attack ACTION before the weapon swings.
// This is Kaputt's primary trigger mechanism for bleedout/getup execution.
// By hooking here, we can start the paired animation BEFORE any physics
// collision occurs, which is essential for proper animation sync.
struct AttackActionHook
{
    static bool                                    thunk(RE::TESActionData* a_actionData);
    static inline REL::Relocation<decltype(thunk)> func;

    static constexpr auto id     = RELOCATION_ID(48139, 49170);
    static constexpr auto offset = REL::VariantOffset(0x4d7, 0x435, 0x0);
};

/* ------------- ENGINE FUNC ------------- */

inline bool _playPairedIdle(RE::AIProcess* proc, RE::Actor* attacker, RE::DEFAULT_OBJECT smth, RE::TESIdleForm* idle, bool a5, bool a6, RE::TESObjectREFR* target)
{
    using func_t = decltype(&_playPairedIdle);
    REL::Relocation<func_t> func{RELOCATION_ID(38290, 39256)};
    return func(proc, attacker, smth, idle, a5, a6, target);
}

/* ------------- UTILITY FUNC ------------- */

bool isInPairedAnimation(const RE::Actor* actor);
bool animPlayable(const RE::Actor* actor);
bool isLastHostileInRange(const RE::Actor* attacker, const RE::Actor* victim, float range);

std::string getSkeletonRace(const RE::Actor* actor);
StrSet      getBannedSkels(const RE::Actor* actor, std::string_view prefix);

} // namespace bleedout_exec
