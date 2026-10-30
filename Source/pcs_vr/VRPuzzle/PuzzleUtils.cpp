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

	void ReleaseGrabComponent(UActorComponent* GrabComp)
	{
		UFunction* Func = GrabComp ? GrabComp->FindFunction(TEXT("TryRelease")) : nullptr;
		if (!Func)
		{
			return;
		}
		// 블루프린트 함수 인자/반환값 버퍼 (TryRelease는 인자 없이 bool 반환)
		uint8* Params = static_cast<uint8*>(FMemory_Alloca(FMath::Max<int32>(Func->ParmsSize, 1)));
		FMemory::Memzero(Params, Func->ParmsSize);
		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}
		GrabComp->ProcessEvent(Func, Params);
		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
	}

	bool TryGrabWithHand(UActorComponent* GrabComp, USceneComponent* Hand)
	{
		UFunction* Func = GrabComp ? GrabComp->FindFunction(TEXT("TryGrab")) : nullptr;
		if (!Func || !Hand)
		{
			return false;
		}
		// TryGrab(MotionController) → bool. 첫 오브젝트 인자에 손을 넣고, bool 반환값을 읽는다
		uint8* Params = static_cast<uint8*>(FMemory_Alloca(FMath::Max<int32>(Func->ParmsSize, 1)));
		FMemory::Memzero(Params, Func->ParmsSize);
		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}
		for (TFieldIterator<FObjectProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_OutParm) && Hand->IsA(It->PropertyClass))
			{
				It->SetObjectPropertyValue_InContainer(Params, Hand);
				break;
			}
		}
		GrabComp->ProcessEvent(Func, Params);
		bool bGrabbed = false;
		for (TFieldIterator<FBoolProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
			{
				bGrabbed = It->GetPropertyValue_InContainer(Params);
			}
		}
		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		return bGrabbed;
	}
}
