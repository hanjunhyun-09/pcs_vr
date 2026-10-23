#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleLiftPad.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * 높이가 다른 곳으로 옮겨 주는 바닥 발판 (노션 5스테이지: 2층 발코니는 계단 대신 텔레포트 발판으로 올라감, 멀미 방지).
 * 플레이어 머리(카메라)가 발판 위에 HoldTime초 머물면 화면이 잠깐 어두워지고, 머리가 Destination 바로 위에 오도록 옮긴다.
 * 높이는 발판 바닥 → Destination 높이 차이만큼 바뀐다. 액터 위치 = 발판 바닥 가운데.
 * 위아래 한 쌍으로 쓰고, Destination은 반대쪽 발판에서 조금 떨어진 곳에 둔다 (도착하자마자 다시 타지 않게).
 */
UCLASS()
class PCS_VR_API APuzzleLiftPad : public AActor
{
	GENERATED_BODY()

public:
	APuzzleLiftPad();

	/** 도착 위치 (월드 좌표, 바닥 높이) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiftPad")
	FVector Destination = FVector::ZeroVector;

	/** 발판 지름 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiftPad")
	float Diameter = 90.f;

	/** 발판 위에 이만큼 서 있어야 옮겨진다 (초). 그냥 지나갈 때 타지 않게 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiftPad", meta = (ClampMin = "0.0"))
	float HoldTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiftPad")
	FLinearColor GlowColor = FLinearColor(0.3f, 0.8f, 1.f);

	UPROPERTY(BlueprintAssignable, Category = "LiftPad")
	FPuzzleSimpleEvent OnLifted;

	/** 바로 옮긴다 (테스트용) */
	UFUNCTION(BlueprintCallable, Category = "LiftPad")
	void Lift();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void MovePlayer();

	UPROPERTY(VisibleAnywhere, Category = "LiftPad")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "LiftPad")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(VisibleAnywhere, Category = "LiftPad")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiscMID;

	FTimerHandle MoveHandle;
	float StandTime = 0.f;
	bool bMoving = false;
};
