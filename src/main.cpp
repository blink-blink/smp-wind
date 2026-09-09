#include "PluginHelper.h"
#include "Config.h"
#include "Wind.h"
#include "Papyrus.h"
#include "UI/SMPWindMenu.h"

constexpr unsigned long VERSION_MAJOR{ 2 };
constexpr unsigned long VERSION_MINOR{ 3 };
constexpr unsigned long VERSION_PATCH{ 0 };
constexpr unsigned long VERSION = (VERSION_MAJOR & 0xFF) << 24 | (VERSION_MINOR & 0xFF) << 16 | (VERSION_PATCH & 0xFF) << 8;

inline static hdt::PluginInterface::Version interfaceMin{ 1, 0, 0 };
inline static hdt::PluginInterface::Version interfaceMax{ 3, 0, 0 };

inline static hdt::PluginInterface::Version bulletMin{ hdt::PluginInterface::BULLET_VERSION };
inline static hdt::PluginInterface::Version bulletMax{ hdt::PluginInterface::BULLET_VERSION.major + 1, 0, 0 };

namespace wind
{
	Config g_config;
	Config g_configDefault;
	Wind   g_wind;
}

namespace
{
	void InitializeLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

		//
		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
	}
}

void SMP_MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg && a_msg->type == hdt::PluginInterface::MSG_STARTUP && a_msg->data) 
	{
		auto* smp = static_cast<hdt::PluginInterface*>(a_msg->data);
		const auto& info = smp->getVersionInfo();

		logger::info("Received hdtSMP64 startup message: interface v{}.{}.{}, Bullet v{}.{}.{}",
			info.interfaceVersion.major, info.interfaceVersion.minor, info.interfaceVersion.patch,
			info.bulletVersion.major, info.bulletVersion.minor, info.bulletVersion.patch);

		//
		if (info.interfaceVersion >= interfaceMin && info.interfaceVersion < interfaceMax) 
		{
			if (info.bulletVersion >= bulletMin && info.bulletVersion < bulletMax) 
			{
				logger::info(".\n");

				// Absolute path next to our DLL (Data/SKSE/Plugins/SMP Wind.ini).
				// The legacy relative path resolved against the game root, so pick
				// it up once if the new location doesn't exist yet.
				const auto iniPath = wind::ConfigPath();
				const std::filesystem::path legacyPath("SKSE\\Plugins\\SMP Wind.ini");
				std::error_code ec;
				const bool isNew = !std::filesystem::exists(iniPath, ec);
				const std::filesystem::path loadPath =
					(isNew && std::filesystem::exists(legacyPath, ec)) ? legacyPath : iniPath;

				//
				logger::info("Loading settings...");
				if (wind::g_config.load(loadPath))
				{
					logger::info("Settings loaded.\n");
				}
				else
				{
					logger::warn("WARNING: Failed to load config file. Settings will not be saved.\n");
				}

				if (isNew) {
					if (loadPath == iniPath) {
						// First run: generate the INI with current values.
						wind::g_config.save();
					}
					else {
						// Migrated from the legacy location: keep the values, move the file.
						wind::g_config.saveAs(iniPath);
					}
				}
		
				wind::g_wind.init(wind::g_config);
				smp->addListener(&wind::g_wind);
				logger::info("Initialisation complete. Wind listener registered.\n");
			} 
			else 
			{
				logger::error("ERROR: Incompatible Bullet version.");
			}
		} 
		else 
		{
			logger::error("ERROR: Incompatible HDT-SMP interface.");
		}
	}
}

void SKSE_MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg && a_msg->type == SKSE::MessagingInterface::kPostPostLoad)
	{
		// Register our config pages with the SKSE Menu Framework, if installed. No-ops otherwise.
		wind::SMPWindMenu::Register();
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION.pack();

	if (a_skse->IsEditor()) 
	{
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() 
{
	SKSE::PluginVersionData v;

	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.UsesAddressLibrary();
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST_SE, SKSE::RUNTIME_SSE_LATEST, SKSE::RUNTIME_1_6_1179, SKSE::RUNTIME_LATEST_VR });
	v.UsesNoStructs();

	return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	InitializeLog();
	logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());

	const auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", SKSE_MessageHandler)) 
	{
		return false;
	}

	// Subscribe to hdtSMP64 startup messages here at load time, NOT in the
	// kPostPostLoad handler: FSMP dispatches MSG_STARTUP during kPostPostLoad,
	// and hdtSMP64.dll sorts before smpwind.dll, so registering there races
	// (and loses) against FSMP's broadcast.
	if (!messaging->RegisterListener("hdtSMP64", SMP_MessageHandler))
	{
		logger::warn("WARNING: Failed to register hdtSMP64 listener. Wind will not work.\n");
	}

	const auto papyrus = SKSE::GetPapyrusInterface();
	if (papyrus) 
	{
		papyrus->Register(wind::Register);
	}

	return true;
}
