#include "AI/Traffic/DTTrafficLight.h"
#include "TimerManager.h"

ADTTrafficLight::ADTTrafficLight()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADTTrafficLight::BeginPlay()
{
	Super::BeginPlay();
	SetLightState(PhaseOffset > 0.f ? EDTLightState::Red : EDTLightState::Green);
	const float First = PhaseOffset > 0.f ? PhaseOffset : GreenTime;
	GetWorldTimerManager().SetTimer(CycleTimer, this, &ADTTrafficLight::Advance, First, false);
}

void ADTTrafficLight::Advance()
{
	float Next = GreenTime;
	switch (LightState)
	{
	case EDTLightState::Green:  SetLightState(EDTLightState::Yellow); Next = YellowTime; break;
	case EDTLightState::Yellow: SetLightState(EDTLightState::Red);    Next = RedTime;    break;
	case EDTLightState::Red:    SetLightState(EDTLightState::Green);  Next = GreenTime;  break;
	}
	GetWorldTimerManager().SetTimer(CycleTimer, this, &ADTTrafficLight::Advance, Next, false);
}

void ADTTrafficLight::SetLightState(EDTLightState NewState)
{
	LightState = NewState;
	OnLightChanged.Broadcast(LightState);
}
