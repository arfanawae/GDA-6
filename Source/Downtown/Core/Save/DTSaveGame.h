#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DTSaveGame.generated.h"

/** Persistent player progress (slot "Downtown_Progress"). Bump SaveVersion when the layout changes. */
UCLASS()
class DOWNTOWN_API UDTSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(VisibleAnywhere, Category = "Save") int32 SaveVersion = CurrentVersion;

	/** ISO codes of every province the player has entered ("TH-10", ...). */
	UPROPERTY(VisibleAnywhere, Category = "Save") TArray<FName> VisitedProvinces;

	/** Province the player was in when the game last saved. */
	UPROPERTY(VisibleAnywhere, Category = "Save") FName LastProvince;

	/** When each province was first visited (same order as VisitedProvinces). */
	UPROPERTY(VisibleAnywhere, Category = "Save") TArray<FDateTime> FirstVisitTimes;

	UPROPERTY(VisibleAnywhere, Category = "Save") FDateTime LastSaved;
};
