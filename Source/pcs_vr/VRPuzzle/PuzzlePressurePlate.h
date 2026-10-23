#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzlePressurePlate.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * 바닥 압력판 (노션 2-3: 2m x 2m, "필요 무게 ≥ 3").
 * 무거운 원근 물체(IsHeavy: 기준 크기의 HeavyRatio배 이상)가 손에서 놓인 채 올라가면 눌리고 OnPressed (한 번만, 계속 눌린 상태).
 * 가벼운 물체가 올라가면 판이 살짝만 내려갔다가 다시 올라온다.
 * 액터 위치 = 판 바닥 가운데.
 */
UCLASS()
class PCS_VR_API APuzzlePressurePlate : public AActor
{
	GENERATED_BODY()

public:
	APuzzlePressurePlate();

	UPROPERTY(BlueprintAssignable, Category = "PressurePlate")
	FPuzzleSimpleEvent OnPressed;

	/** 판 크기 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PressurePlate")
	FVector2D Size = FVector2D(200.f, 200.f);

	UFUNCTION(BlueprintPure, Category = "PressurePlate")
	bool IsPressed() const { return bPressed; }

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
	TObjectPtr<UStaticMeshComponent> Plate;

	UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
	TObjectPtr<UBoxComponent> Sensor;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlateMID;

	bool bPressed = false;
	float Depth = 0.f;          // 지금 눌린 깊이 (cm)
	float LightTouchTime = -1.f; // 가벼운 물체가 마지막으로 올라온 시간
};
