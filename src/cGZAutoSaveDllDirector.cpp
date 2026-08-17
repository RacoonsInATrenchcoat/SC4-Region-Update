////////////////////////////////////////////////////////////////////////
//
// This file is part of SC4RegionUpdate, a DLL Plugin for SimCity 4
// that synchronises region tile data across a region.
//
// Copyright (c) 2026 Tamas Tabi
//
// This file is derived from sc4-auto-save by Nicholas Hayes (0xC0000054),
// used under the terms of the MIT License. See LICENSE.txt and
// Third Party Notices.txt for more information.
//
////////////////////////////////////////////////////////////////////////

// Framework and game interface headers this plugin needs.
// (Auto-save-specific includes for its timer/settings/service were removed.)
#include "Logger.h"                    // Writes status/debug lines to a .log file.
#include "version.h"                   // Provides PLUGIN_VERSION_STR.
#include "cIGZFrameWork.h"             // The game's framework, for hooking in at startup.
#include "cIGZApp.h"
#include "cISC4City.h"                 // Represents a loaded city tile.
#include "cIGZMessageServer2.h"        // Lets us subscribe to game event messages.
#include "cIGZMessage2.h"
#include "cIGZMessage2Standard.h"
#include "cRZMessage2COMDirector.h"    // Base class: a plugin director that handles messages.
#include "GZServPtrs.h"
#include <filesystem>
#include <vector>
#include <Windows.h>
#include "wil/resource.h"
#include "wil/filesystem.h"

// Message ID the game broadcasts when a city has finished loading.
// This is the single event this hello-world version reacts to.
static constexpr uint32_t kSC4MessagePostCityInit = 0x26d31ec1;

// This plugin's unique identifier. Must not clash with any other installed
// DLL plugin. Randomly generated for SC4RegionUpdate.
static constexpr uint32_t kRegionUpdatePluginDirectorID = 0xE04809A9;

// Name of the log file this plugin writes (created next to the DLL).
static constexpr std::string_view PluginLogFileName = "SC4RegionUpdate.log";

// The director IS the plugin. The game creates one of these and talks to it.
// Inherits from cRZMessage2COMDirector so it can receive game event messages.
class cRegionUpdateDllDirector : public cRZMessage2COMDirector
{
public:

	// Runs once when the plugin object is created: sets up the log file.
	cRegionUpdateDllDirector()
	{
		std::filesystem::path dllFolder = GetDllFolderPath();

		std::filesystem::path logFilePath = dllFolder;
		logFilePath /= PluginLogFileName;   // Put the log next to the DLL.

		Logger& logger = Logger::GetInstance();

		logger.Init(logFilePath, LogLevel::Info);
		logger.WriteLogFileHeader("SC4RegionUpdate v" PLUGIN_VERSION_STR);
	}

	// The game uses this to identify the plugin. Returns our unique ID.
	uint32_t GetDirectorID() const
	{
		return kRegionUpdatePluginDirectorID;
	}

	// Called when a city finishes loading. This is where THIS plugin's
	// behaviour lives. For now it just logs and shows a confirmation box;
	// later this is where the region-refresh logic will go.
	void PostCityInit(cIGZMessage2Standard* pStandardMsg)
	{
		// The message carries a pointer to the city that was loaded.
		cISC4City* pCity = reinterpret_cast<cISC4City*>(pStandardMsg->GetIGZUnknown());

		if (pCity)
		{
			Logger::GetInstance().WriteLine(LogLevel::Info, "PostCityInit fired: a city was loaded.");

			// Visible proof the plugin ran. Hello-world milestone.
			MessageBoxA(
				nullptr,
				"SC4RegionUpdate is loaded and a city was entered.",
				"SC4RegionUpdate",
				MB_OK | MB_ICONINFORMATION);
		}
	}

	// The "switchboard": every subscribed game event arrives here, and we
	// route it to the right handler based on its type. Only one case for now.
	bool DoMessage(cIGZMessage2* pMessage)
	{
		cIGZMessage2Standard* pStandardMsg = static_cast<cIGZMessage2Standard*>(pMessage);
		uint32_t dwType = pMessage->GetType();

		switch (dwType)
		{
		case kSC4MessagePostCityInit:
			PostCityInit(pStandardMsg);
			break;
		}

		return true;
	}

	// Runs once at application startup. This is where we tell the game which
	// events we want to be notified about (here: just "city loaded").
	bool PostAppInit()
	{
		cIGZMessageServer2Ptr pMsgServ;   // Handle to the game's message system.
		if (pMsgServ)
		{
			// Subscribe to the city-loaded event so DoMessage() will receive it.
			if (!pMsgServ->AddNotification(this, kSC4MessagePostCityInit))
			{
				MessageBoxA(nullptr, "Failed to subscribe to the required notifications.", "SC4RegionUpdate", MB_OK | MB_ICONERROR);
				return false;
			}
		}
		else
		{
			// The message system wasn't available; the plugin can't function.
			MessageBoxA(nullptr, "Failed to subscribe to the required notifications.", "SC4RegionUpdate", MB_OK | MB_ICONERROR);
			return false;
		}

		return true;
	}

	// Called very early by the framework. Hooks this director into the game
	// so it will receive the lifecycle callbacks above. Standard boilerplate.
	bool OnStart(cIGZCOM* pCOM)
	{
		cIGZFrameWork* const pFramework = RZGetFrameWork();

		if (pFramework->GetState() < cIGZFrameWork::kStatePreAppInit)
		{
			pFramework->AddHook(this);
		}
		else
		{
			PreAppInit();
		}

		return true;
	}

private:

	// Helper: finds the folder this DLL is running from, so the log file
	// can be written next to it rather than in some unpredictable location.
	std::filesystem::path GetDllFolderPath()
	{
		wil::unique_cotaskmem_string modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());

		std::filesystem::path temp(modulePath.get());

		return temp.parent_path();
	}
};

// The single entry point the game looks for. It creates the director object
// and hands it to the game. Every SC4 DLL plugin must provide this function
// with exactly this name.
cRZCOMDllDirector* RZGetCOMDllDirector() {
	static cRegionUpdateDllDirector sDirector;
	return &sDirector;
}