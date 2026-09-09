#include "UI/SMPWindMenu.h"

#include "Config.h"
#include "Wind.h"

#include <algorithm>
#include <string>

#include "SKSEMenuFramework.h"

namespace wind
{
	extern Config g_config;
	extern Wind   g_wind;
}

// In-DLL configuration UI, following the FSMP pattern: the SKSE Menu Framework
// owns the D3D11/ImGui hook and calls our render functions while its panel is
// open; we emit ImGui widgets (under the ImGuiMCP namespace, forwarded into
// SKSEMenuFramework.dll) bound to the live wind config. Every edit goes through
// Config::set(), so it persists to "SKSE\\Plugins\\SMP Wind.ini" immediately,
// exactly like the JGWD_MCM Papyrus setters.

namespace
{
	using wind::Config;

	// ---- Small helpers (same policy as FSMPMenu: toggles commit on click,
	// sliders preview live, thread count re-applies on release) ----

	void tip(const char* english)
	{
		if (ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled))
			ImGuiMCP::SetTooltip("%s", english);
	}

	std::string hid(const char* label)
	{
		return std::string("##") + label;
	}

	bool beginRows(const char* id)
	{
		if (!ImGuiMCP::BeginTable(id, 2, ImGuiMCP::ImGuiTableFlags_PadOuterX))
			return false;
		ImGuiMCP::TableSetupColumn("label", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 260.0f);
		ImGuiMCP::TableSetupColumn("control", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
		return true;
	}

	void endRows()
	{
		ImGuiMCP::EndTable();
	}

	void rowLabel(const char* label, const char* help)
	{
		ImGuiMCP::TableNextRow();
		ImGuiMCP::TableNextColumn();
		ImGuiMCP::Text("%s", label);
		tip(help);
		ImGuiMCP::TableNextColumn();
		ImGuiMCP::SetNextItemWidth(-FLT_MIN);
	}

	bool rowCheck(const char* label, const char* help, bool* v)
	{
		rowLabel(label, help);
		const bool changed = ImGuiMCP::Checkbox(hid(label).c_str(), v);
		tip(help);
		return changed;
	}

	bool rowFloat(const char* label, const char* help, float* v, float lo, float hi, const char* fmt)
	{
		rowLabel(label, help);
		const bool changed = ImGuiMCP::SliderFloat(hid(label).c_str(), v, lo, hi, fmt);
		tip(help);
		return changed;
	}

	bool rowInt(const char* label, const char* help, int* v, int lo, int hi)
	{
		rowLabel(label, help);
		const bool changed = ImGuiMCP::SliderInt(hid(label).c_str(), v, lo, hi);
		tip(help);
		return changed;
	}

	void section(const char* title)
	{
		ImGuiMCP::NewLine();
		ImGuiMCP::TextColored(ImGuiMCP::ImVec4{ 0.40f, 0.72f, 1.00f, 1.00f }, "%s", title);
		ImGuiMCP::Separator();
	}

	// Shared footer rendered at the bottom of every page: auto-save toggle,
	// manual save and reload side by side, so no separate Config page is needed.
	void ConfigFooter()
	{
		auto& cfg = wind::g_config;

		ImGuiMCP::Spacing();
		ImGuiMCP::Separator();

		bool autosave = cfg.getb(Config::AUTOSAVE);
		if (ImGuiMCP::Checkbox("Auto-save", &autosave))
			cfg.set(Config::AUTOSAVE, autosave);
		tip("Persist every change to the INI immediately. When off, use Save.");
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Save"))
			cfg.save();
		tip("Write all current settings to the INI file.");
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Reload")) {
			cfg.load(cfg.path());
			wind::g_wind.updateThreadCount();
		}
		tip("Discard unsaved changes and reload the INI file.");
		if (!cfg.getb(Config::AUTOSAVE)) {
			ImGuiMCP::Spacing();
			ImGuiMCP::TextDisabled("%s", "Auto-save is off: changes need Save to survive a restart.");
		}
	}

	void WindBody()
	{
		auto& cfg = wind::g_config;

		section("Wind force");
		if (beginRows("smpwind.wind")) {
			float v = cfg.getf(Config::FORCE);
			if (rowFloat("Overall force", "Base wind strength multiplier.", &v, 0.0f, 100.0f, "%.1f"))
				cfg.set(Config::FORCE, v);
			v = cfg.getf(Config::HEIGHTFACTOR);
			if (rowFloat("Height factor", "How much wind speed grows with altitude.", &v, 0.0f, 10.0f, "%.2f"))
				cfg.set(Config::HEIGHTFACTOR, v);
			v = cfg.getf(Config::NOISE);
			if (rowFloat("Noise", "Random per-bone turbulence amount.", &v, 0.0f, 10.0f, "%.2f"))
				cfg.set(Config::NOISE, v);
			bool b = cfg.getb(Config::MASS_INDEPENDENT);
			if (rowCheck("Mass independent", "Scale force by 100x mass so heavy and light bones move alike.", &b))
				cfg.set(Config::MASS_INDEPENDENT, b);
			endRows();
		}

		section("Gust oscillation 1 (along-wind surge)");
		if (beginRows("smpwind.osc1")) {
			float v = cfg.getf(Config::OSC01FORCE);
			if (rowFloat("Osc 1 force", "Surge strength along the wind direction.", &v, 0.0f, 5.0f, "%.2f"))
				cfg.set(Config::OSC01FORCE, v);
			v = cfg.getf(Config::OSC01FREQ);
			if (rowFloat("Osc 1 frequency", "Surge frequency.", &v, 0.0f, 20.0f, "%.2f"))
				cfg.set(Config::OSC01FREQ, v);
			endRows();
		}

		section("Gust oscillation 2 (direction spread)");
		if (beginRows("smpwind.osc2")) {
			float v = cfg.getf(Config::OSC02FORCE);
			if (rowFloat("Osc 2 force", "Cross-wind gust strength.", &v, 0.0f, 5.0f, "%.2f"))
				cfg.set(Config::OSC02FORCE, v);
			v = cfg.getf(Config::OSC02FREQ);
			if (rowFloat("Osc 2 frequency", "Cross-wind gust frequency.", &v, 0.0f, 20.0f, "%.2f"))
				cfg.set(Config::OSC02FREQ, v);
			v = cfg.getf(Config::OSC02SPAN);
			if (rowFloat("Osc 2 span", "How far the wind angle swings (radians).", &v, 0.0f, 6.29f, "%.2f"))
				cfg.set(Config::OSC02SPAN, v);
			endRows();
		}

		ConfigFooter();
	}

	void PerformanceBody()
	{
		auto& cfg = wind::g_config;

		section("Multithreading");
		if (beginRows("smpwind.perf")) {
			int v = cfg.geti(Config::THREADS);
			if (rowInt("Threads", "Worker threads for wind evaluation (1 = single-threaded).", &v, 1, 128)) {
				cfg.set(Config::THREADS, v);
				wind::g_wind.updateThreadCount();
			}
			v = cfg.geti(Config::MULTITHREAD_THRESHOLD);
			if (rowInt("Multithread threshold", "Use the thread pool only above this many collision objects.", &v, 0, 10000))
				cfg.set(Config::MULTITHREAD_THRESHOLD, v);
			bool b = cfg.getb(Config::LOG_PERFORMANCE);
			if (rowCheck("Log performance", "Log mean update time every 120 frames.", &b))
				cfg.set(Config::LOG_PERFORMANCE, b);
			endRows();
		}

		ConfigFooter();
	}
}

void wind::SMPWindMenu::Register()
{
	if (!SKSEMenuFramework::IsInstalled())
		return;

	SKSEMenuFramework::SetSection("SMP Wind");
	SKSEMenuFramework::AddSectionItem("Wind", WindBody);
	SKSEMenuFramework::AddSectionItem("Performance", PerformanceBody);
}
