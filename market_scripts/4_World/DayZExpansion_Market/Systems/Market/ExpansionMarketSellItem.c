/**
 * ExpansionMarketSell.c
 *
 * DayZ Expansion Mod
 * www.dayzexpansion.com
 * © 2022 DayZ Expansion Mod Team
 *
 * This work is licensed under the Creative Commons Attribution-NonCommercial-NoDerivatives 4.0 International License. 
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-nd/4.0/.
 *
*/

class ExpansionMarketSell
{
	bool Valid;

	int Price;
	ExpansionTraderObjectBase Trader;
	int Time;

	//! Total amount to sell (without modifiers and attachments)
	int TotalAmount;

	ref ExpansionMarketItem Item;

	ref array<ref ExpansionMarketSellItem> Sell;
	
	// ------------------------------------------------------------
	// ExpansionMarketSell Constructor
	// ------------------------------------------------------------
	void ExpansionMarketSell()
	{
		Sell = new array<ref ExpansionMarketSellItem>;
	}
	
	// ------------------------------------------------------------
	// ExpansionMarketSell Debug
	// ------------------------------------------------------------
	void Debug()
	{
		Print("ExpansionMarketSell::Debug - Valid: " + Valid);
		Print("ExpansionMarketSell::Debug - Price: " + Price);
		Print("ExpansionMarketSell::Debug - Trader: " + Trader);
		Print("ExpansionMarketSell::Debug - Time: " + Time);
		Print("ExpansionMarketSell::Debug - TotalAmount: " + TotalAmount);
		Print("ExpansionMarketSell::Debug - Item: " + Item);
		Print("ExpansionMarketSell::Debug - ClassName: " + Item.ClassName);
		Print("ExpansionMarketSell::Debug - Sell[N=" + Sell.Count() + "]:");
		
		for (int i = 0; i < Sell.Count(); i++)
		{
			Sell[i].Debug(i);
		}
	}
	
	ExpansionMarketSellItem AddSellItem(int takenAmount, float addStockAmount, EntityAI item, string className = "")
	{
		EXError.ErrorOnce(this, "DEPRECATED, use AddSellItem(takenAmount, soldAmount, ...)");
		int soldAmount = addStockAmount;
		return AddSellItem(takenAmount, soldAmount, 1.0, item, className);
	}

	ExpansionMarketSellItem AddSellItem(int takenAmount, int soldAmount, float incrementStockModifier, EntityAI item, string className = "")
	{
#ifdef EXPANSIONTRACE
		auto trace = CF_Trace_0(ExpansionTracing.MARKET, this, "AddItem");
#endif

		if (!className && item)
		{
			className = item.GetType();
			className.ToLower();
		}
		
		ExpansionMarketSellItem itemSell = new ExpansionMarketSellItem;
		itemSell.TakenAmount = takenAmount;
		itemSell.SoldAmount = soldAmount;
		itemSell.ItemRep = item;  //! Can be NULL! (ammo in mags/ammopiles are not entities)
		itemSell.ClassName = className;
		if (item)
			itemSell.IsEntity = true;
		Sell.Insert(itemSell);

		itemSell.AddStockAmount = soldAmount * incrementStockModifier;
		return itemSell;
	}
}

class ExpansionMarketSellItem
{
	int TakenAmount;
	int SoldAmount;
	EntityAI ItemRep;
	string ClassName;
	bool IsEntity;

	//! Amount to be added to trader stock when selling (including modifiers)
	float AddStockAmount;

	//! For debug purposes
	int Price;
	ref array<ref ExpansionMarketItemPrice> PriceTiers = {};
		
	// ------------------------------------------------------------
	// ExpansionMarketSellItem Debug
	// ------------------------------------------------------------
	void Debug(int i)
	{
		Print("ExpansionMarketSellItem::Debug - ExpansionMarketSellItem[" + i + "]: ");
		Print("ExpansionMarketSellItem::Debug - ClassName: " + ClassName);
		Print("ExpansionMarketSellItem::Debug - TakenAmount: " + TakenAmount);
		Print("ExpansionMarketSellItem::Debug - AddStockAmount: " + AddStockAmount);
	}
	
	// ------------------------------------------------------------
	// ExpansionMarketSellItem DestroyItem
	// ------------------------------------------------------------
	void DestroyItem()
	{
		if (!ItemRep)
			return;

		//! @note if TakenAmount is zero item is destroyed without checking amount
		int remainAmount;
		if (TakenAmount > 0)
		{
			int amount = ExpansionMarketModule.GetInstance().GetItemAmount(ItemRep);
			remainAmount = amount - TakenAmount;
		}

		if (remainAmount <= 0)
		{
			#ifdef EXPANSIONMODVEHICLE
			auto vehicle = ExpansionVehicle.Get(ItemRep);
			if (vehicle)
			{
				//! Delete all keys for vehicle
				array< ExpansionCarKey > keys;
				ExpansionCarKey.GetKeysForVehicle(vehicle, keys);
				foreach (ExpansionCarKey key : keys)
				{
					GetGame().ObjectDelete(key);
				}
			}
			#endif

			GetGame().ObjectDelete(ItemRep);
			return;
		}

		ItemBase itm;
		Magazine mag;
		Ammunition_Base ammo;
		if (Class.CastTo(mag, ItemRep))
		{
			mag.ServerSetAmmoCount(remainAmount);
		}
		else if (Class.CastTo(ammo, ItemRep))
		{
			ammo.ServerSetAmmoCount(remainAmount);
		}
		else if ( Class.CastTo(itm, ItemRep)) 
		{
			itm.SetQuantity(remainAmount);
		}
	}
}

class ExpansionMarketSellDebug
{
	float m_ZoneSellPricePercent;
	autoptr array<ref ExpansionMarketSellDebugItem> m_Items;

	void ExpansionMarketSellDebug(ExpansionMarketSell marketSell = null, ExpansionMarketTraderZone zone = null)
	{
		m_Items = new array<ref ExpansionMarketSellDebugItem>;

		if (!marketSell || !zone)
			return;

		m_ZoneSellPricePercent = zone.SellPricePercent;

		foreach (ExpansionMarketSellItem sellItem: marketSell.Sell)
		{
			auto debugItem = new ExpansionMarketSellDebugItem(sellItem, zone);
			m_Items.Insert(debugItem);
		}
	}

	TStringArray GetItemClassNames()
	{
		TStringArray classNames();

		foreach (ExpansionMarketSellDebugItem debugItem: m_Items)
		{
			classNames.Insert(debugItem.ClassName);
		}

		return classNames;
	}

	void OnSend(ParamsWriteContext ctx)
	{
		ctx.Write(m_ZoneSellPricePercent);

		ctx.Write(m_Items.Count());

		int i;
		foreach (ExpansionMarketSellDebugItem debugItem: m_Items)
		{
			debugItem.OnSend(ctx, i == 0);
			i++;
		}
	}

	void OnReceive(ParamsReadContext ctx, int mainItemID)
	{
		ctx.Read(m_ZoneSellPricePercent);

		int count;
		ctx.Read(count);

		for (int i = 0; i < count; i++)
		{
			auto debugItem = new ExpansionMarketSellDebugItem;
			debugItem.OnReceive(ctx, i == 0);
			if (i == 0)
				debugItem.ItemID = mainItemID;
			ExpansionMarketItem item = ExpansionMarketCategory.GetGlobalItem(debugItem.ItemID, false);
			if (item)
				debugItem.ClassName = item.ClassName;
			m_Items.Insert(debugItem);
		}
	}

	void Dump()
	{
		EXLogPrint("| Zone SellPricePercent: " + m_ZoneSellPricePercent);
		EXLogPrint("| Items: " + m_Items.Count());
		foreach (int i, ExpansionMarketSellDebugItem debugItem: m_Items)
		{
			EXLogPrint("| Item " + (i + 1).ToString());
			EXLogPrint("|   ItemID: " + debugItem.ItemID);
			EXLogPrint("|   ClassName: " + debugItem.ClassName);
			EXLogPrint("|   MaxPriceThreshold: " + debugItem.MaxPriceThreshold);
			EXLogPrint("|   MinPriceThreshold: " + debugItem.MinPriceThreshold);
			EXLogPrint("|   SellPricePercent: " + debugItem.SellPricePercent);
			EXLogPrint("|   MaxStockThreshold: " + debugItem.MaxStockThreshold);
			EXLogPrint("|   MinStockThreshold: " + debugItem.MinStockThreshold);
			EXLogPrint("|   Stock: " + debugItem.Stock);
			EXLogPrint("|   SoldAmount: " + debugItem.SoldAmount);
			EXLogPrint("|   AddStockAmount: " + debugItem.AddStockAmount);
			EXLogPrint("|   Price: " + debugItem.Price);
			EXLogPrint("|   PriceTiers");
			foreach (auto itemPrice: debugItem.PriceTiers)
			{
				EXLogPrint("|     Price at stock #: " + itemPrice.param1 + " at " + itemPrice.param2);
			}
		}
	}
}

class ExpansionMarketSellDebugItem
{
	int ItemID;
	string ClassName;

	int MaxPriceThreshold;
	int MinPriceThreshold;

	float SellPricePercent;

	int MaxStockThreshold;
	int MinStockThreshold;

	int Stock;

	int SoldAmount;
	float AddStockAmount;
	int Price;
	ref array<ref ExpansionMarketItemPrice> PriceTiers;

	void ExpansionMarketSellDebugItem(ExpansionMarketSellItem sellItem = null, ExpansionMarketTraderZone zone = null)
	{
		if (!sellItem || !zone)
			return;

		ExpansionMarketItem item = ExpansionMarketCategory.GetGlobalItem(sellItem.ClassName);
		if (!item)
			return;

		ItemID = item.ItemID;
		ClassName = item.ClassName;
		MaxPriceThreshold = item.MaxPriceThreshold;
		MinPriceThreshold = item.MinPriceThreshold;
		SellPricePercent = item.SellPricePercent;
		MaxStockThreshold = item.MaxStockThreshold;
		MinStockThreshold = item.MinStockThreshold;
		if (item.IsStaticStock())
			Stock = ExpansionMarketStock.Static;
		else
			Stock = zone.GetStock(item.ClassName);
		SoldAmount = sellItem.SoldAmount;
		AddStockAmount = sellItem.AddStockAmount;
		Price = sellItem.Price;
		PriceTiers = sellItem.PriceTiers;
	}

	void OnSend(ParamsWriteContext ctx, bool isMainItem = false)
	{
		if (!isMainItem)
			ctx.Write(ItemID);

		ctx.Write(MaxPriceThreshold);
		ctx.Write(MinPriceThreshold);

		ctx.Write(SellPricePercent);

		ctx.Write(MaxStockThreshold);
		ctx.Write(MinStockThreshold);

		ctx.Write(Stock);

		ctx.Write(SoldAmount);
		ctx.Write(AddStockAmount);
		ctx.Write(Price);

		ctx.Write(PriceTiers.Count());
		foreach (auto itemPrice: PriceTiers)
		{
			ctx.Write(itemPrice.param1);  //! price
			ctx.Write(itemPrice.param2);  //! stock
		}
	}

	void OnReceive(ParamsReadContext ctx, bool isMainItem = false)
	{
		if (!isMainItem)
			ctx.Read(ItemID);

		ctx.Read(MaxPriceThreshold);
		ctx.Read(MinPriceThreshold);

		ctx.Read(SellPricePercent);

		ctx.Read(MaxStockThreshold);
		ctx.Read(MinStockThreshold);

		ctx.Read(Stock);

		ctx.Read(SoldAmount);
		ctx.Read(AddStockAmount);
		ctx.Read(Price);

		int count;
		ctx.Read(count);
		PriceTiers = {};
		while (count--)
		{
			int price;
			ctx.Read(price);
			float stock;
			ctx.Read(stock);
			PriceTiers.Insert(new ExpansionMarketItemPrice(price, stock));
		}
	}
}

typedef Param2<int, float> ExpansionMarketItemPrice;
