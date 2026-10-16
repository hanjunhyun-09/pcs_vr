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
	float CollisionRadius = 20.f;

	/** true면 무릎 높이 소품(침대, 책상, 큰 큐브)에도 막힌다. 스테이지 진행 담당이 켠다 (인트로는 끔) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	bool bBlockLowObstacles = false;

	/**
	 * true면 오르기/내려오기를 쓴다. 스테이지 진행 담당이 켠다 (L_02).
	 * - 오르기: 큰 큐브·선반 앞에 막힌 채로 조이스틱을 ClimbHoldTime 동안 앞으로 밀면 그 윗면(NavMesh가 있는 곳)으로 올라간다
	 * - 내려오기: 발밑(머리 아래)이 비면 아래 바닥(NavMesh가 있는 곳)으로 내려온다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Climb")
	bool bAllowClimb = false;

	/** 한 번에 오를 수 있는 최대 높이 (cm). L_02: 바닥 → 큐브(1~2m) → 선반(3m)이 이어지도록 200 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Climb")
	float MaxClimbHeight = 200.f;

	/** 이보다 낮은 턱은 오르기로 치지 않는다 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Climb")
	float MinClimbHeight = 20.f;

	/** 막힌 채로 앞으로 밀고 있어야 하는 시간 (초). 실수로 올라가지 않게 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Climb", meta = (ClampMin = "0.0"))
	float ClimbHoldTime = 0.3f;

private:
	void SetupInput(class APlayerController* PC);
	void OnMoveAxis(const FInputActionValue& Value);
	void OnMoveEnd(const FInputActionValue& Value);
	float ReadRawStick(class APlayerController* PC) const;
	void DisableTemplateTeleport(class APawn* Pawn);

	/** 플레이어를 실제로 막는 물체인지 (겹침 전용 영역, 손에 든 물건은 제외) */
	bool IsSolidForPawn(const class UPrimitiveComponent* Comp, const AActor* Actor, const class APawn* Pawn) const;
	/** (X, Y)에서 TopZ부터 BottomZ까지 아래로 훑어 처음 밟을 수 있는 면의 높이. Radius > 0이면 그만큼 여유를 두고 찾는다 */
	bool FindGround(const FVector2D& XY, float TopZ, float BottomZ, float Radius, const class APawn* Pawn, float& OutZ) const;
	bool HasNavMeshNear(const FVector& Point) const;
	/** 막힌 면(BlockHit) 너머 윗면으로 올라간다. 올라갔으면 true */
	bool TryClimb(class APawn* Pawn, const class UCameraComponent* Cam, const FVector& Dir, const FHitResult& BlockHit);
	/** 머리 아래 바닥이 낮아졌으면 내려온다 */
	void FollowGround(class APawn* Pawn, const class UCameraComponent* Cam);
	void FadeIn(class APawn* Pawn) const;

	float ClimbHeldTime = 0.f;

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
