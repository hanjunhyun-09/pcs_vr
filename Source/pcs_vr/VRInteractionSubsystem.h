#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VRInteractionSubsystem.generated.h"

class APerspectiveCube;
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
 *
 * 원근 큐브 레이 잡기 (멀리서 잡으면 작아짐):
 * - VR: 컨트롤러(Aim) 레이가 원근 큐브를 겨누면 큐브가 밝아지고, 그립을 쥐면 손으로 끌려온다.
 *   손 바로 옆(MinRayGrabDistance 이내)의 큐브는 템플릿의 일반 잡기에 맡긴다.
 *   그립 값은 템플릿의 IA_Grab_* / IA_Hand_Grasp_* 액션에서 읽는다 (트리거와 같은 이유).
 * - 데스크톱 테스트: R 키 → 시선 레이로 잡기 / 다시 R → 놓기
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

	/** 레이로 잡을 수 있는 최대 거리 (cm). 노션 원근법 규칙: 최대 12m */
	UPROPERTY(EditAnywhere, Category = "Interact|RayGrab")
	float RayGrabDistance = 1200.f;

	/** 손에서 이보다 가까운 큐브는 레이로 잡지 않는다 (cm) */
	UPROPERTY(EditAnywhere, Category = "Interact|RayGrab")
	float MinRayGrabDistance = 40.f;

	/** 트리거를 끝까지 당기고 있는지 (키패드처럼 직접 손끝을 보는 부품이 쓴다) */
	bool IsTriggerDown(bool bLeft) const { return bLeft ? bLeftDown : bRightDown; }

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

	// 원근 큐브 레이 잡기
	struct FRayHand
	{
		const TCHAR* AimName;
		const TCHAR* GripName;
		bool bGripDown = false;
		TWeakObjectPtr<APerspectiveCube> Held;
		TWeakObjectPtr<APerspectiveCube> Aimed;
	};
	void UpdateRayGrab();
	void UpdateRayHand(FRayHand& Hand, float GripValue);
	/** bIncludeLocked면 레이 잡기가 막힌 큐브도 돌려준다 (잡으려 했다는 알림용) */
	APerspectiveCube* TraceCube(const FVector& Start, const FVector& Dir, const FVector& MinDistFrom, bool bIncludeLocked = false) const;
	void OnDesktopRayGrab();

	FRayHand LeftRay{ TEXT("MotionControllerLeftAim"), TEXT("MotionControllerLeftGrip") };
	FRayHand RightRay{ TEXT("MotionControllerRightAim"), TEXT("MotionControllerRightGrip") };
	TWeakObjectPtr<APerspectiveCube> DesktopHeld;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> LeftGripActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> RightGripActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DesktopRayAction;

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
