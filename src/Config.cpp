#include "Config.h"

constexpr const char* HDR_GENERAL = "General";
constexpr const char* HDR_WIND = "Wind";
constexpr const char* HDR_PERFORMANCE = "Performance";

constexpr bool DEFAULTSB[wind::Config::BOOL_COUNT]
{
	false,
	false,
	true,
};

constexpr const char* KEYSB[wind::Config::BOOL_COUNT]
{
	"bMassIndependent",
	"bLogPerformance",
	"bAutoSave",
};

constexpr const char* HDRSB[wind::Config::BOOL_COUNT]
{
	HDR_WIND,
	HDR_PERFORMANCE,
	HDR_GENERAL,
};

constexpr float DEFAULTSF[wind::Config::FLOAT_COUNT]
{
	10.0f,
	0.67f,
	4.29f,
	0.55f,
	8.93f,
	1.0f,
	1.0f,
	1.0f,
};

constexpr const char* KEYSF[wind::Config::FLOAT_COUNT]
{
	"fOverallForce",
	"fOsc01Force",
	"fOsc01Frequency",
	"fOsc02Force",
	"fOsc02Frequency",
	"fOsc02Span",
	"fNoise",
	"fHeightFactor",
};

constexpr const char* HDRSF[wind::Config::FLOAT_COUNT]
{
	HDR_WIND,
	HDR_WIND,
	HDR_WIND,
	HDR_WIND,
	HDR_WIND,
	HDR_WIND,
	HDR_WIND,
	HDR_WIND,
};

constexpr int DEFAULTSI[wind::Config::INT_COUNT]
{
	0,
	4,
};

constexpr const char* KEYSI[wind::Config::INT_COUNT]
{
	"iMultithreadThreshold",
	"iThreads",
};

constexpr const char* HDRSI[wind::Config::INT_COUNT]
{
	HDR_PERFORMANCE,
	HDR_PERFORMANCE,
};

wind::Config::Config()
{
	for (int i = 0; i < BOOL_COUNT; i++) {
		m_bools[i] = DEFAULTSB[i];
	}
	for (int i = 0; i < FLOAT_COUNT; i++) {
		m_floats[i] = DEFAULTSF[i];
	}
	for (int i = 0; i < INT_COUNT; i++) {
		m_ints[i] = DEFAULTSI[i];
	}
}

std::filesystem::path wind::ConfigPath()
{
	// Resolve the INI against our own DLL location so reads AND WinAPI writes
	// (WritePrivateProfileString) both hit <game>/Data/SKSE/Plugins/SMP Wind.ini.
	// (Relative paths would resolve against C:\Windows for the WinAPI calls.)
	HMODULE module = nullptr;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&wind::ConfigPath), &module) &&
		module) {
		wchar_t buf[MAX_PATH]{};
		if (GetModuleFileNameW(module, buf, MAX_PATH)) {
			return std::filesystem::path(buf).parent_path() / "SMP Wind.ini";
		}
	}

	// Fallback: current working directory (game root when launched normally).
	return std::filesystem::current_path() / "SKSE" / "Plugins" / "SMP Wind.ini";
}

bool wind::Config::load(const std::filesystem::path& path)
{
	if (path.extension().string() != ".ini") {
		return false;
	}
	else {
		// Normalize to absolute up front so the WinAPI INI calls in set()/save()
		// target the real file instead of C:\Windows.
		std::error_code ec;
		m_path = std::filesystem::absolute(path, ec);
		if (ec) {
			m_path = path;
		}

		if (std::filesystem::exists(path)) {
			// Read through SimpleINI, not WinAPI: GetPrivateProfileString
			// caches file contents per process, so after our own SaveFile
			// writes it would keep returning stale values on Reload
			// (a fresh game start works only because the cache is empty).
			m_ini.Reset();
			m_ini.LoadFile(m_path.string().c_str());

			for (int i = 0; i < BOOL_COUNT; i++) {
				m_bools[i] = m_ini.GetBoolValue(HDRSB[i], KEYSB[i], DEFAULTSB[i]);
			}

			for (int i = 0; i < FLOAT_COUNT; i++) {
				const char* raw = m_ini.GetValue(HDRSF[i], KEYSF[i], nullptr);
				if (!raw) {
					m_floats[i] = DEFAULTSF[i];
				}
				else {
					errno = 0;
					char* end = nullptr;
					float result = std::strtof(raw, &end);
					if (!end || *end != '\0' || errno == ERANGE) {
						logger::warn("WARNING: invalid float value %s for %s. Using default.", raw, KEYSF[i]);
						m_floats[i] = DEFAULTSF[i];
					}
					else {
						m_floats[i] = result;
					}
				}
			}
			for (int i = 0; i < INT_COUNT; i++) {
				const char* raw = m_ini.GetValue(HDRSI[i], KEYSI[i], nullptr);
				if (!raw) {
					m_ints[i] = DEFAULTSI[i];
				}
				else {
					errno = 0;
					char* end = nullptr;
					long result = std::strtol(raw, &end, 10);
					if (!end || *end != '\0' || errno == ERANGE) {
						logger::warn("WARNING: invalid integer value %s for %s. Using default.", raw, KEYSI[i]);
						m_ints[i] = DEFAULTSI[i];
					}
					else {
						m_ints[i] = static_cast<int>(result);
					}
				}
			}

			m_boneFactors.clear();
			CSimpleIniA::TNamesDepend keys;
			m_ini.GetAllKeys("Bones", keys);
			for (const auto& key : keys) {
				if (!key.pItem) {
					continue;
				}
				const char* raw = m_ini.GetValue("Bones", key.pItem, "1");
				const float f = std::max(std::strtof(raw, nullptr), 0.0f);
				if (f != 1.0f) {
					m_boneFactors[key.pItem] = f;
					logger::info("Scaling wind on bone \"%s\" by a factor %g.", key.pItem, f);
				}
			}
		}
		else {
			for (int i = 0; i < BOOL_COUNT; i++) {
				set(i, DEFAULTSB[i]);
			}
			for (int i = 0; i < FLOAT_COUNT; i++) {
				set(i, DEFAULTSF[i]);
			}
			for (int i = 0; i < INT_COUNT; i++) {
				set(i, DEFAULTSI[i]);
			}
		}
		return true;
	}
}

bool wind::Config::saveAs(const std::filesystem::path& path)
{
	if (path.extension().string() != ".ini") {
		return false;
	}

	std::error_code ec;
	m_path = std::filesystem::absolute(path, ec);
	if (ec) {
		m_path = path;
	}
	return save();
}

bool wind::Config::save()
{
	if (m_path.empty()) {
		return false;
	}

	for (int i = 0; i < BOOL_COUNT; i++) {
		m_ini.SetBoolValue(HDRSB[i], KEYSB[i], m_bools[i]);
	}
	for (int i = 0; i < FLOAT_COUNT; i++) {
		m_ini.SetDoubleValue(HDRSF[i], KEYSF[i], static_cast<double>(m_floats[i]));
	}
	for (int i = 0; i < INT_COUNT; i++) {
		m_ini.SetLongValue(HDRSI[i], KEYSI[i], static_cast<long>(m_ints[i]));
	}

	// Rewrite the [Bones] section from the live map (load() only keeps factors != 1.0).
	// Also drop a literally-"[Bones]" section: the old WinAPI save() passed the
	// brackets as part of the name, producing a "[[Bones]]" artifact.
	m_ini.Delete("Bones", nullptr);
	m_ini.Delete("[Bones]", nullptr);
	for (const auto& [name, factor] : m_boneFactors) {
		m_ini.SetDoubleValue("Bones", name.c_str(), static_cast<double>(factor));
	}

	return m_ini.SaveFile(m_path.string().c_str()) == SI_OK;
}

void wind::Config::set(int id, bool b)
{
	assert(id >= 0 && id < BOOL_COUNT);

	m_bools[id] = b;

	// The autosave flag itself always persists; everything else only when autosave is on.
	if ((id == AUTOSAVE || m_bools[AUTOSAVE]) && !m_path.empty()) {
		m_ini.SetBoolValue(HDRSB[id], KEYSB[id], b);
		m_ini.SaveFile(m_path.string().c_str());
	}
}

void wind::Config::set(int id, float f)
{
	assert(id >= 0 && id < FLOAT_COUNT);

	m_floats[id] = f;

	if (m_bools[AUTOSAVE] && !m_path.empty()) {
		m_ini.SetDoubleValue(HDRSF[id], KEYSF[id], static_cast<double>(f));
		m_ini.SaveFile(m_path.string().c_str());
	}
}

void wind::Config::set(int id, int i)
{
	assert(id >= 0 && id < INT_COUNT);

	m_ints[id] = i;

	if (m_bools[AUTOSAVE] && !m_path.empty()) {
		m_ini.SetLongValue(HDRSI[id], KEYSI[id], static_cast<long>(i));
		m_ini.SaveFile(m_path.string().c_str());
	}
}
