#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleStageGate.generated.h"

class APuzzleDoor;

/**
 * 복도 끝 보안문을 지나 다음 스테이지로 (노션 맵 구조: "엘리베이터 또는 복도 끝 보안문").
 * 액터 위치 = 문 아래쪽 가운데, 앞(+X) = 문 너머(다음 스테이지 쪽).
 * - 잠겨 있지 않으면 플레이어가 OpenRadius 안에 오면 Door가 열린다
 * - 문 너머(로컬 X 20 ~ TriggerDepth, |Y| < TriggerHalfWidth)에 머리가 들어가면 화면이 어두워지고 NextLevel을 연다
 */
UCLASS()
class PCS_VR_API APuzzleStageGate : public AActor
{
	GENERATED_BODY()

public:
	APuzzleStageGate();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	FName NextLevel = TEXT("/Game/VRPuzzle/Maps/L_03_Observation");

	/** 열고 닫을 문 (없으면 트리거만) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	TObjectPtr<APuzzleDoor> Door;

	/** true면 문이 열리지 않고 넘어가지도 않는다 (다음 스테이지가 아직 없을 때) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	bool bLocked = false;

	/** 문에서 이 거리(cm) 안에 오면 문이 열린다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	float OpenRadius = 250.f;

	/** 문 너머 이만큼(cm) 안쪽까지가 넘어가는 영역 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	float TriggerDepth = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	float TriggerHalfWidth = 100.f;

	UFUNCTION(BlueprintCallable, Category = "Gate")
	void Unlock();

	virtual void Tick(float DeltaTime) override;

private:
	void OpenNextLevel();

	UPROPERTY(VisibleAnywhere, Category = "Gate")
	TObjectPtr<USceneComponent> Root;

	bool bLeaving = false;
	FTimerHandle LevelHandle;
};
