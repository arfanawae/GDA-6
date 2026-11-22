#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTProvinceSign.generated.h"

class UTextRenderComponent;

/**
 * "ยินดีต้อนรับสู่จังหวัด..." roadside sign, spawned by ADTGeoImporter at every road/border crossing.
 * BP_ProvinceSign adds the mesh; the text is filled from DT_Provinces at BeginPlay.
 * NOTE: TextRender/UMG need a font with Thai glyphs (e.g. Noto Sans Thai, Sarabun) - the default Roboto has none.
 */
UCLASS()
class DOWNTOWN_API ADTProvinceSign : public AActor
{
	GENERATED_BODY()

public:
	ADTProvinceSign();

	/** Province the driver is entering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Province")
	FName ProvinceCode;

	/** Province the driver is leaving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Province")
	FName FromProvinceCode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Province")
	FString RoadName;

protected:
	virtual void BeginPlay() override;

	/** Set mesh text / materials in BP. Default implementation fills the TextRender. */
	UFUNCTION(BlueprintNativeEvent, Category = "Province")
	void ApplyProvinceText(const FText& NameTH, const FText& NameEN);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> Text;
};
