#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleLever.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * 배전함 레버. 손잡이에 손을 대고 트리거 → 레버가 위에서 아래로 내려가고 OnActivated.
 * 액터의 앞(+X)이 벽 바깥(플레이어 쪽)을 보게 놓는다. 한 번만 동작한다.
 */
UCLASS()
class PCS_VR_API APuzzleLever : public AActor
{
	GENERATED_BODY()

public:
	APuzzleLever();

	UPROPERTY(BlueprintAssignable, Category = "Lever")
	FPuzzleSimpleEvent OnActivated;

	/** 내려가는 데 걸리는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever", meta = (ClampMin = "0.05"))
	float PullTime = 0.35f;

	UFUNCTION(BlueprintCallable, Category = "Lever")
	void Pull();

	bool IsPulled() const { return bPulled; }

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleInteract(AActor* InteractInstigator);

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<UStaticMeshComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<UStaticMeshComponent> Handle;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<UStaticMeshComponent> Knob;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<UStaticMeshComponent> Indicator;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<UPointLightComponent> IndicatorLight;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<UPuzzleInteractComponent> Interact;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IndicatorMID;

	bool bPulled = false;
	bool bAnimating = false;
	float AnimAlpha = 0.f;

	static constexpr float UpPitch = -35.f;
	static constexpr float DownPitch = -145.f;
};
