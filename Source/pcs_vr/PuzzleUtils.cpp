#include "PuzzleUtils.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

namespace PuzzleUtils
{
	UMaterialInstanceDynamic* ApplySolidColor(UPrimitiveComponent* Comp, const FLinearColor& Color, float Emissive)
	{
		if (!Comp)
		{
			return nullptr;
		}
		static const TCHAR* SolidPath = TEXT("/Game/VRPuzzle/Materials/M_VRP_Solid.M_VRP_Solid");
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, SolidPath);
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Comp);
		SetSolidColor(MID, Color, Emissive);
		for (int32 i = 0; i < FMath::Max(1, Comp->GetNumMaterials()); ++i)
		{
			Comp->SetMaterial(i, MID);
		}
		return MID;
	}

	void SetSolidColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Emissive)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		}
	}

	APawn* GetPlayerPawn(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	USceneComponent* GetPlayerCamera(const UObject* WorldContext)
	{
		const APawn* Pawn = GetPlayerPawn(WorldContext);
		return Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	}

	bool GetPlayerView(const UObject* WorldContext, FVector& OutLocation, FRotator& OutRotation)
	{
		if (const USceneComponent* Cam = GetPlayerCamera(WorldContext))
		{
			OutLocation = Cam->GetComponentLocation();
			OutRotation = Cam->GetComponentRotation();
			return true;
		}
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
		{
			PC->GetPlayerViewPoint(OutLocation, OutRotation);
			return true;
		}
		return false;
	}

	USceneComponent* FindPawnComponent(const APawn* Pawn, const TCHAR* Name)
	{
		if (!Pawn)
		{
			return nullptr;
		}
		TInlineComponentArray<USceneComponent*> Comps;
		Pawn->GetComponents(Comps);
		for (USceneComponent* C : Comps)
		{
			if (C && C->GetName().Equals(Name))
			{
				return C;
			}
		}
		return nullptr;
	}

	UActorComponent* FindGrabComponent(const AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}
		TInlineComponentArray<UActorComponent*> Comps;
		Actor->GetComponents(Comps);
		for (UActorComponent* C : Comps)
		{
			if (C && C->GetClass()->GetName().Contains(TEXT("GrabComponent")))
			{
				return C;
			}
		}
		return nullptr;
	}

	bool IsGrabComponentHeld(const UActorComponent* GrabComp)
	{
		if (!GrabComp)
		{
			return false;
		}
		const FBoolProperty* HeldProp = FindFProperty<FBoolProperty>(GrabComp->GetClass(), TEXT("bIsHeld"));
		return HeldProp && HeldProp->GetPropertyValue_InContainer(GrabComp);
	}
}
