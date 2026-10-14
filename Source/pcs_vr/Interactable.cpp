#include "Interactable.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

bool UInteractLibrary::TryInteract(AActor* Target, AActor* Interactor)
{
	if (!Target)
	{
		return false;
	}

	bool bAny = false;
	if (Target->Implements<UInteractable>())
	{
		IInteractable::Execute_Interact(Target, Interactor);
		bAny = true;
	}

	TInlineComponentArray<UActorComponent*> Comps;
	Target->GetComponents(Comps);
	for (UActorComponent* C : Comps)
	{
		if (C && C->Implements<UInteractable>())
		{
			IInteractable::Execute_Interact(C, Interactor);
			bAny = true;
		}
	}
	return bAny;
}

bool UInteractLibrary::CanInteract(const AActor* Target)
{
	if (!Target)
	{
		return false;
	}
	if (Target->Implements<UInteractable>())
	{
		return true;
	}

	TInlineComponentArray<UActorComponent*> Comps;
	Target->GetComponents(Comps);
	for (const UActorComponent* C : Comps)
	{
		if (C && C->Implements<UInteractable>())
		{
			return true;
		}
	}
	return false;
}
