#include "Interaction/DTInteractionComponent.h"
#include "Interaction/DTInteractable.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

UDTInteractionComponent::UDTInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDTInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetTimerManager().SetTimer(ScanTimer, this, &UDTInteractionComponent::UpdateFocus, ScanInterval, true);
}

void UDTInteractionComponent::UpdateFocus()
{
	AActor* Owner = GetOwner();
	if (!Owner || Owner->IsHidden())
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTInteractScan), false, Owner);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_Vehicle);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	GetWorld()->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(InteractRadius), Params);

	// Prefer the closest interactable in front of the owner.
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate || !Candidate->Implements<UDTInteractable>() || !IDTInteractable::Execute_CanInteract(Candidate, Owner))
		{
			continue;
		}
		const FVector ToCandidate = Candidate->GetActorLocation() - Owner->GetActorLocation();
		const float Facing = FVector::DotProduct(Owner->GetActorForwardVector(), ToCandidate.GetSafeNormal());
		const float Score = ToCandidate.Size() * (1.5f - Facing);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	if (Best != FocusedActor.Get())
	{
		FocusedActor = Best;
		OnFocusChanged.Broadcast(Best, Best ? IDTInteractable::Execute_GetInteractionPrompt(Best) : FText::GetEmpty());
	}
}

bool UDTInteractionComponent::TryInteract()
{
	AActor* Target = FocusedActor.Get();
	if (Target && IDTInteractable::Execute_CanInteract(Target, GetOwner()))
	{
		IDTInteractable::Execute_Interact(Target, GetOwner());
		return true;
	}
	return false;
}
