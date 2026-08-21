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

	// --- Region enumeration + adjacency ---

	// A simple record of one tile, gathered during enumeration.
	struct TileInfo
	{
		int32_t x, z;           // origin (top-left) cell position
		int32_t sizeX, sizeZ;   // size in cells (small 1x1, medium 2x2, large 4x4)
		bool established;
	};

	// Returns true if two tiles share an edge (NOT merely a corner).
	// Each tile occupies cells [origin .. origin + size - 1] in both axes.
	// Corner-only contact is deliberately excluded (matches the research: two
	// tiles meeting at a single corner point are not neighbours).
	bool AreEdgeAdjacent(
		int32_t ax, int32_t az, int32_t asx, int32_t asz,   // tile A: pos + size
		int32_t bx, int32_t bz, int32_t bsx, int32_t bsz)   // tile B: pos + size
	{
		// Each tile's inclusive cell span.
		int32_t aLeft = ax, aRight = ax + asx - 1;
		int32_t aTop = az, aBottom = az + asz - 1;
		int32_t bLeft = bx, bRight = bx + bsx - 1;
		int32_t bTop = bz, bBottom = bz + bsz - 1;

		// Do the two tiles' vertical spans overlap by at least one cell?
		bool vertOverlap = (aTop <= bBottom) && (bTop <= aBottom);
		// Do their horizontal spans overlap by at least one cell?
		bool horizOverlap = (aLeft <= bRight) && (bLeft <= aRight);

		// Side-by-side horizontally (A's right meets B's left, or vice versa)
		// AND their vertical spans overlap => a genuine shared vertical edge.
		bool horizAdjacent =
			((aRight + 1 == bLeft) || (bRight + 1 == aLeft)) && vertOverlap;

		// Stacked vertically (A's bottom meets B's top, or vice versa)
		// AND their horizontal spans overlap => a genuine shared horizontal edge.
		bool vertAdjacent =
			((aBottom + 1 == bTop) || (bBottom + 1 == aTop)) && horizOverlap;

		return horizAdjacent || vertAdjacent;
	}

	// Reads every established tile in the current region, then computes and
	// logs which pairs are edge-adjacent. This is the Planner's core data:
	// the tile list and the adjacency graph.
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

		// Gather unique tiles. A multi-cell tile appears once per cell it
		// occupies, so we de-duplicate by the tile's reported origin position.
		std::vector<TileInfo> tiles;

		for (int32_t y = minY; y <= maxY; y++)
		{
			for (int32_t x = minX; x <= maxX; x++)
			{
				cISC4RegionalCity** ppCity = pRegion->GetCity(
					static_cast<uint32_t>(x), static_cast<uint32_t>(y));

				if (!ppCity || !*ppCity)
				{
					continue;   // Empty cell (no established city here).
				}

				cISC4RegionalCity* pCity = *ppCity;

				int32_t px = 0, pz = 0;
				pCity->GetPosition(px, pz);

				// Skip if we've already recorded this tile via another of its cells.
				bool seen = false;
				for (const auto& t : tiles)
				{
					if (t.x == px && t.z == pz) { seen = true; break; }
				}
				if (seen) { continue; }

				int32_t sx = 0, sz = 0;
				pCity->GetCitySize(sx, sz);
				bool est = pCity->GetEstablished();

				tiles.push_back({ px, pz, sx, sz, est });
			}
		}

		// Log the tile list.
		logger.WriteLineFormatted(LogLevel::Info, "  Found %zu unique tile(s):", tiles.size());
		for (const auto& t : tiles)
		{
			logger.WriteLineFormatted(LogLevel::Info,
				"    pos(%d,%d) size %dx%d established=%s",
				t.x, t.z, t.sizeX, t.sizeZ, t.established ? "yes" : "no");
		}

		// Compute and log edge-adjacency for every unique pair of tiles.
		logger.WriteLine(LogLevel::Info, "  Adjacent pairs (shared edge, corners excluded):");
		int pairCount = 0;
		for (size_t i = 0; i < tiles.size(); i++)
		{
			for (size_t j = i + 1; j < tiles.size(); j++)
			{
				if (AreEdgeAdjacent(
					tiles[i].x, tiles[i].z, tiles[i].sizeX, tiles[i].sizeZ,
					tiles[j].x, tiles[j].z, tiles[j].sizeX, tiles[j].sizeZ))
				{
					pairCount++;
					logger.WriteLineFormatted(LogLevel::Info,
						"    (%d,%d) <-> (%d,%d)",
						tiles[i].x, tiles[i].z, tiles[j].x, tiles[j].z);
				}
			}
		}

		logger.WriteLineFormatted(LogLevel::Info,
			"=== END: %zu tile(s), %d adjacent pair(s) ===",
			tiles.size(), pairCount);
	}

	// --- end region enumeration + adjacency ---

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

	// At startup, subscribe to cheat messages and register our cheat code.
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