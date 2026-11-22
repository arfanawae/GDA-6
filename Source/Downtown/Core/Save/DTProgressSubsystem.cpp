#include "Core/Save/DTProgressSubsystem.h"
#include "Core/Save/DTSaveGame.h"
#include "World/Province/DTProvinceSubsystem.h"
#include "Downtown.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

const TCHAR* UDTProgressSubsystem::SlotName = TEXT("Downtown_Progress");

namespace DTProgressConsole
{
	static UDTProgressSubsystem* Get(UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UDTProgressSubsystem* Sub = GI ? GI->GetSubsystem<UDTProgressSubsystem>() : nullptr;
		if (!Sub)
		{
			UE_LOG(LogDTGeo, Warning, TEXT("[Progress] Not available - run the command during PIE / in game."));
		}
		return Sub;
	}
}

static FAutoConsoleCommandWithWorld CmdSaveNow(TEXT("dt.Save.Now"), TEXT("Save province progress now."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UDTProgressSubsystem* S = DTProgressConsole::Get(World)) { S->SaveNow(false); } }));

static FAutoConsoleCommandWithWorld CmdSaveInfo(TEXT("dt.Save.Info"), TEXT("Log saved province progress."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (const UDTProgressSubsystem* S = DTProgressConsole::Get(World)) { S->LogInfo(); } }));

static FAutoConsoleCommandWithWorld CmdSaveReset(TEXT("dt.Save.Reset"), TEXT("Delete province progress (save slot + current session)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UDTProgressSubsystem* S = DTProgressConsole::Get(World)) { S->ResetProgress(); } }));

void UDTProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
		Save = Cast<UDTSaveGame>(Loaded);
		if (!Save)
		{
			UE_LOG(LogDTGeo, Warning, TEXT("[Progress] Slot '%s' exists but is not a DTSaveGame (or is corrupt) - starting fresh."), SlotName);
		}
		else if (Save->SaveVersion > UDTSaveGame::CurrentVersion)
		{
			UE_LOG(LogDTGeo, Warning, TEXT("[Progress] Slot '%s' has version %d (newer than %d) - ignored to avoid data loss."), SlotName, Save->SaveVersion, UDTSaveGame::CurrentVersion);
			Save = nullptr;
		}
		else
		{
			if (Save->FirstVisitTimes.Num() != Save->VisitedProvinces.Num())
			{
				Save->FirstVisitTimes.SetNum(Save->VisitedProvinces.Num());
			}
			UE_LOG(LogDTGeo, Display, TEXT("[Progress] Loaded slot '%s': %d visited province(s), last in %s, saved %s."),
				SlotName, Save->VisitedProvinces.Num(), *Save->LastProvince.ToString(), *Save->LastSaved.ToString());
		}
	}
	else
	{
		UE_LOG(LogDTGeo, Display, TEXT("[Progress] No save slot '%s' yet - new progress."), SlotName);
	}
}

void UDTProgressSubsystem::Deinitialize()
{
	if (bDirty && Save)
	{
		SaveNow(false); // synchronous on shutdown so nothing is lost
	}
	Super::Deinitialize();
}

UDTSaveGame* UDTProgressSubsystem::GetOrCreateSave()
{
	if (!Save)
	{
		Save = Cast<UDTSaveGame>(UGameplayStatics::CreateSaveGameObject(UDTSaveGame::StaticClass()));
	}
	return Save;
}

void UDTProgressSubsystem::AttachProvinceSubsystem(UDTProvinceSubsystem* Provinces)
{
	if (!Provinces)
	{
		return;
	}
	AttachedProvinces = Provinces;
	Provinces->OnProvinceChanged.AddUniqueDynamic(this, &UDTProgressSubsystem::HandleProvinceChanged);

	if (!Save || Save->VisitedProvinces.Num() == 0)
	{
		return;
	}

	// Drop codes the current province table does not know (renamed/removed rows, partial test tables).
	TArray<FName> Known;
	TArray<FName> Unknown;
	FDTProvinceRow Row;
	for (const FName& Code : Save->VisitedProvinces)
	{
		(Provinces->GetProvinceRow(Code, Row) ? Known : Unknown).Add(Code);
	}
	Provinces->SetVisited(Known);
	UE_LOG(LogDTGeo, Display, TEXT("[Progress] Restored %d/%d visited province(s) into the world%s%s"),
		Known.Num(), Provinces->GetProvinceCount(),
		Unknown.Num() > 0 ? TEXT("; kept in the save but unknown to this table: ") : TEXT(""),
		Unknown.Num() > 0 ? *FString::JoinBy(Unknown, TEXT(", "), [](const FName& N) { return N.ToString(); }) : TEXT(""));
}

void UDTProgressSubsystem::HandleProvinceChanged(FName NewProvince, FName OldProvince, bool bFirstVisit)
{
	UDTSaveGame* S = GetOrCreateSave();
	if (!S || NewProvince.IsNone())
	{
		return;
	}
	S->LastProvince = NewProvince;
	bDirty = true;
	if (bFirstVisit && !S->VisitedProvinces.Contains(NewProvince))
	{
		S->VisitedProvinces.Add(NewProvince);
		S->FirstVisitTimes.Add(FDateTime::UtcNow());
		SaveNow(true);
	}
}

void UDTProgressSubsystem::SaveNow(bool bAsync)
{
	UDTSaveGame* S = GetOrCreateSave();
	if (!S)
	{
		return;
	}
	if (bAsync && bSaveInFlight)
	{
		bSaveQueued = true; // save again once the current write finishes
		return;
	}

	S->SaveVersion = UDTSaveGame::CurrentVersion;
	S->LastSaved = FDateTime::UtcNow();
	if (bAsync)
	{
		bSaveInFlight = true;
		bDirty = false;
		UGameplayStatics::AsyncSaveGameToSlot(S, SlotName, 0, FAsyncSaveGameToSlotDelegate::CreateUObject(this, &UDTProgressSubsystem::HandleAsyncSaved));
	}
	else
	{
		const bool bOk = UGameplayStatics::SaveGameToSlot(S, SlotName, 0);
		bDirty = !bOk;
		UE_LOG(LogDTGeo, Display, TEXT("[Progress] Saved slot '%s' (%d provinces): %s"), SlotName, S->VisitedProvinces.Num(), bOk ? TEXT("ok") : TEXT("FAILED"));
		OnProgressSaved.Broadcast(bOk, S->VisitedProvinces.Num());
	}
}

void UDTProgressSubsystem::HandleAsyncSaved(const FString& Slot, const int32 UserIndex, bool bSuccess)
{
	bSaveInFlight = false;
	const int32 Count = Save ? Save->VisitedProvinces.Num() : 0;
	UE_LOG(LogDTGeo, Display, TEXT("[Progress] Saved slot '%s' (%d provinces): %s"), *Slot, Count, bSuccess ? TEXT("ok") : TEXT("FAILED"));
	if (!bSuccess)
	{
		bDirty = true;
	}
	OnProgressSaved.Broadcast(bSuccess, Count);
	if (bSaveQueued)
	{
		bSaveQueued = false;
		SaveNow(true);
	}
}

void UDTProgressSubsystem::ResetProgress()
{
	const int32 Before = Save ? Save->VisitedProvinces.Num() : 0;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}
	Save = nullptr;
	bDirty = false;
	if (UDTProvinceSubsystem* Provinces = AttachedProvinces.Get())
	{
		Provinces->ResetVisited();
	}
	UE_LOG(LogDTGeo, Display, TEXT("[Progress] Progress reset (%d visited province(s) removed, slot '%s' deleted)."), Before, SlotName);
}

TArray<FName> UDTProgressSubsystem::GetVisitedProvinces() const
{
	return Save ? Save->VisitedProvinces : TArray<FName>();
}

bool UDTProgressSubsystem::GetFirstVisitTime(FName Province, FDateTime& OutTime) const
{
	const int32 Index = Save ? Save->VisitedProvinces.IndexOfByKey(Province) : INDEX_NONE;
	if (Index == INDEX_NONE || !Save->FirstVisitTimes.IsValidIndex(Index))
	{
		return false;
	}
	OutTime = Save->FirstVisitTimes[Index];
	return true;
}

void UDTProgressSubsystem::LogInfo() const
{
	if (!Save)
	{
		UE_LOG(LogDTGeo, Display, TEXT("[Progress] Nothing saved yet (slot '%s')."), SlotName);
		return;
	}
	UE_LOG(LogDTGeo, Display, TEXT("[Progress] Slot '%s' v%d: %d visited, last %s, saved %s UTC%s"), SlotName, Save->SaveVersion,
		Save->VisitedProvinces.Num(), *Save->LastProvince.ToString(), *Save->LastSaved.ToString(), bDirty ? TEXT(" (unsaved changes)") : TEXT(""));
	for (int32 i = 0; i < Save->VisitedProvinces.Num(); ++i)
	{
		UE_LOG(LogDTGeo, Display, TEXT("  %-6s first visit %s UTC"), *Save->VisitedProvinces[i].ToString(),
			Save->FirstVisitTimes.IsValidIndex(i) ? *Save->FirstVisitTimes[i].ToString() : TEXT("?"));
	}
}
