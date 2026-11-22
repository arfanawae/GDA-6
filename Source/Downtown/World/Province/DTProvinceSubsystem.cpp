#include "World/Province/DTProvinceSubsystem.h"
#include "World/Geo/DTGeoLibrary.h"
#include "World/Geo/DTGeoSettings.h"
#include "Core/DTGameMode.h"
#include "Downtown.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarDrawProvinceBorders(
	TEXT("dt.Province.DrawBorders"), 0, TEXT("Draw province borders around the player (1 = on)."));

static TAutoConsoleVariable<int32> CVarLogProvinceChecks(
	TEXT("dt.Province.LogChecks"), 0, TEXT("Verbose LogDTGeo output for every province detection step (1 = on)."));

namespace DTProvinceConsole
{
	static UDTProvinceSubsystem* Get(UWorld* World)
	{
		UDTProvinceSubsystem* Sub = World ? World->GetSubsystem<UDTProvinceSubsystem>() : nullptr;
		if (!Sub)
		{
			UE_LOG(LogDTGeo, Warning, TEXT("[Province] Not available in this world - run the command during PIE / in game."));
		}
		return Sub;
	}

	static FString JoinArgs(const TArray<FString>& Args)
	{
		return FString::Join(Args, TEXT(" "));
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdTeleportProvince(
	TEXT("dt.Province.Teleport"),
	TEXT("Teleport the player to a province reference point: dt.Province.Teleport TH-12 | Nonthaburi"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UDTProvinceSubsystem* Sub = DTProvinceConsole::Get(World))
		{
			Sub->TeleportPlayerToZone(DTProvinceConsole::JoinArgs(Args));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdProvinceZone(
	TEXT("dt.Province.Zone"),
	TEXT("Limit province detection to a zone: dt.Province.Zone test-area | TH-12,TH-13 | north | all"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UDTProvinceSubsystem* Sub = DTProvinceConsole::Get(World))
		{
			Sub->SetActiveZone(DTProvinceConsole::JoinArgs(Args));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdProvinceList(
	TEXT("dt.Province.List"),
	TEXT("List provinces of a zone (default: the active zone): dt.Province.List bangkok-metro"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UDTProvinceSubsystem* Sub = DTProvinceConsole::Get(World))
		{
			Sub->LogZone(Args.Num() > 0 ? DTProvinceConsole::JoinArgs(Args) : Sub->GetActiveZone());
		}
	}));

static FAutoConsoleCommandWithWorld CmdProvinceWhere(
	TEXT("dt.Province.Where"),
	TEXT("Log the player's world position, lat/lon and province."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const UDTProvinceSubsystem* Sub = DTProvinceConsole::Get(World))
		{
			Sub->LogPlayerLocation();
		}
	}));

static FAutoConsoleCommandWithWorld CmdProvinceResetVisited(
	TEXT("dt.Province.ResetVisited"),
	TEXT("Clear visited-province (checkpoint) progress."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UDTProvinceSubsystem* Sub = DTProvinceConsole::Get(World))
		{
			Sub->ResetVisited();
		}
	}));

bool UDTProvinceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDTProvinceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const ADTGameMode* GM = InWorld.GetAuthGameMode<ADTGameMode>();
	const UDataTable* Table = GM ? GM->ProvinceTable.Get() : nullptr;
	if (!GM)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[Province] GameMode is not a DTGameMode - province detection disabled."));
		return;
	}
	if (!Table)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[Province] %s has no ProvinceTable - province detection disabled. Set it to DT_Provinces."), *GM->GetClass()->GetName());
		return;
	}

	TArray<FString> Warnings;
	Geometry.Build(Table, Warnings);
	LogLoadSummary(Table, Warnings);
}

void UDTProvinceSubsystem::LogLoadSummary(const UDataTable* Table, const TArray<FString>& Warnings) const
{
	const UDTGeoSettings* S = GetDefault<UDTGeoSettings>();
	UE_LOG(LogDTGeo, Display, TEXT("[Province] Table: %s (%d rows)"), *Table->GetPathName(), Table->GetRowMap().Num());
	UE_LOG(LogDTGeo, Display, TEXT("[Province] Projection: origin %.4f N %.4f E, horizontal 1:%.0f (1 game km = %.0f real km), vertical 1:%.0f"),
		S->OriginLat, S->OriginLon, S->HorizontalScale, S->HorizontalScale, S->VerticalScale);

	TMap<EDTThaiRegion, int32> PerRegion;
	int32 Points = 0;
	FBox2D All(ForceInit);
	for (const TPair<FName, FDTProvinceGeometry::FEntry>& Pair : Geometry.GetEntries())
	{
		PerRegion.FindOrAdd(Pair.Value.Row.Region)++;
		All += Pair.Value.Bounds;
		for (const FDTProvinceRing& Ring : Pair.Value.Row.Rings)
		{
			Points += Ring.Points.Num();
		}
	}

	FString Regions;
	for (const TPair<EDTThaiRegion, int32>& R : PerRegion)
	{
		Regions += FString::Printf(TEXT("%s %d, "), *FDTProvinceGeometry::RegionToString(R.Key), R.Value);
	}
	Regions.RemoveFromEnd(TEXT(", "));

	UE_LOG(LogDTGeo, Display, TEXT("[Province] Loaded %d/%d provinces, %d outline points (%s)"),
		Geometry.Num(), Table->GetRowMap().Num(), Points, *Regions);
	if (All.bIsValid)
	{
		const FVector2D Size = All.GetSize() / 100000.0;
		const FVector2D LLMin = UDTGeoLibrary::WorldToLatLon(FVector(All.Min.X, All.Min.Y, 0.0));
		const FVector2D LLMax = UDTGeoLibrary::WorldToLatLon(FVector(All.Max.X, All.Max.Y, 0.0));
		UE_LOG(LogDTGeo, Display, TEXT("[Province] World bounds X %.0f..%.0f, Y %.0f..%.0f cm = %.2f x %.2f km in game (%.0f x %.0f km real), lat %.3f..%.3f, lon %.3f..%.3f"),
			All.Min.X, All.Max.X, All.Min.Y, All.Max.Y, Size.X, Size.Y, Size.X * S->HorizontalScale, Size.Y * S->HorizontalScale,
			LLMin.X, LLMax.X, LLMin.Y, LLMax.Y);
	}
	if (Geometry.Num() != 77)
	{
		UE_LOG(LogDTGeo, Display, TEXT("[Province] Note: %d provinces loaded (Thailand has 77) - fine for a partial test table."), Geometry.Num());
	}
	for (const FString& W : Warnings)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[Province] %s"), *W);
	}
}

FString UDTProvinceSubsystem::DescribeLocation(const FVector& Location) const
{
	return FString::Printf(TEXT("X=%.0f Y=%.0f Z=%.0f (%s)"), Location.X, Location.Y, Location.Z, *UDTGeoLibrary::FormatLatLon(Location));
}

FName UDTProvinceSubsystem::FindProvinceAt(const FVector& WorldLocation, FName Hint) const
{
	return Geometry.FindAt(FVector2D(WorldLocation.X, WorldLocation.Y), Hint, ActiveZone.Num() > 0 ? &ActiveZone : nullptr);
}

bool UDTProvinceSubsystem::GetProvinceRow(FName Code, FDTProvinceRow& OutRow) const
{
	if (const FDTProvinceGeometry::FEntry* Entry = Geometry.Find(Code))
	{
		OutRow = Entry->Row;
		return true;
	}
	return false;
}

void UDTProvinceSubsystem::ResetVisited()
{
	UE_LOG(LogDTGeo, Display, TEXT("[Province] Visited progress cleared (%d -> 0)."), Visited.Num());
	Visited.Reset();
	CurrentProvince = NAME_None;
	PendingProvince = NAME_None;
	PendingTime = 0.f;
}

bool UDTProvinceSubsystem::SetActiveZone(const FString& Zone)
{
	TArray<FName> Codes;
	FString Error;
	if (!Geometry.ResolveZone(Zone, Codes, Error))
	{
		UE_LOG(LogDTGeo, Error, TEXT("[Province] SetActiveZone('%s') failed: %s"), *Zone, *Error);
		return false;
	}

	const bool bAll = Zone.TrimStartAndEnd().IsEmpty() || Zone.TrimStartAndEnd().Equals(TEXT("all"), ESearchCase::IgnoreCase);
	ActiveZone = bAll ? TSet<FName>() : TSet<FName>(Codes);
	ActiveZoneName = bAll ? TEXT("all") : Zone.TrimStartAndEnd();

	FString Names;
	for (const FName& Code : Codes)
	{
		Names += Geometry.Describe(Code) + TEXT(", ");
	}
	Names.RemoveFromEnd(TEXT(", "));
	UE_LOG(LogDTGeo, Display, TEXT("[Province] Active zone '%s': %d province(s)%s%s"),
		*ActiveZoneName, Codes.Num(), bAll ? TEXT("") : TEXT(" - "), bAll ? TEXT("") : *Names);

	// Re-detect from scratch so the next check announces the province inside the new zone.
	CurrentProvince = NAME_None;
	PendingProvince = NAME_None;
	PendingTime = 0.f;
	bLoggedOutside = false;
	return true;
}

void UDTProvinceSubsystem::LogZone(const FString& Zone) const
{
	TArray<FName> Codes;
	FString Error;
	if (!Geometry.ResolveZone(Zone, Codes, Error))
	{
		UE_LOG(LogDTGeo, Error, TEXT("[Province] List '%s' failed: %s"), *Zone, *Error);
		return;
	}
	const double Scale = GetDefault<UDTGeoSettings>()->HorizontalScale;
	UE_LOG(LogDTGeo, Display, TEXT("[Province] Zone '%s': %d province(s)"), *Zone, Codes.Num());
	for (const FName& Code : Codes)
	{
		const FDTProvinceGeometry::FEntry* E = Geometry.Find(Code);
		if (!E)
		{
			UE_LOG(LogDTGeo, Warning, TEXT("  %s: not in the loaded table"), *Code.ToString());
			continue;
		}
		const FVector2D Size = E->Bounds.GetSize() / 100000.0;
		UE_LOG(LogDTGeo, Display, TEXT("  %-6s %-24s %-9s ref X=%9.0f Y=%9.0f (%.4f N %.4f E) size %.2f x %.2f km game / %.0f x %.0f km real%s%s"),
			*Code.ToString(), *E->Row.NameEN.ToString(), *FDTProvinceGeometry::RegionToString(E->Row.Region),
			E->Row.ReferencePoint.X, E->Row.ReferencePoint.Y, E->Row.ReferenceLatLon.X, E->Row.ReferenceLatLon.Y,
			Size.X, Size.Y, Size.X * Scale, Size.Y * Scale,
			Visited.Contains(Code) ? TEXT(" [visited]") : TEXT(""), Code == CurrentProvince ? TEXT(" [current]") : TEXT(""));
	}
}

void UDTProvinceSubsystem::LogPlayerLocation() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[Province] Where: no player pawn."));
		return;
	}
	const FVector Location = Player->GetActorLocation();
	const FName Found = FindProvinceAt(Location, CurrentProvince);
	UE_LOG(LogDTGeo, Display, TEXT("[Province] Player at %s -> %s | current %s | visited %d/%d | zone '%s'"),
		*DescribeLocation(Location), Found.IsNone() ? TEXT("<no province>") : *Geometry.Describe(Found),
		CurrentProvince.IsNone() ? TEXT("<none>") : *Geometry.Describe(CurrentProvince), Visited.Num(), Geometry.Num(), *ActiveZoneName);
}

void UDTProvinceSubsystem::SetCurrent(FName NewProvince)
{
	const FName Old = CurrentProvince;
	CurrentProvince = NewProvince;
	bool bAlreadyVisited = false;
	if (!NewProvince.IsNone())
	{
		Visited.Add(NewProvince, &bAlreadyVisited);
	}
	const bool bFirstVisit = !NewProvince.IsNone() && !bAlreadyVisited;

	if (const FDTProvinceGeometry::FEntry* E = Geometry.Find(NewProvince))
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		UE_LOG(LogDTGeo, Display, TEXT("[Province] Entered %s / %s (%s) from %s%s | visited %d/%d | at %s"),
			*Geometry.Describe(NewProvince), *E->Row.NameTH.ToString(), *FDTProvinceGeometry::RegionToString(E->Row.Region),
			Old.IsNone() ? TEXT("<start>") : *Geometry.Describe(Old), bFirstVisit ? TEXT(" | FIRST VISIT") : TEXT(""),
			Visited.Num(), Geometry.Num(), Player ? *DescribeLocation(Player->GetActorLocation()) : TEXT("?"));
	}
	OnProvinceChanged.Broadcast(NewProvince, Old, bFirstVisit);
}

void UDTProvinceSubsystem::Tick(float DeltaTime)
{
	if (Geometry.IsEmpty())
	{
		return;
	}

	if (CVarDrawProvinceBorders.GetValueOnGameThread() > 0)
	{
		DebugDrawBorders();
	}

	CheckTimer += DeltaTime;
	if (CheckTimer < CheckInterval)
	{
		return;
	}
	const float Elapsed = CheckTimer;
	CheckTimer = 0.f;

	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		return;
	}

	const FVector Location = Player->GetActorLocation();
	if (!FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y))
	{
		return;
	}

	const bool bVerbose = CVarLogProvinceChecks.GetValueOnGameThread() > 0;
	const FName Found = FindProvinceAt(Location, CurrentProvince);

	if (Found.IsNone())
	{
		if (!bLoggedOutside)
		{
			UE_LOG(LogDTGeo, Log, TEXT("[Province] Player outside every %sprovince at %s (sea, border or unloaded data) - keeping '%s'."),
				ActiveZone.Num() > 0 ? TEXT("active-zone ") : TEXT(""), *DescribeLocation(Location),
				CurrentProvince.IsNone() ? TEXT("<none>") : *CurrentProvince.ToString());
			bLoggedOutside = true;
		}
	}
	else
	{
		bLoggedOutside = false;
	}

	if (Found == CurrentProvince)
	{
		PendingProvince = NAME_None;
		PendingTime = 0.f;
		return;
	}

	// First detection after spawning (or after a zone change) is announced immediately.
	if (CurrentProvince.IsNone() && !Found.IsNone())
	{
		SetCurrent(Found);
		return;
	}

	if (Found != PendingProvince)
	{
		if (bVerbose)
		{
			UE_LOG(LogDTGeo, Log, TEXT("[Province] check: now inside %s, waiting %.1fs to confirm"), *Found.ToString(), ConfirmSeconds);
		}
		PendingProvince = Found;
		PendingTime = 0.f;
		return;
	}

	PendingTime += Elapsed;
	if (PendingTime >= ConfirmSeconds && !Found.IsNone()) // leaving to the sea keeps the last province
	{
		SetCurrent(Found);
		PendingProvince = NAME_None;
		PendingTime = 0.f;
	}
}

bool UDTProvinceSubsystem::TeleportPlayerToZone(const FString& Zone)
{
	TArray<FName> Codes;
	FString Error;
	if (!Geometry.ResolveZone(Zone, Codes, Error) || Codes.Num() == 0)
	{
		UE_LOG(LogDTGeo, Error, TEXT("[Province] Teleport '%s' failed: %s"), *Zone, *Error);
		return false;
	}
	if (Codes.Num() > 1)
	{
		UE_LOG(LogDTGeo, Display, TEXT("[Province] Teleport '%s' matches %d provinces - using the first (%s)."), *Zone, Codes.Num(), *Codes[0].ToString());
	}
	return TeleportPlayerToProvince(Codes[0]);
}

bool UDTProvinceSubsystem::TeleportPlayerToProvince(FName Code)
{
	const FDTProvinceGeometry::FEntry* E = Geometry.Find(Code);
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!E || !Player)
	{
		UE_LOG(LogDTGeo, Warning, TEXT("[Province] Teleport failed: %s."), !E ? *FString::Printf(TEXT("unknown province %s"), *Code.ToString()) : TEXT("no player pawn"));
		return false;
	}

	// Find the ground (may be unloaded in World Partition: fall back to a safe height and let streaming catch up).
	const FVector2D XY = E->Row.ReferencePoint;
	FVector Target(XY.X, XY.Y, 5000.0);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTProvinceTeleport), false, Player);
	const bool bGround = GetWorld()->LineTraceSingleByChannel(Hit, FVector(XY.X, XY.Y, 200000.0), FVector(XY.X, XY.Y, -200000.0), ECC_Visibility, Params);
	if (bGround)
	{
		Target.Z = Hit.ImpactPoint.Z + 200.0;
	}

	const bool bMoved = Player->TeleportTo(Target, Player->GetActorRotation(), false, true);
	UE_LOG(LogDTGeo, Display, TEXT("[Province] Teleport to %s / %s at %s - %s%s"),
		*Geometry.Describe(Code), *E->Row.NameTH.ToString(), *DescribeLocation(Target),
		bMoved ? TEXT("ok") : TEXT("FAILED"),
		bGround ? TEXT("") : TEXT(" (no ground under the point yet: World Partition cell not loaded or no landscape - placed at Z=5000)"));
	return bMoved;
}

void UDTProvinceSubsystem::DebugDrawBorders() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		return;
	}
	const FVector Center = Player->GetActorLocation();
	const double Range = 300000.0; // 3 km game
	const FBox2D View(FVector2D(Center.X - Range, Center.Y - Range), FVector2D(Center.X + Range, Center.Y + Range));
	const double Z = Center.Z + 200.0;

	for (const TPair<FName, FDTProvinceGeometry::FEntry>& Pair : Geometry.GetEntries())
	{
		if (!Pair.Value.Bounds.Intersect(View))
		{
			continue;
		}
		const bool bInZone = ActiveZone.Num() == 0 || ActiveZone.Contains(Pair.Key);
		const FColor Color = Pair.Key == CurrentProvince ? FColor::Green : (bInZone ? FColor::Yellow : FColor(90, 90, 90));
		for (const FDTProvinceRing& Ring : Pair.Value.Row.Rings)
		{
			for (int32 i = 0; i < Ring.Points.Num(); ++i)
			{
				const FVector2D& A = Ring.Points[i];
				const FVector2D& B = Ring.Points[(i + 1) % Ring.Points.Num()];
				if (View.IsInside(A) || View.IsInside(B))
				{
					DrawDebugLine(GetWorld(), FVector(A.X, A.Y, Z), FVector(B.X, B.Y, Z), Color, false, -1.f, 0, 20.f);
				}
			}
		}
	}
}
