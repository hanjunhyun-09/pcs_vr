#pragma once

#include "CoreMinimal.h"
#include "PuzzleGrabbable.h"
#include "PuzzleInteractComponent.h"
#include "PerspectiveCube.generated.h"

/**
 * 원근법 물체 (첫 버전). 손에서 놓는 순간, 눈에서 보이는 크기를 그대로 유지한 채
 * 시선 방향 끝(벽·바닥)까지 밀어 보내고 그 거리만큼 크기를 바꾼다.
 * → 멀리 있는 벽을 보며 놓으면 커지고, 가까운 바닥을 보며 놓으면 작아진다.
 * 1스테이지 엘리베이터 앞 큐브(맛보기)에서 쓰고, 2스테이지 BP_PerspectiveObject의 바탕이 된다.
 */
UCLASS()
class PCS_VR_API APerspectiveCube : public APuzzleGrabbable
{
	GENERATED_BODY()

public:
	APerspectiveCube();

	/** 처음 크기 대비 최소/최대 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "0.05"))
	float MinScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "0.1"))
	float MaxScale = 5.f;

	/** 처음 크기의 이 배율 이상이 되면 OnGrown (한 번만) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "1.0"))
	float GrownThreshold = 2.f;

	/** 시선 방향으로 찾아볼 최대 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective")
	float MaxProjectDistance = 3000.f;

	UPROPERTY(BlueprintAssignable, Category = "Perspective")
	FPuzzleSimpleEvent OnGrown;

	UFUNCTION(BlueprintPure, Category = "Perspective")
	float GetScaleRatio() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnReleased() override;

private:
	void ProjectFromView();

	FVector InitialScale = FVector::OneVector;
	bool bGrownFired = false;
};
