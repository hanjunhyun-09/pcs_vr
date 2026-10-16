#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleBlueprintSlot.generated.h"

class APerspectiveCube;
class UBoxComponent;

/**
 * 벽 도면의 빈 칸 (노션 3-3: 한 변 1m 홈).
 * 원근 조각(APerspectiveCube, RequiredTag)이 손에서 놓인 채 홈 앞 감지 영역에 들어오고,
 * 크기가 홈의 MinFit ~ MaxFit배(85% ~ 115%)면 홈에 딱 끼우고(위치·방향·크기 보정) OnFilled.
 * 너무 크거나 작으면 아무 일 없이 홈 앞에 떨어진다 → 다시 잡아서 시도.
 * 액터 위치 = 홈 가운데(벽 면), 앞(+X) = 방 쪽.
 */
UCLASS()
class PCS_VR_API APuzzleBlueprintSlot : public AActor
{
	GENERATED_BODY()

public:
	APuzzleBlueprintSlot();

	/** 홈 한 변 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	float SlotSize = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	float MinFit = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	float MaxFit = 1.15f;

	/** 홈 앞 감지 깊이 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	float SensorDepth = 60.f;

	/** 이 Tag가 붙은 원근 물체만 받는다 (None이면 아무 원근 물체나) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FName RequiredTag = TEXT("BlueprintPiece");

	UPROPERTY(BlueprintAssignable, Category = "Slot")
	FPuzzleSimpleEvent OnFilled;

	UFUNCTION(BlueprintPure, Category = "Slot")
	bool IsFilled() const { return bFilled; }

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void SnapPiece(APerspectiveCube* Piece);

	UPROPERTY(VisibleAnywhere, Category = "Slot")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Slot")
	TObjectPtr<UBoxComponent> Sensor;

	bool bFilled = false;
	double LastRejectLogTime = -1000.0;
};
