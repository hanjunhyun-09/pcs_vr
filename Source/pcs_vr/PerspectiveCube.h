#pragma once

#include "CoreMinimal.h"
#include "PuzzleGrabbable.h"
#include "PuzzleInteractComponent.h"
#include "PerspectiveCube.generated.h"

class USceneComponent;

/**
 * 원근법 물체 (파란 격자 무늬 = M_VRP_PerspectiveGrid). 눈에서 보이는 크기를 그대로 유지한 채 거리를 바꿔서 크기가 변한다.
 * - 커지기: 손에서 놓는 순간 시선 방향 끝(벽·바닥)까지 밀어 보내고 그 거리만큼 커진다.
 * - 작아지기: 멀리 있는 큐브를 컨트롤러 레이로 잡으면(그립) 손으로 끌려오면서 그 거리만큼 작아진다.
 *   레이 잡기 입력은 UVRInteractionSubsystem이 처리하고 BeginRayGrab / EndRayGrab을 부른다.
 * 1스테이지 엘리베이터 앞 큐브(맛보기)에서 쓰고, 2스테이지 BP_PerspectiveObject의 바탕이 된다.
 */
UCLASS()
class PCS_VR_API APerspectiveCube : public APuzzleGrabbable
{
	GENERATED_BODY()

public:
	APerspectiveCube();

	/** 기준 크기 대비 최소/최대 배율 (노션 원근법 규칙 기본값 0.25 ~ 8배) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "0.05"))
	float MinScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "0.1"))
	float MaxScale = 8.f;

	/**
	 * 배치된 크기가 기준 크기(배율 1.0)의 몇 배인지. 보통 1.
	 * 크게 시작하는 물체에 쓴다 (2스테이지 거대 상자: 기준 30cm, 배치 2.5m → 8.33)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "0.05"))
	float StartRatio = 1.f;

	/** 기준 크기 대비 이 배율 이상이면 무거움 → 압력판을 누를 수 있다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "1.0"))
	float HeavyRatio = 3.f;

	/** 한 변이 이보다 크면 손으로 직접 잡을 수 없다 (레이로만). cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective")
	float MaxHandGrabSize = 100.f;

	/** false면 레이로 잡을 수 없다 (2스테이지: 통로 불이 켜지기 전의 거대 상자) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective|RayGrab")
	bool bRayGrabEnabled = true;

	/** true면 처음 레이로 잡힐 때까지 물리를 끄고 제자리에 고정 (다른 물체에 밀려 길이 열리지 않게) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective")
	bool bFrozenUntilGrabbed = false;

	/** 잡으려 했지만 안 됨 (손으로 잡기엔 너무 큼 / 레이 잡기가 아직 막힘) */
	UPROPERTY(BlueprintAssignable, Category = "Perspective")
	FPuzzleSimpleEvent OnGrabRefused;

	/** 레이 잡기가 막혀 있을 때 레이로 잡으려 하면 UVRInteractionSubsystem이 부른다 */
	void NotifyRayGrabRefused();

	/** 기준 크기의 이 배율 이상이 되면 OnGrown (한 번만) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective", meta = (ClampMin = "1.0"))
	float GrownThreshold = 2.f;

	/** 시선 방향으로 찾아볼 최대 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective")
	float MaxProjectDistance = 3000.f;

	/** 레이로 잡았을 때 손까지 끌려오는 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perspective|RayGrab", meta = (ClampMin = "0.05"))
	float RayPullTime = 0.35f;

	UPROPERTY(BlueprintAssignable, Category = "Perspective")
	FPuzzleSimpleEvent OnGrown;

	/** 기준 크기 대비 지금 배율 */
	UFUNCTION(BlueprintPure, Category = "Perspective")
	float GetScaleRatio() const;

	UFUNCTION(BlueprintPure, Category = "Perspective")
	bool IsHeavy() const { return GetScaleRatio() >= HeavyRatio - KINDA_SMALL_NUMBER; }

	/** 지금 가장 긴 변의 길이 (cm) */
	UFUNCTION(BlueprintPure, Category = "Perspective")
	float GetSizeCm() const;

	virtual bool IsHeld() const override;
	virtual void Tick(float DeltaTime) override;

	/** 레이로 잡을 수 있는 상태인지 (손에 들려 있지 않고, 레이 잡기가 허용됨) */
	bool CanRayGrab() const { return bRayGrabEnabled && !IsHeld(); }

	/** Hand(컨트롤러 그립 / 데스크톱 카메라)의 HoldOffset 위치로 끌어온다. 끌려오는 동안 보이는 크기 유지 */
	UFUNCTION(BlueprintCallable, Category = "Perspective|RayGrab")
	void BeginRayGrab(USceneComponent* Hand, FVector HoldOffset);

	/** 그립을 놓음 → 일반 놓기와 같이 시선 방향으로 투사된다 */
	UFUNCTION(BlueprintCallable, Category = "Perspective|RayGrab")
	void EndRayGrab();

	UFUNCTION(BlueprintPure, Category = "Perspective|RayGrab")
	bool IsRayHeld() const { return bRayHeld; }

	/** 레이가 겨누고 있을 때 밝게 */
	void SetAimHighlight(bool bOn);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void OnGrabbed() override;
	virtual void OnReleased() override;

private:
	void ProjectFromView();
	void UpdateRayPull(float DeltaTime);
	/** 한 변 1m 이상이고 손에 없으면 윗면이 텔레포트 가능한 바닥(NavMesh)이 된다 */
	void UpdateNavigation();

	/** 너무 커서 손으로 못 잡을 때 강제로 놓은 경우: 다음 놓기에서 투사하지 않음 */
	bool bSkipNextProjection = false;

	/** bFrozenUntilGrabbed로 고정된 상태 (첫 레이 잡기에서 풀림) */
	bool bFrozen = false;

	FVector InitialScale = FVector::OneVector;
	bool bGrownFired = false;

	// 레이 잡기 상태
	TWeakObjectPtr<USceneComponent> RayHand;
	FVector RayHoldOffset = FVector::ZeroVector;
	bool bRayHeld = false;
	bool bPulling = false;
	float PullElapsed = 0.f;
	FVector PullStartLoc = FVector::ZeroVector;
	FVector PullStartScale = FVector::OneVector;
	float PullStartEyeDist = 1.f;
	bool bAimHighlight = false;
};
