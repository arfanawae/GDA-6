#include "World/Province/DTProvinceSign.h"
#include "World/Province/DTProvinceSubsystem.h"
#include "Components/TextRenderComponent.h"

ADTProvinceSign::ADTProvinceSign()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetupAttachment(RootComponent);
	Text->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetWorldSize(60.f);
}

void ADTProvinceSign::BeginPlay()
{
	Super::BeginPlay();
	if (const UDTProvinceSubsystem* Provinces = GetWorld()->GetSubsystem<UDTProvinceSubsystem>())
	{
		FDTProvinceRow Row;
		if (Provinces->GetProvinceRow(ProvinceCode, Row))
		{
			ApplyProvinceText(Row.NameTH, Row.NameEN);
		}
	}
}

void ADTProvinceSign::ApplyProvinceText_Implementation(const FText& NameTH, const FText& NameEN)
{
	Text->SetText(FText::Format(NSLOCTEXT("Downtown", "ProvinceSign", "\u0E22\u0E34\u0E19\u0E14\u0E35\u0E15\u0E49\u0E2D\u0E19\u0E23\u0E31\u0E1A\u0E2A\u0E39\u0E48 {0}\nWelcome to {1}"), NameTH, NameEN));
}
