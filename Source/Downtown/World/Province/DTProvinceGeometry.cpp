#include "World/Province/DTProvinceGeometry.h"
#include "Engine/DataTable.h"

namespace DTProvinceZones
{
	struct FPreset
	{
		const TCHAR* Name;
		const TCHAR* Codes;
	};

	// Keep in sync with Tools/GeoPipeline/prepare_geodata.py (ZONES).
	static const FPreset Presets[] = {
		{ TEXT("bangkok-metro"), TEXT("TH-10,TH-11,TH-12,TH-13,TH-73,TH-74") },
		{ TEXT("test-area"), TEXT("TH-10,TH-12,TH-13") },
	};

	static bool RegionFromName(const FString& Name, EDTThaiRegion& OutRegion)
	{
		static const TPair<const TCHAR*, EDTThaiRegion> Regions[] = {
			{ TEXT("north"), EDTThaiRegion::North }, { TEXT("northeast"), EDTThaiRegion::Northeast },
			{ TEXT("isan"), EDTThaiRegion::Northeast }, { TEXT("central"), EDTThaiRegion::Central },
			{ TEXT("east"), EDTThaiRegion::East }, { TEXT("west"), EDTThaiRegion::West },
			{ TEXT("south"), EDTThaiRegion::South },
		};
		for (const TPair<const TCHAR*, EDTThaiRegion>& R : Regions)
		{
			if (Name.Equals(R.Key, ESearchCase::IgnoreCase))
			{
				OutRegion = R.Value;
				return true;
			}
		}
		return false;
	}
}

int32 FDTProvinceGeometry::Build(const UDataTable* Table, TArray<FString>& OutWarnings)
{
	Entries.Reset();
	if (!Table)
	{
		OutWarnings.Add(TEXT("Province table is null."));
		return 0;
	}
	if (Table->GetRowStruct() != FDTProvinceRow::StaticStruct())
	{
		OutWarnings.Add(FString::Printf(TEXT("Table %s uses row struct %s, expected DTProvinceRow."),
			*Table->GetPathName(), Table->GetRowStruct() ? *Table->GetRowStruct()->GetName() : TEXT("<none>")));
		return 0;
	}

	Table->ForeachRow<FDTProvinceRow>(TEXT("FDTProvinceGeometry::Build"), [this, &OutWarnings](const FName& RowName, const FDTProvinceRow& Row)
	{
		FEntry Entry;
		Entry.Row = Row;
		if (Entry.Row.Code.IsNone())
		{
			Entry.Row.Code = RowName;
		}
		const FString CodeStr = Entry.Row.Code.ToString();

		int32 BadPoints = 0;
		TArray<FDTProvinceRing> ValidRings;
		for (const FDTProvinceRing& Ring : Row.Rings)
		{
			FDTProvinceRing Clean;
			Clean.Points.Reserve(Ring.Points.Num());
			for (const FVector2D& P : Ring.Points)
			{
				if (FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::Abs(P.X) < MaxAbsCoordinateCm && FMath::Abs(P.Y) < MaxAbsCoordinateCm)
				{
					Clean.Points.Add(P);
				}
				else
				{
					++BadPoints;
				}
			}
			if (Clean.Points.Num() >= 3)
			{
				ValidRings.Add(MoveTemp(Clean));
			}
		}

		if (BadPoints > 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: skipped %d invalid/out-of-range outline point(s)."), *CodeStr, BadPoints));
		}
		if (ValidRings.Num() == 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: no usable outline (need >= 3 valid points) - province skipped."), *CodeStr));
			return;
		}
		if (ValidRings.Num() < Row.Rings.Num())
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: dropped %d degenerate ring(s)."), *CodeStr, Row.Rings.Num() - ValidRings.Num()));
		}

		Entry.Row.Rings = MoveTemp(ValidRings);
		for (const FDTProvinceRing& Ring : Entry.Row.Rings)
		{
			const FBox2D RingBox(Ring.Points);
			Entry.RingBounds.Add(RingBox);
			Entry.Bounds += RingBox;
		}

		const FVector2D Ref = Entry.Row.ReferencePoint;
		if (!FMath::IsFinite(Ref.X) || !FMath::IsFinite(Ref.Y) || !Entry.Bounds.ExpandBy(1000.0).IsInside(Ref))
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: ReferencePoint (%.0f, %.0f) is outside the outline - using the bounds centre."), *CodeStr, Ref.X, Ref.Y));
			Entry.Row.ReferencePoint = Entry.Bounds.GetCenter();
		}

		if (Entries.Contains(Entry.Row.Code))
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: duplicate row %s ignored."), *CodeStr, *RowName.ToString()));
			return;
		}
		Entries.Add(Entry.Row.Code, MoveTemp(Entry));
	});

	return Entries.Num();
}

bool FDTProvinceGeometry::PointInRing(const TArray<FVector2D>& Ring, const FVector2D& Point)
{
	// Even-odd ray casting.
	bool bInside = false;
	const int32 Num = Ring.Num();
	for (int32 i = 0, j = Num - 1; i < Num; j = i++)
	{
		const FVector2D& A = Ring[i];
		const FVector2D& B = Ring[j];
		if (((A.Y > Point.Y) != (B.Y > Point.Y)) &&
			(Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X))
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

bool FDTProvinceGeometry::Contains(FName Code, const FVector2D& Point) const
{
	const FEntry* Entry = Entries.Find(Code);
	if (!Entry || !Entry->Bounds.IsInside(Point))
	{
		return false;
	}
	for (int32 i = 0; i < Entry->Row.Rings.Num(); ++i)
	{
		if (Entry->RingBounds[i].IsInside(Point) && PointInRing(Entry->Row.Rings[i].Points, Point))
		{
			return true;
		}
	}
	return false;
}

FName FDTProvinceGeometry::FindAt(const FVector2D& Point, FName Hint, const TSet<FName>* Allowed) const
{
	auto IsAllowed = [Allowed](FName Code) { return !Allowed || Allowed->Contains(Code); };

	if (const FEntry* HintEntry = Entries.Find(Hint))
	{
		if (IsAllowed(Hint) && Contains(Hint, Point))
		{
			return Hint;
		}
		for (const FName& Neighbor : HintEntry->Row.Neighbors)
		{
			if (IsAllowed(Neighbor) && Contains(Neighbor, Point))
			{
				return Neighbor;
			}
		}
	}
	for (const TPair<FName, FEntry>& Pair : Entries)
	{
		if (IsAllowed(Pair.Key) && Contains(Pair.Key, Point))
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

FBox2D FDTProvinceGeometry::GetBounds(const TArray<FName>& Codes) const
{
	FBox2D Box(ForceInit);
	for (const FName& Code : Codes)
	{
		if (const FEntry* Entry = Entries.Find(Code))
		{
			Box += Entry->Bounds;
		}
	}
	return Box;
}

FString FDTProvinceGeometry::NormalizeName(const FString& In)
{
	FString Out;
	Out.Reserve(In.Len());
	for (const TCHAR C : In.ToLower())
	{
		if (C != TEXT(' ') && C != TEXT('-') && C != TEXT('.') && C != TEXT('_') && C != TEXT('\''))
		{
			Out.AppendChar(C);
		}
	}
	return Out;
}

FName FDTProvinceGeometry::NormalizeCode(const FString& Token)
{
	// Accepts TH-12, th12, 12
	FString Digits = Token.ToUpper().Replace(TEXT(" "), TEXT(""));
	Digits.RemoveFromStart(TEXT("TH"));
	Digits.RemoveFromStart(TEXT("-"));
	if (Digits.Len() == 2 && FChar::IsDigit(Digits[0]) && FChar::IsDigit(Digits[1]))
	{
		return FName(*(TEXT("TH-") + Digits));
	}
	return NAME_None;
}

bool FDTProvinceGeometry::ResolveZone(const FString& Zone, TArray<FName>& OutCodes, FString& OutError) const
{
	OutCodes.Reset();
	OutError.Reset();
	const FString Trimmed = Zone.TrimStartAndEnd();

	if (Trimmed.IsEmpty() || Trimmed.Equals(TEXT("all"), ESearchCase::IgnoreCase))
	{
		Entries.GetKeys(OutCodes);
		OutCodes.Sort(FNameLexicalLess());
		if (OutCodes.Num() == 0)
		{
			OutError = TEXT("province table is empty");
			return false;
		}
		return true;
	}

	for (const DTProvinceZones::FPreset& Preset : DTProvinceZones::Presets)
	{
		if (Trimmed.Equals(Preset.Name, ESearchCase::IgnoreCase))
		{
			TArray<FString> Codes;
			FString(Preset.Codes).ParseIntoArray(Codes, TEXT(","));
			for (const FString& C : Codes)
			{
				OutCodes.Add(FName(*C));
			}
			return true;
		}
	}

	EDTThaiRegion Region;
	if (DTProvinceZones::RegionFromName(Trimmed, Region))
	{
		for (const TPair<FName, FEntry>& Pair : Entries)
		{
			if (Pair.Value.Row.Region == Region)
			{
				OutCodes.Add(Pair.Key);
			}
		}
		OutCodes.Sort(FNameLexicalLess());
		if (OutCodes.Num() == 0)
		{
			OutError = FString::Printf(TEXT("no provinces of region '%s' in the table"), *Trimmed);
			return false;
		}
		return true;
	}

	TArray<FString> Tokens;
	Trimmed.ParseIntoArray(Tokens, TEXT(","), true);
	TArray<FString> Unknown;
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		FName Code = NormalizeCode(Token);
		if (Code.IsNone() || !Entries.Contains(Code))
		{
			Code = NAME_None;
			const FString Wanted = NormalizeName(Token);
			for (const TPair<FName, FEntry>& Pair : Entries)
			{
				if (NormalizeName(Pair.Value.Row.NameEN.ToString()) == Wanted || NormalizeName(Pair.Value.Row.NameTH.ToString()) == Wanted)
				{
					Code = Pair.Key;
					break;
				}
			}
		}
		if (Code.IsNone())
		{
			Unknown.Add(Token);
		}
		else
		{
			OutCodes.AddUnique(Code);
		}
	}

	if (Unknown.Num() > 0)
	{
		OutError = FString::Printf(TEXT("unknown province/zone: %s. %s"), *FString::Join(Unknown, TEXT(", ")), *GetZoneHelp());
		return false;
	}
	return OutCodes.Num() > 0;
}

FString FDTProvinceGeometry::Describe(FName Code) const
{
	const FEntry* Entry = Entries.Find(Code);
	return Entry ? FString::Printf(TEXT("%s %s"), *Code.ToString(), *Entry->Row.NameEN.ToString()) : Code.ToString();
}

FString FDTProvinceGeometry::GetZoneHelp()
{
	FString Names;
	for (const DTProvinceZones::FPreset& Preset : DTProvinceZones::Presets)
	{
		Names += FString(Preset.Name) + TEXT(", ");
	}
	return TEXT("Valid zones: all, ") + Names + TEXT("north, northeast, central, east, west, south, a code (TH-12) or a Thai/English province name; comma separated.");
}

FString FDTProvinceGeometry::RegionToString(EDTThaiRegion Region)
{
	switch (Region)
	{
	case EDTThaiRegion::North:     return TEXT("North");
	case EDTThaiRegion::Northeast: return TEXT("Northeast");
	case EDTThaiRegion::Central:   return TEXT("Central");
	case EDTThaiRegion::East:      return TEXT("East");
	case EDTThaiRegion::West:      return TEXT("West");
	case EDTThaiRegion::South:     return TEXT("South");
	default:                       return TEXT("Unknown");
	}
}
