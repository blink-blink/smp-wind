#pragma once

namespace wind::SMPWindMenu
{
	// Register SMP Wind's pages with the SKSE Menu Framework's Mod Control Panel:
	// one "SMP Wind" section with Wind / Performance pages bound to the live
	// wind::g_config (INI-backed, same path as `SetFloat`/`SetInt` Papyrus calls).
	// Call once, after the framework is up (kPostPostLoad). Safe to call when the
	// framework is not installed --- it simply no-ops via the SDK.
	void Register();
}
