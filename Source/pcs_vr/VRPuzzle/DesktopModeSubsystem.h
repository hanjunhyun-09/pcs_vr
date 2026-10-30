#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DesktopModeSubsystem.generated.h"

class APlayerController;
class UActorComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * 데스크톱 테스트 모드 (노션 "개발 일정": 집에서 VR 없이 테스트하기).
 * 헤드셋이 없으면 템플릿 VRPawn을 그대로 쓰면서 마우스 + 키보드로 움직인다.
 * (폰을 바꾸지 않으므로 손 컴포넌트·카메라를 찾는 퍼즐 코드가 VR과 똑같이 동작한다)
 *
 * - 마우스: 둘러보기 (카메라 회전)
 * - WASD: 이동 (UVRSmoothLocomotionSubsystem으로 → 벽 막힘, 오르기, 내려오기 그대로)
 * - 왼쪽 클릭: 잡기 / 놓기
 *     원근 큐브 → 시선 레이 잡기 (R 키와 같음, 보이는 크기 유지)
 *     그 밖에 템플릿 GrabComponent가 있는 물건(카드 등) → 오른손으로 잡기
 *     앞에 잡을 게 없으면 → 허리에 걸린 카드를 꺼내 잡기
 * - 두 손은 카메라 앞 아래쪽에 둔다. 물건을 든 채 벽·리더기·슬롯을 보면 오른손이 그 면까지 뻗는다
 *   (그 상태로 놓으면 바라보는 곳에 놓임)
 * E / R / Q 키와 눈높이 보정은 UVRInteractionSubsystem이 맡는다.
 */
UCLASS()
class PCS_VR_API UDesktopModeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UDesktopModeSubsystem, STATGROUP_Tickables);
	}

	/** 헤드셋이 없어서 데스크톱 테스트 모드로 실행 중인지 */
	static bool IsDesktopMode();

	/** 마우스 1칸당 회전 (도) */
	UPROPERTY(EditAnywhere, Category = "Desktop")
	float MouseSensitivity = 0.4f;

	/** 왼쪽 클릭으로 손에 잡을 수 있는 거리 (cm, 눈에서) */
	UPROPERTY(EditAnywhere, Category = "Desktop")
	float HandReach = 160.f;

	/**
	 * 물건을 든 오른손이 바라보는 면까지 뻗는 최대 거리 (cm, 눈에서).
	 * VR에서는 팔을 뻗고 몸을 기울여 닿는 곳도 있어서 넉넉하게 (L_05 코어 슬롯: 받침대 때문에 눈에서 1m 넘게 떨어짐)
	 */
	UPROPERTY(EditAnywhere, Category = "Desktop")
	float HandExtendReach = 160.f;

	/** 카메라 기준 손 위치 (앞, 오른쪽, 위) cm. 왼손은 Y를 뒤집는다 */
	UPROPERTY(EditAnywhere, Category = "Desktop")
	FVector HandOffset = FVector(32.f, 20.f, -28.f);

private:
	void SetupInput(APlayerController* PC);
	void PlaceHands(APawn* Pawn, const USceneComponent* Cam);
	/** Target의 템플릿 GrabComponent를 오른손으로 잡는다. 잡았으면 true */
	bool GrabWithRightHand(APawn* Pawn, AActor* Target);
	void OnMove(const FInputActionValue& Value);
	void OnMoveEnd(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnGrab(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Context;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GrabAction;

	/** 오른손으로 잡고 있는 물건의 GrabComponent */
	TWeakObjectPtr<UActorComponent> HeldGrab;

	FVector2D MoveInput = FVector2D::ZeroVector;
	float LookYaw = 0.f;
	float LookPitch = 0.f;
	bool bInputReady = false;
};
