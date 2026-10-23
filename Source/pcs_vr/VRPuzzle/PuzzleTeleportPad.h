#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleTeleportPad.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * 바닥의 빛나는 발판 ◎ (텔레포트 연습 목표).
 * 플레이어 머리(카메라)가 발판 위에 오면 꺼지고 OnReached.
 */
UCLASS()
class PCS_VR_API APuzzleTeleportPad : public AActor
{
	GENERATED_BODY()

public:
	APuzzleTeleportPad();

	UPROPERTY(BlueprintAssignable, Category = "Pad")
	FPuzzleSimpleEvent OnReached;

	/** 발판 지름 (cm). 노션: 80cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pad")
	float Diameter = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pad")
	FLinearColor GlowColor = FLinearColor(0.1f, 0.7f, 1.f);

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Pad")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(VisibleAnywhere, Category = "Pad")
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere, Category = "Pad")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiscMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMID;

	bool bReached = false;
};
