#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VRSmoothLocomotionSubsystem.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * VR 템플릿 폰(VRPawn)을 왼쪽 조이스틱(위/아래)으로 부드럽게 이동시킨다.
 * - 이동 방향: 헤드셋이 바라보는 방향(수평)
 * - 오른쪽 조이스틱 앞으로 밀기(텔레포트)는 입력을 가로채서 막는다
 * - 이동 앞쪽에 벽이 있으면 멈춘다
 * 게임 월드에서 자동으로 생성되며, VRPawn이 없으면 아무 일도 하지 않는다.
 */
UCLASS()
class PCS_VR_API UVRSmoothLocomotionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UVRSmoothLocomotionSubsystem, STATGROUP_Tickables);
	}

	/** 이동 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float MoveSpeed = 140.f;

	/** 조이스틱 데드존 (0~1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float DeadZone = 0.25f;

	/** 벽 충돌 검사 반지름 (cm). 0이면 충돌 검사를 하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float CollisionRadius = 0.f;

private:
	void SetupInput(class APlayerController* PC);
	void OnMoveAxis(const FInputActionValue& Value);
	void OnMoveEnd(const FInputActionValue& Value);
	float ReadRawStick(class APlayerController* PC) const;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LocomotionContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BlockTeleportAction;

	bool bInputReady = false;
	float EnhancedAxis = 0.f;
	double LastLogTime = 0.0;
	double LastBlockLogTime = 0.0;
};
