#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleCoreSocket.generated.h"

class APuzzleGrabbable;
class USphereComponent;

/**
 * 중앙 코어 받침대의 부품 슬롯 (노션 5스테이지: ▲ 부품 / ■ 부품은 잡아서 슬롯에 끼우기).
 * RequiredTag가 붙은 잡을 수 있는 물건(APuzzleGrabbable)이 손에서 놓인 채 감지 구 안에 들어오면
 * 슬롯에 딱 붙이고(물리 끔, 다시 잡히지 않게) OnFilled.
 * 원근법으로 크기를 맞춰야 하는 ● 코어 축은 APuzzleBlueprintSlot을 쓴다.
 * 액터 위치 = 슬롯 면 가운데, 앞(+X) = 코어 바깥쪽.
 */
UCLASS()
class PCS_VR_API APuzzleCoreSocket : public AActor
{
	GENERATED_BODY()

public:
	APuzzleCoreSocket();

	/** 이 Tag가 붙은 물건만 받는다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FName RequiredTag;

	/** 감지 반지름 (cm). 손에서 놓기만 하면 들어가게 넉넉히 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	float Radius = 35.f;

	/** 끼운 부품의 가운데가 슬롯 면에서 앞으로 나오는 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	float SnapOffset = 10.f;

	UPROPERTY(BlueprintAssignable, Category = "Socket")
	FPuzzleSimpleEvent OnFilled;

	UFUNCTION(BlueprintPure, Category = "Socket")
	bool IsFilled() const { return bFilled; }

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void SnapPart(APuzzleGrabbable* Part);

	UPROPERTY(VisibleAnywhere, Category = "Socket")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Socket")
	TObjectPtr<USphereComponent> Sensor;

	bool bFilled = false;
};
