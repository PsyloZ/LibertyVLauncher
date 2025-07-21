/**
 * ExpansionMarketTraderZone.c
 *
 * DayZ Expansion Mod
 * www.dayzexpansion.com
 * © 2022 DayZ Expansion Mod Team
 *
 * This work is licensed under the Creative Commons Attribution-NonCommercial-NoDerivatives 4.0 International License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-nd/4.0/.
 *
*/

enum ExpansionMarketStock
{
	Undefined = -3,  //! -3 for legacy 3rd party compat
	Static = -2
}

class ExpansionMarketTraderZoneBase
{
	int m_Version;
}

class ExpansionMarketTraderZoneV4: ExpansionMarketTraderZoneBase
{
	float PricePercent;
}

class ExpansionMarketTraderZone: ExpansionMarketTraderZoneBase
{
	static int VERSION = 6;

	[NonSerialized()]
	string m_FileName;

	string m_DisplayName;

	vector Position;
	float Radius;

	float BuyPricePercent;
	float SellPricePercent;

	ref map<string, int> Stock;
	[NonSerialized()]
	ref ExpansionMarketTraderZoneReserved ReservedZone;
	
	// ------------------------------------------------------------
	// ExpansionMarketTraderZone Constructor
	// ------------------------------------------------------------
	void ExpansionMarketTraderZone()
	{
		Stock = new map<string, int>;
		ReservedZone = new ExpansionMarketTraderZoneReserved;
	}

	void DebugPrint()
	{
		//! Print( "DebugPrint Count: " + Stock.Count() );
		foreach (string clsName, int stock: Stock)
		{
			Print( "Item " + clsName + " | Stock " + stock + " | Reserved " + ReservedZone.ReservedStock.Get( clsName ) );
		}
	}

	// ------------------------------------------------------------
	// ExpansionMarketTraderZone Load
	// ------------------------------------------------------------
	static ExpansionMarketTraderZone Load(string name)
	{
		ExpansionMarketTraderZone settingsDefault = new ExpansionMarketTraderZone;
		settingsDefault.Defaults();

		ExpansionMarketTraderZone settings = new ExpansionMarketTraderZone;
		
		if (!ExpansionJsonFileParser<ExpansionMarketTraderZone>.Load( EXPANSION_TRADER_ZONES_FOLDER + name + ".json", settings ))
			return NULL;

		settings.m_FileName = name;
		
		//! Make sure item classnames are lowercase
		map<string, int> items = settings.Stock;
		settings.Stock = new map<string, int>;
		foreach (string className, int stock : items)
		{
			className.ToLower();
			settings.Stock.Insert(className, stock);
		}

		//! Automatically convert outdated trader zone files to current version
		if (settings.m_Version < VERSION)
		{
			if (settings.m_Version < 4)
			{
				settings.BuyPricePercent = settingsDefault.BuyPricePercent;
			}
			else if (settings.m_Version < 5)
			{
				ExpansionMarketTraderZoneV4 settingsV4;
				if (!ExpansionJsonFileParser<ExpansionMarketTraderZoneV4>.Load( EXPANSION_TRADER_ZONES_FOLDER + name + ".json", settingsV4 ))
					return NULL;

				settings.BuyPricePercent = settingsV4.PricePercent;
			}

			settings.SellPricePercent = settingsDefault.SellPricePercent;

			settings.m_Version = VERSION;

			settings.Save();
		}

		return settings;
	}
	
	// ------------------------------------------------------------
	// ExpansionMarketTraderZone Save
	// ------------------------------------------------------------	
	void Save()
	{
		JsonFileLoader<ExpansionMarketTraderZone>.JsonSaveFile( EXPANSION_TRADER_ZONES_FOLDER + m_FileName + ".json", this );
	}

	// ------------------------------------------------------------
	// ExpansionMarketTraderZone Defaults
	// ------------------------------------------------------------
	void Defaults()
	{
		m_Version = VERSION;
		m_DisplayName = "World Trader Zone";
		m_FileName = "World";
		Position = "7500 0 7500"; 	
		Radius = 15000;
		BuyPricePercent = 100;
		SellPricePercent = -1;  //! -1 = Use global sell price percentage
	}
	
	// ------------------------------------------------------------
	// ExpansionMarketTraderZone GetNetworkSerialization
	// ------------------------------------------------------------
	int GetNetworkSerialization( ExpansionMarketTrader trader, out array< ref ExpansionMarketNetworkItem > list, int start, bool stockOnly, TIntArray itemIDs = NULL )
	{
		#ifdef EXPANSIONMODMARKET_DEBUG
		EXPrint("ExpansionMarketTraderZone::GetNetworkSerialization - Start - " + trader.m_FileName + " items count " + trader.m_Items.Count());
		#endif
		
		if ( !list )
			list = new array< ref ExpansionMarketNetworkItem >;

		int batchSize;
		int count;
		array<ref ExpansionMarketTraderItem> items;

		if (itemIDs && itemIDs.Count())
		{
			items = new array<ref ExpansionMarketTraderItem>;
			foreach (ExpansionMarketTraderItem tItem: trader.m_Items)
			{
				if (itemIDs.Find(tItem.MarketItem.ItemID) > -1)
					items.Insert(tItem);
			}
		}
		else
		{
			items = trader.m_Items;

			//! Send at most n items per batch
			batchSize = GetExpansionSettings().GetMarket().NetworkBatchSize;
		}

		count = items.Count();

		if (!batchSize)
		{
			//! Force batch size to item count - implicit if itemIDs given or incorrect zero value configured
			batchSize = count;
		}
		
		#ifdef EXPANSIONEXPRINT
		EXPrint("GetNetworkSerialization - start: " + start + " batch size: " + batchSize);
		#endif

		for (int i = start; i < count; i++)
		{
			if (i == start + batchSize)
				break;

			ExpansionMarketNetworkItem item = GetNetworkItemSerialization( items[i], stockOnly );
			//EXPrint("GetNetworkSerialization - " + items[i].MarketItem.ClassName + " (ID " + items[i].MarketItem.ItemID + ") - stock: " + item.Stock);
			list.Insert( item );
		}
		
		#ifdef EXPANSIONMODMARKET_DEBUG
		EXPrint("ExpansionMarketTraderZone::GetNetworkSerialization - End");
		#endif

		return i;
	}

	ExpansionMarketNetworkItem GetNetworkItemSerialization( ExpansionMarketTraderItem tItem, bool stockOnly )
	{
		#ifdef EXPANSIONMODMARKET_DEBUG
		EXPrint("ExpansionMarketTraderZone::GetNetworkItemSerialization - Start " + tItem.MarketItem.ClassName);
		#endif

		int stock;

		if ( tItem.MarketItem.IsStaticStock() )
		{
			stock = 1;
			//EXPrint("GetNetworkItemSerialization - " + tItem.MarketItem.ClassName + " (ID " + tItem.MarketItem.ItemID + ") - static stock: " + stock);
		} 
		else if ( Stock.Contains( tItem.MarketItem.ClassName ) )
		{
			int reservedStock = ReservedZone.ReservedStock.Get( tItem.MarketItem.ClassName );
			int zoneStock = Stock.Get( tItem.MarketItem.ClassName );
			
			//Print("GetNetworkSerialization:: - name:" + tItem.MarketItem.ClassName);
			//Print("GetNetworkSerialization:: - reservedStock:" + reservedStock);
			//Print("GetNetworkSerialization:: - zoneStock:" + zoneStock);
			//Print("GetNetworkSerialization:: - calculated:" + (zoneStock - reservedStock));

			//! Here we remove the current reserved stock amount from the current stock so we don't send over an incorrect stock value
			stock = zoneStock - reservedStock;
			//EXPrint("GetNetworkItemSerialization - " + tItem.MarketItem.ClassName + " (ID " + tItem.MarketItem.ItemID + ") - stock: " + stock);
		}
		else
		{
			//! For items that are not in this trader zone's inventory (i.e. attachments on other items), set min stock so price calc can work correctly
			stock = tItem.MarketItem.MinStockThreshold;
			//EXPrint("GetNetworkItemSerialization - " + tItem.MarketItem.ClassName + " (ID " + tItem.MarketItem.ItemID + ") - min stock: " + stock);
		}

		ExpansionMarketNetworkItem item = new ExpansionMarketNetworkItem( tItem.MarketItem.ItemID, stock );

		item.m_StockOnly = tItem.MarketItem.m_StockOnly;

		if (stockOnly || item.m_StockOnly)
			return item;

		item.CategoryID = tItem.MarketItem.CategoryID;
		item.ClassName = tItem.MarketItem.ClassName;
		item.MinPriceThreshold = tItem.MarketItem.MinPriceThreshold;
		item.MaxPriceThreshold = tItem.MarketItem.MaxPriceThreshold;
		item.MinStockThreshold = tItem.MarketItem.MinStockThreshold;
		item.MaxStockThreshold = tItem.MarketItem.MaxStockThreshold;
		item.AttachmentIDs = new array< int >;
		foreach (string className: tItem.MarketItem.SpawnAttachments)
		{
			ExpansionMarketItem attachment = ExpansionMarketCategory.GetGlobalItem(className);
			if (attachment)
				item.AttachmentIDs.Insert(attachment.ItemID);
			else
				EXPrint("ExpansionMarketTraderZone::GetNetworkItemSerialization - WARNING: Attachment '" + className + "' does not exist!");
		}
		item.Variants = new array< string >;
		item.Variants.Copy(tItem.MarketItem.Variants);

		//! Network optimization: Pack BuySell, Hardline item rarity (if loaded), QuantityPercent and SellPricePercent into one 32-bit int
		//! (8 bits for BuySell and item rarity combined, 8 bits for QuantityPercent, 16 bits for SellPricePercent)
		//! @note we need to include Hardline item rarity here because it is used in market menu, and item previews are only rendered clientside so we can't use
		//!       ItemBase netsync
		//! @note for QuantityPercent, we use 0x0..0x7f for 0..127 and 0x80..0xff for -128..-1, this needs to be dealt with when decoding!
		//! @note for SellPricePercent, we use 0x0..0x00007fff for 0..32767 and 0x00008000..0x0000ffff for -32768..-1, this needs to be dealt with when decoding!
		int param1 = tItem.BuySell;
#ifdef EXPANSIONMODHARDLINE
		param1 |= tItem.MarketItem.m_Rarity << 4;
#endif
		item.Packed = ((param1 & 0xff) << 24) | ((tItem.MarketItem.QuantityPercent & 0xff) << 16) | (tItem.MarketItem.m_SellPricePercent & 0x0000ffff);

		#ifdef EXPANSIONMODMARKET_DEBUG
		EXPrint("ExpansionMarketTraderZone::GetNetworkItemSerialization - End " + tItem.MarketItem.ClassName);
		#endif

		return item;
	}

	// ------------------------------------------------------------
	// ExpansionMarketTraderZone SetStock
	// ------------------------------------------------------------
	void SetStock( string className, int stock )
	{
		SetStock_Internal(className, stock, false);
	}

	// ------------------------------------------------------------
	// ExpansionMarketTraderZone SetStock_Internal
	// ------------------------------------------------------------
	protected void SetStock_Internal( string className, int stock, bool addToExisting = false )
	{
		#ifdef EXPANSIONMODMARKET_DEBUG
		EXPrint("ExpansionMarketTraderZone::SetStock_Internal - Start - " + className + " " + stock + " add " + addToExisting);
		#endif
		
		className.ToLower();

		ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem( className, false );
		if ( !marketItem )
		{
			#ifdef EXPANSIONMODMARKET_DEBUG
			EXPrint("ExpansionMarketTraderZone::SetStock_Internal - End");
			#endif
			return;
		}

		bool staticStock = marketItem.IsStaticStock();

		if ( staticStock )
			stock = 1;
		
		if ( Stock.Contains( className ) )
		{
			if ( !staticStock && addToExisting )
				stock += Stock.Get( className );

			if ( stock > marketItem.MaxStockThreshold )
				stock = marketItem.MaxStockThreshold;

			Stock.Set( className, stock );
		} 
		else 
		{
			if ( stock > marketItem.MaxStockThreshold )
				stock = marketItem.MaxStockThreshold;

			Stock.Insert( className, stock );			
			ReservedZone.ReservedStock.Insert( className, 0 );
		}
		
		#ifdef EXPANSIONMODMARKET_DEBUG
		EXPrint("ExpansionMarketTraderZone::SetStock_Internal - End");
		#endif
	}

	// ------------------------------------------------------------
	// Expansion ClearReservedStock
	// ------------------------------------------------------------
	void ClearReservedStock( string className, int reserved )
	{
		#ifdef EXPANSIONMODMARKET_DEBUG
		EXPrint("ExpansionMarketTraderZone::ClearReservedStock - Start");
		#endif

		className.ToLower();

		ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem( className );
		if ( !marketItem )
			return;

		if ( !marketItem.IsStaticStock() )
		{
			#ifdef EXPANSIONMODMARKET_DEBUG
			EXPrint("ExpansionMarketTraderZone::ClearReservedStock - Clear reserved stock: Name: " + className + " || Reserved now: " + ReservedZone.ReservedStock.Get( className ) + " || To Remove: " + reserved);
			#endif
			
			int new_stock = ReservedZone.ReservedStock.Get( className ) - reserved;
			ReservedZone.ReservedStock.Set( className, new_stock );
			
			#ifdef EXPANSIONMODMARKET_DEBUG
			EXPrint("ExpansionMarketTraderZone::ClearReservedStock - Cleared reserved stock: Name: " + className + " || Reserved after: " + new_stock);
			#endif
		}

		#ifdef EXPANSIONMODMARKET_DEBUG
		EXPrint("ExpansionMarketTraderZone::ClearReservedStock - End");
		#endif
	}

	// ------------------------------------------------------------
	// Expansion AddStock
	// Adds the stock level for the item
	// ------------------------------------------------------------
	void AddStock( string className, int stock )
	{
		SetStock_Internal(className, stock, true);
	}

	// ------------------------------------------------------------
	// Expansion RemoveStock
	// Remove the stock level for the item
	// ------------------------------------------------------------
	void RemoveStock( string className, int stock, bool inReserve = false )
	{
#ifdef EXPANSIONTRACE
		auto trace = CF_Trace_0(ExpansionTracing.MARKET, this, "RemoveStock");
#endif
	
		className.ToLower();

		ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem( className );
		if ( !marketItem )
			return;
		
		if ( Stock.Contains( className ) )
		{
			if ( !marketItem.IsStaticStock() )
			{
				int new_stock;

				if ( inReserve )
				{
					new_stock = ReservedZone.ReservedStock.Get( className ) + stock;

					ReservedZone.ReservedStock.Set( className, new_stock );
				} 
				else
				{
					new_stock = Stock.Get( className ) - stock;
					
					if ( new_stock < 0 )
						new_stock = 0;

					Stock.Set( className, new_stock );
				}
			}
		} 
		else 
		{
			Stock.Insert( className, 0 );
			ReservedZone.ReservedStock.Insert( className, 0 );
		}
	}

	// ------------------------------------------------------------
	// Expansion ItemExists
	// ------------------------------------------------------------
	bool ItemExists(string className)
	{
		className.ToLower();
		
		return Stock.Contains(className);
	}

	// ------------------------------------------------------------
	// Expansion GetStock
	// Gets the stock of an item within a trading zone
	// ------------------------------------------------------------
	int GetStock( string className, bool actual = false )
	{
#ifdef EXPANSIONTRACE
		auto trace = CF_Trace_0(ExpansionTracing.MARKET, this, "GetStock");
#endif

		className.ToLower();

		if (!ItemExists(className))
		{
			Error("ExpansionMarketTraderZone::GetStock - Item " + className + " does not exist in trader zone!");
			return ExpansionMarketStock.Undefined;
		}

		int stock = Stock.Get( className );

		if ( !actual )
		{
			int reservedStock;
			
			if ( ReservedZone.ReservedStock.Find( className, reservedStock ) )
			{
				stock = stock - reservedStock;
			}
			else
			{
				ReservedZone.ReservedStock.Insert( className, 0 );
			}

			if ( stock < 0 )
			{
				Error( "WARNING: ReservedStock for " + className + " is " + reservedStock + " which brings the total stock to " + stock );
			}
		}

		return stock;
	}

	//! Updates stock - optionally removes items that do no longer exist and/or adds items from categories
	void Update(bool removeNonExistent = false, map<int, ref ExpansionMarketCategory> categories = NULL)
	{
		int removed;

		if (removeNonExistent)
		{
			TStringArray toRemove = new TStringArray;
			foreach (string className, int stock : Stock)
			{
				ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem( className );
				if ( !marketItem )
				{
					toRemove.Insert(className);
				}
			}
			foreach (string className_toRemove : toRemove)
			{
				EXPrint("ExpansionMarketTraderZone::Update - " + m_FileName + " - removing " + className_toRemove);
				Stock.Remove(className_toRemove);
			}
			removed = toRemove.Count();
		}

		int added;

		if (categories)
		{
			foreach (ExpansionMarketCategory cat : categories)
			{
				foreach (ExpansionMarketItem item: cat.Items)
				{
					if (!Stock.Contains(item.ClassName))
					{
						EXPrint("ExpansionMarketTraderZone::Update - " + m_FileName + " - adding " + item.ClassName);
						int newStock;
						if (item.IsStaticStock())
							newStock = 1;
						else
							newStock = item.MaxStockThreshold;
						Stock.Insert(item.ClassName, newStock);
						added++;
					}
				}
			}
		}

		if (removed || added)
			Save();
	}
}