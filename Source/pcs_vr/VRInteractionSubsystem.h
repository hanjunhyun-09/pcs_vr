#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VRInteractionSubsystem.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;
class UEnhancedPlayerInput;
class UPuzzleInteractComponent;

/**
 * 트리거 상호작용 입력을 받아서 손 가까이에 있는 UPuzzleInteractComponent를 누른다.
 * - VR: 왼손/오른손 트리거 → 그 손의 MotionController(Grip) 위치에서 가장 가까운 대상
 *   (트리거 값은 템플릿의 IA_Hand_IndexCurl / IA_Shoot 액션에서 읽는다. OpenXR은 PMI_VRTemplate에
 *    등록된 입력만 받기 때문에 실행 중에 새로 만든 트리거 매핑은 동작하지 않음)
 * - 데스크톱 테스트: E 키 → 카메라가 바라보는 방향에서 가장 가까운 대상
 */
UCLASS()
class PCS_VR_API UVRInteractionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UVRInteractionSubsystem, STATGROUP_Tickables);
	}

	void Register(UPuzzleInteractComponent* Comp);
	void Unregister(UPuzzleInteractComponent* Comp);

	/** 데스크톱 E 키로 누를 수 있는 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "Interact")
	float DesktopReach = 350.f;

private:
	void SetupInput(APlayerController* PC);
	void OnLeftTrigger();
	void OnRightTrigger();
	void OnDesktopInteract();
	void InteractWithHand(const TCHAR* GripName);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Context;

	float ReadTrigger(const TArray<TObjectPtr<UInputAction>>& Actions) const;
	void UpdateTriggers();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> LeftTriggerActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> RightTriggerActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DesktopAction;

	TWeakObjectPtr<UEnhancedPlayerInput> PlayerInput;
	TArray<TWeakObjectPtr<UPuzzleInteractComponent>> Interactables;
	bool bInputReady = false;
	bool bLeftDown = false;
	bool bRightDown = false;
};
