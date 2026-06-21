#pragma once

#include <nlohmann/json.hpp>
#include <random>

namespace bleedout_exec
{
using json = nlohmann::json;

class BleedoutExecutor
{
public:
    static BleedoutExecutor* getSingleton()
    {
        static BleedoutExecutor executor;
        return std::addressof(executor);
    }

    bool init();
    bool isReady() { return ready.load(); }

    // Main processing function — called from ProcessHitHook (post-hit)
    bool process(RE::Actor* victim, RE::HitData& hit_data);
    // Primary trigger — called from AttackActionHook (pre-swing, before physics)
    void processAttackAction(RE::TESActionData* action_data);

private:
    std::atomic_bool ready{false};

    // Animation registry: EditorID -> set of tags
    StrMap<StrSet> anim_tags_map;

    // KaputtRoot idle form — used for IdleTagger weapon/condition filtering
    RE::TESIdleForm* idle_kaputt_root = nullptr;

    // Random engine
    std::mt19937                          rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist{0.f, 100.f};

    // Core functions
    bool loadAnims();
    bool loadKaputtRoot();
    bool precondition(const RE::Actor* attacker, const RE::Actor* victim);
    bool submit(RE::Actor* attacker, RE::Actor* victim);
    bool lottery();

    // IdleTagger — evaluates conditions on KaputtRoot child idles
    void applyIdleTagger(RE::Actor* attacker, RE::Actor* victim,
                         std::vector<std::string_view>& anims,
                         const StrMap<StrSet>& exp_tags_map);
};

} // namespace bleedout_exec
