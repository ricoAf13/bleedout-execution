#include <spdlog/sinks/basic_file_sink.h>

#include "executor.h"
#include "hooks.h"
#include "settings.h"

namespace bleedout_exec
{

bool installLog()
{
    auto path = logger::log_directory();
    if (!path)
        return false;

    *path /= std::format("{}.log", SKSE::PluginDeclaration::GetSingleton()->GetName());
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S:%e][%5l] %v"s);

    return true;
}

void processMessage(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type)
    {
        case SKSE::MessagingInterface::kDataLoaded:
        {
            logger::info("Game: data loaded");

            auto executor = BleedoutExecutor::getSingleton();
            if (!executor->init())
                logger::warn("Something went wrong while initializing BleedoutExecution. Check the log.");

            if (executor->isReady())
            {
                logger::info("Installing ProcessHitHook...");
                stl::write_thunk_call<ProcessHitHook>();
                logger::info("ProcessHitHook installed.");

                logger::info("Installing AttackActionHook...");
                stl::write_thunk_call<AttackActionHook>();
                logger::info("AttackActionHook installed.");
            }
            else
            {
                logger::error("BleedoutExecution not ready — hook NOT installed.");
            }

            break;
        }
        default:
            break;
    }
}

} // namespace bleedout_exec

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    using namespace bleedout_exec;

    installLog();

    const auto plugin = SKSE::PluginDeclaration::GetSingleton();
    logger::info("{} v{} is loading...", plugin->GetName(), plugin->GetVersion());

    SKSE::Init(a_skse);
    SKSE::AllocTrampoline(28); // 2 hooks * 14 bytes

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", processMessage))
        return false;

    logger::info("{} loaded.", plugin->GetName());

    return true;
}
