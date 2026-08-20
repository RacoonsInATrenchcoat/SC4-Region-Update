# Here are all the Attributions and Thanks for the following mods:

### 1, "vendor/" by gzcom-dll (nsgomez) - https://github.com/nsgomez/gzcom-dll
  Used to enable SC4 DLL mods to work.

### 2, SC4 Auto-save by 0xC0000054 (Nicholas Hayes) - https://github.com/0xC0000054/sc4-auto-save
  Used as a baseline example to see how SC4 DLL mods work.

### 3, SC4MessageViewer by 0xC0000054 (Nicholas Hayes) - https://github.com/0xC0000054/SC4MessageViewer
  Used during development to capture the game's message sequence and identify
  the correct lifecycle and timing signals. Not distributed with this plugin.

### 4, SC4 Region Census by 0xC0000054 (Nicholas Hayes) - https://github.com/0xC0000054/sc4-region-census
  MIT License. Its source revealed the correct method for enumerating region
  tiles (cISC4Region::GetCity returns a double pointer; GetCityLocations /
  GetBoundingRect usage). This was critical to implementing tile enumeration.



And the usuals:
  Windows Implementation Library (Microsoft), MIT.
  Boost.PropertyTree, Boost Software License 1.0.
