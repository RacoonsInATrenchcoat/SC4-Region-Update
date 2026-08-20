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

#include "Logger.h"
#include "version.h"
#include "cIGZFrameWork.h"
#include "cIGZApp.h"
#include "cISC4App.h"                  // Top-level game app: gateway to the region.
#include "cISC4Region.h"               // The region: holds the list of city tiles.
#include "cISC4RegionalCity.h"         // One city tile (position, size, established...).
#include "cIGZMessageServer2.h"
#include "cIGZMessage2.h"
#include "cIGZMessage2Standard.h"
#include "cIGZString.h"                // For reading city names into a string.
#include "cRZBaseString.h"             // A concrete cIGZString we can write into.
#include "cRZMessage2COMDirector.h"
#include "GZServPtrs.h"                // Provides cISC4AppPtr (how we reach the app).
#include <filesystem>
#include <list>
#include <vector>
#include <Windows.h>
#include "wil/resource.h"
#include "wil/filesystem.h"
#include "cIGZCheatCodeManager.h"
#include "cIGZMessageTarget2.h"

// Fires when region view is entered. We enumerate the region's tiles here.
static constexpr uint32_t kSC4MessagePostRegionInit = 0xCBB5BB45;

// City fully loaded (kept for later; not used by this enumeration step).
static constexpr uint32_t kSC4MessagePostCityInitComplete = 0xEA8AE29A;

static constexpr uint32_t kRegionUpdatePluginDirectorID = 0xE04809A9;

static constexpr std::string_view PluginLogFileName = "SC4RegionUpdate.log";

// Using for testing:
// Cheat-code support. The game sends this message type when any cheat is typed.
static constexpr uint32_t kMessageCheatIssued = 0x230e27ac;
// Our unique cheat ID (reuse the director-ID style: random, unique).
static constexpr uint32_t kEnumTilesCheatID = 0xE0480A01;
// The text the user types in the cheat box.
static constexpr std::string_view EnumTilesCheatName = "enumtiles";

class cRegionUpdateDllDirector : public cRZMessage2COMDirector
{
public:

	cRegionUpdateDllDirector()
	{
		std::filesystem::path dllFolder = GetDllFolderPath();

		std::filesystem::path logFilePath = dllFolder;
		logFilePath /= PluginLogFileName;

		Logger& logger = Logger::GetInstance();

		logger.Init(logFilePath, LogLevel::Info);
		logger.WriteLogFileHeader("SC4RegionUpdate v" PLUGIN_VERSION_STR);
	}

	uint32_t GetDirectorID() const
	{
		return kRegionUpdatePluginDirectorID;
	}

	// Walks every tile in the current region and logs its details.
	// This is the Planner's core data-gathering, proven here by logging.
	void EnumerateRegionTiles()
	{
		Logger& logger = Logger::GetInstance();
		logger.WriteLine(LogLevel::Info, "=== EnumerateRegionTiles: START ===");

		cISC4AppPtr pApp;
		if (!pApp) { logger.WriteLine(LogLevel::Error, "  no app."); return; }

		cISC4Region* pRegion = pApp->GetRegion();
		if (!pRegion) { logger.WriteLine(LogLevel::Error, "  no region."); return; }

		// Discover the region's real bounds: [minX, minY, maxX, maxY].
		int32_t rect[8] = { 0 };
		pRegion->GetBoundingRect(reinterpret_cast<intptr_t>(rect));
		int32_t minX = rect[0], minY = rect[1], maxX = rect[2], maxY = rect[3];
		logger.WriteLineFormatted(LogLevel::Info,
			"  region bounds: x %d..%d, y %d..%d", minX, maxX, minY, maxY);

		// Scan every cell in bounds. A multi-cell tile appears once per cell,
		// so we de-duplicate by the tile's reported origin position.
		std::vector<std::pair<int32_t, int32_t>> seenPositions;
		int uniqueTiles = 0;
		int establishedTiles = 0;

		for (int32_t y = minY; y <= maxY; y++)
		{
			for (int32_t x = minX; x <= maxX; x++)
			{
				auto ppCity = pRegion->GetCity(
					static_cast<uint32_t>(x), static_cast<uint32_t>(y));

				if (!ppCity || !*ppCity)
				{
					continue;   // Empty cell (no tile here).
				}

				cISC4RegionalCity* pCity = *ppCity;

				int32_t px = 0, pz = 0;
				pCity->GetPosition(px, pz);

				// Skip if we've already recorded this tile (from another of its cells).
				bool alreadySeen = false;
				for (const auto& p : seenPositions)
				{
					if (p.first == px && p.second == pz)
					{
						alreadySeen = true;
						break;
					}
				}
				if (alreadySeen)
				{
					continue;
				}
				seenPositions.push_back({ px, pz });

				// First time seeing this tile: record its details.
				int32_t sx = 0, sz = 0;
				pCity->GetCitySize(sx, sz);
				bool est = pCity->GetEstablished();

				uniqueTiles++;
				if (est)
				{
					establishedTiles++;
				}

				logger.WriteLineFormatted(LogLevel::Info,
					"  Tile: pos(%d,%d) size %dx%d established=%s",
					px, pz, sx, sz, est ? "yes" : "no");
			}
		}

		logger.WriteLineFormatted(LogLevel::Info,
			"=== END: %d unique tiles (%d established) ===",
			uniqueTiles, establishedTiles);
	}

	// The switchboard: routes subscribed events to their handlers.
	bool DoMessage(cIGZMessage2* pMessage)
	{
		uint32_t dwType = pMessage->GetType();

		if (dwType == kMessageCheatIssued)
		{
			cIGZMessage2Standard* pStd = static_cast<cIGZMessage2Standard*>(pMessage);
			uint32_t cheatID = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pStd->GetVoid1()));

			Logger::GetInstance().WriteLineFormatted(LogLevel::Info, "Cheat issued, id=0x%08X", cheatID);

			if (cheatID == kEnumTilesCheatID)
			{
				EnumerateRegionTiles();
			}
		}

		return true;
	}

	// At startup, subscribe to the "region entered" event.
	bool PostAppInit()
	{
		Logger::GetInstance().WriteLine(LogLevel::Info, "PostAppInit: starting.");

		// Subscribe to game messages (we keep the cheat-issued message here).
		cIGZMessageServer2Ptr pMsgServ;
		if (pMsgServ)
		{
			pMsgServ->AddNotification(this, kMessageCheatIssued);
			Logger::GetInstance().WriteLine(LogLevel::Info, "PostAppInit: subscribed to cheat messages.");
		}

		// Register our cheat code with the game.
		cISC4AppPtr pApp;
		if (pApp)
		{
			cIGZCheatCodeManager* pCheatMgr = pApp->GetCheatCodeManager();
			if (pCheatMgr)
			{
				cRZBaseString cheatName(EnumTilesCheatName.data());
				pCheatMgr->AddNotification2(this, 0);
				bool reg = pCheatMgr->RegisterCheatCode(kEnumTilesCheatID, cheatName);
				Logger::GetInstance().WriteLineFormatted(LogLevel::Info, "PostAppInit: cheat registered = %s", reg ? "true" : "false");
			}
			else
			{
				Logger::GetInstance().WriteLine(LogLevel::Error, "PostAppInit: no cheat manager.");
			}
		}

		return true;
	}

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

	std::filesystem::path GetDllFolderPath()
	{
		wil::unique_cotaskmem_string modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());

		std::filesystem::path temp(modulePath.get());

		return temp.parent_path();
	}
};

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static cRegionUpdateDllDirector sDirector;
	return &sDirector;
}