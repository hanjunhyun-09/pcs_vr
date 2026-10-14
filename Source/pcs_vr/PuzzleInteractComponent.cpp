#include "PuzzleInteractComponent.h"

#include "Engine/World.h"
#include "VRInteractionSubsystem.h"

void UPuzzleInteractComponent::Interact(AActor* InteractInstigator)
{
	if (bInteractEnabled)
	{
		OnInteract.Broadcast(InteractInstigator);
	}
}

void UPuzzleInteractComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UVRInteractionSubsystem* Sub = World->GetSubsystem<UVRInteractionSubsystem>())
		{
			Sub->Register(this);
		}
	}
}

void UPuzzleInteractComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UVRInteractionSubsystem* Sub = World->GetSubsystem<UVRInteractionSubsystem>())
		{
			Sub->Unregister(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}
