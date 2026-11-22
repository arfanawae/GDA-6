#include "World/Province/DTProvinceSubsystem.h"
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

static FAutoConsoleCommandWithWorldAndArgs CmdTeleportProvince(
	TEXT("dt.Province.Teleport"),
	TEXT("Teleport the player to a province reference point, e.g. dt.Province.Teleport TH-12"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (World && Args.Num() > 0)
		{
			if (UDTProvinceSubsystem* Sub = World->GetSubsystem<UDTProvinceSubsystem>())
			{
				Sub->TeleportPlayerToProvince(FName(*Args[0].ToUpper()));
			}
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
	if (!Table)
	{
		UE_LOG(LogDowntown, Warning, TEXT("No ProvinceTable on the GameMode; province detection disabled."));
		return;
	}

	Table->ForeachRow<FDTProvinceRow>(TEXT("DTProvinceSubsystem"), [this](const FName& RowName, const FDTProvinceRow& Row)
	{
		FRuntimeProvince P;
		P.Row = Row;
		if (P.Row.Code.IsNone())
		{
			P.Row.Code = RowName;
		}
		P.Bounds = FBox2D(ForceInit);
		for (const FDTProvinceRing& Ring : Row.Rings)
		{
			FBox2D RingBox(Ring.Points);
			P.RingBounds.Add(RingBox);
			P.Bounds += RingBox;
		}
		Provinces.Add(P.Row.Code, MoveTemp(P));
	});
	UE_LOG(LogDowntown, Log, TEXT("Loaded %d provinces."), Provinces.Num());
}

bool UDTProvinceSubsystem::PointInRing(const TArray<FVector2D>& Ring, const FVector2D& Point)
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

bool UDTProvinceSubsystem::Contains(const FRuntimeProvince& P, const FVector2D& Point) const
{
	if (!P.Bounds.IsInside(Point))
	{
		return false;
	}
	for (int32 i = 0; i < P.Row.Rings.Num(); ++i)
	{
		if (P.RingBounds[i].IsInside(Point) && PointInRing(P.Row.Rings[i].Points, Point))
		{
			return true;
		}
	}
	return false;
}

FName UDTProvinceSubsystem::FindProvinceAt(const FVector& WorldLocation, FName Hint) const
{
	const FVector2D Point(WorldLocation.X, WorldLocation.Y);

	// 1) the hinted (current) province, 2) its neighbours, 3) everything.
	if (const FRuntimeProvince* HintP = Provinces.Find(Hint))
	{
		if (Contains(*HintP, Point))
		{
			return Hint;
		}
		for (const FName& N : HintP->Row.Neighbors)
		{
			if (const FRuntimeProvince* NP = Provinces.Find(N); NP && Contains(*NP, Point))
			{
				return N;
			}
		}
	}
	for (const TPair<FName, FRuntimeProvince>& Pair : Provinces)
	{
		if (Contains(Pair.Value, Point))
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

bool UDTProvinceSubsystem::GetProvinceRow(FName Code, FDTProvinceRow& OutRow) const
{
	if (const FRuntimeProvince* P = Provinces.Find(Code))
	{
		OutRow = P->Row;
		return true;
	}
	return false;
}

void UDTProvinceSubsystem::SetCurrent(FName NewProvince)
{
	const FName Old = CurrentProvince;
	CurrentProvince = NewProvince;
	bool bFirstVisit = false;
	if (!NewProvince.IsNone())
	{
		Visited.Add(NewProvince, &bFirstVisit);
		bFirstVisit = !bFirstVisit; // Add() reports "already in set"
	}
	OnProvinceChanged.Broadcast(NewProvince, Old, bFirstVisit);
}

void UDTProvinceSubsystem::Tick(float DeltaTime)
{
	if (Provinces.Num() == 0)
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

	const FName Found = FindProvinceAt(Player->GetActorLocation(), CurrentProvince);
	if (Found == CurrentProvince)
	{
		PendingProvince = NAME_None;
		PendingTime = 0.f;
		return;
	}

	// First detection after spawning is announced immediately.
	if (CurrentProvince.IsNone() && !Found.IsNone() && Visited.Num() == 0)
	{
		SetCurrent(Found);
		return;
	}

	if (Found != PendingProvince)
	{
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

bool UDTProvinceSubsystem::TeleportPlayerToProvince(FName Code)
{
	const FRuntimeProvince* P = Provinces.Find(Code);
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!P || !Player)
	{
		UE_LOG(LogDowntown, Warning, TEXT("Teleport failed: unknown province %s or no player."), *Code.ToString());
		return false;
	}

	// Find the ground (may be unloaded in World Partition: fall back to a safe height and let streaming catch up).
	const FVector2D XY = P->Row.ReferencePoint;
	FVector Target(XY.X, XY.Y, 5000.0);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTProvinceTeleport), false, Player);
	if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(XY.X, XY.Y, 200000.0), FVector(XY.X, XY.Y, -200000.0), ECC_Visibility, Params))
	{
		Target.Z = Hit.ImpactPoint.Z + 200.0;
	}

	Player->TeleportTo(Target, Player->GetActorRotation(), false, true);
	UE_LOG(LogDowntown, Log, TEXT("Teleported to %s (%s)"), *Code.ToString(), *P->Row.NameEN.ToString());
	return true;
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

	for (const TPair<FName, FRuntimeProvince>& Pair : Provinces)
	{
		if (!Pair.Value.Bounds.Intersect(View))
		{
			continue;
		}
		const FColor Color = Pair.Key == CurrentProvince ? FColor::Green : FColor::Yellow;
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
