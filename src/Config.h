#pragma once

#include <SimpleIni.h>

namespace wind
{
	// Absolute path of the INI next to our DLL: <game>/Data/SKSE/Plugins/SMP Wind.ini
	std::filesystem::path ConfigPath();

	class Config
	{
	public:
		enum BoolID : int
		{
			MASS_INDEPENDENT,
			LOG_PERFORMANCE,
			AUTOSAVE,
			BOOL_COUNT
		};

		enum FloatID : int
		{
			FORCE,
			OSC01FORCE,
			OSC01FREQ,
			OSC02FORCE,
			OSC02FREQ,
			OSC02SPAN,
			NOISE,
			HEIGHTFACTOR,
			FLOAT_COUNT
		};
		
		enum IntID : int
		{
			MULTITHREAD_THRESHOLD,
			THREADS,
			INT_COUNT
		};

	public:
		Config();
		~Config() = default;

		bool  load(const std::filesystem::path& path);
		bool  save();
		bool  saveAs(const std::filesystem::path& path);
		const std::filesystem::path& path() const { return m_path; }
		bool  getb(int id) const { assert(id >= 0 && id < BOOL_COUNT); return m_bools[id]; }
		void  set(int id, bool b);
		float getf(int id) const { assert(id >= 0 && id < FLOAT_COUNT); return m_floats[id]; }
		void  set(int id, float f);
		int   geti(int id) const { assert(id >= 0 && id < INT_COUNT); return m_ints[id]; }
		void  set(int id, int i);

		bool  hasBoneFactors() const { return !m_boneFactors.empty(); }
		
		float getBoneFactor(const char* name) const
		{
			if (auto it = m_boneFactors.find(std::string(name)); it != m_boneFactors.end()) {
				return it->second;
			}
			else {
				return 1.0;
			}
		}
		void setBoneFactor(const char* name, float f)
		{
			m_boneFactors[std::string(name)] = f;
			if (!m_path.empty()) {
				m_ini.SetDoubleValue("Bones", name, static_cast<double>(f));
				if (m_bools[AUTOSAVE]) {
					m_ini.SaveFile(m_path.string().c_str());
				}
			}
		}

	private:
		std::filesystem::path m_path;
		float                 m_floats[FLOAT_COUNT];
		int                   m_ints[INT_COUNT];
		bool                  m_bools[BOOL_COUNT];

		// Live INI document (SimpleINI). load() parses the file into it, set()
		// updates it and saves, so the file on disk always mirrors the arrays.
		CSimpleIniA m_ini;

		std::map<std::string, float> m_boneFactors;
	};
}
