#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObservationStageDirector.generated.h"

class APuzzleBlueprintSlot;
class APuzzleDoor;
class APuzzleKeypad;
class APuzzleSign;
class APuzzleViewPoint;

/**
 * 3스테이지(L_03_Observation, 관측실) 진행 담당. 노션 "3스테이지 퍼즐 설계" 순서:
 * 3-1) 발자국과 페인트: 발자국 위에서 셔터 쪽을 보면 "코어를 보지 마 / 1114" → 키패드 1114 → 셔터 열림
 * 3-2) 파이프로 만드는 원: 눈금 선 교차점에서 보면 천장 파이프 5개가 원 → 출구 문 잠금 1 해제
 * 3-3) 도면의 빠진 조각: 도면 조각을 원근법으로 키워 1m 빈 칸에 끼움 → 잠금 2 해제
 * 잠금 2개가 모두 풀리면 출구 문 열림 + 지도 메모. 3-2와 3-3은 순서 상관없음.
 * 막힘 방지: 3-1 60초 / 3-2 90초 / 3-3 90초 진행 없으면 지도 메모 힌트
 */
UCLASS()
class PCS_VR_API AObservationStageDirector : public AActor
{
	GENERATED_BODY()

public:
	AObservationStageDirector();

	// ---- 레벨에 놓인 부품 ----
	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TObjectPtr<APuzzleViewPoint> PaintView;

	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TObjectPtr<APuzzleKeypad> Keypad;

	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TObjectPtr<APuzzleDoor> Shutter;

	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TObjectPtr<APuzzleViewPoint> PipeView;

	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TObjectPtr<APuzzleBlueprintSlot> Slot;

	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TObjectPtr<APuzzleDoor> ExitDoor;

	/** 출구 문 위 잠금 표시 2개 (0 = 원/파이프, 1 = 도면) */
	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TArray<TObjectPtr<APuzzleSign>> LockLamps;

	/** 출구 문 위 빈 원 모양 홈 (3-2 힌트, 맞추면 불이 들어옴) */
	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TObjectPtr<APuzzleSign> RingGroove;

	/** 3-3 도면이 완성되면 빛나는 코어 내부 구조 그림 */
	UPROPERTY(EditAnywhere, Category = "Stage3|Refs")
	TObjectPtr<APuzzleSign> CoreDiagram;

	// ---- 안내 / 막힘 방지 ----
	UPROPERTY(EditAnywhere, Category = "Stage3|Hints")
	float PaintHintDelay = 60.f;

	UPROPERTY(EditAnywhere, Category = "Stage3|Hints")
	float PipeHintDelay = 90.f;

	UPROPERTY(EditAnywhere, Category = "Stage3|Hints")
	float SlotHintDelay = 90.f;

	UPROPERTY(EditAnywhere, Category = "Stage3|Hints", meta = (MultiLine = "true"))
	FText PaintHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage3|Hints", meta = (MultiLine = "true"))
	FText PipeHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage3|Hints", meta = (MultiLine = "true"))
	FText SlotHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage3|Hints", meta = (MultiLine = "true"))
	FText ClearMemo;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleKeypadSolved();

	UFUNCTION()
	void HandlePipesSolved();

	UFUNCTION()
	void HandleSlotFilled();

	void SetLock(int32 Index, bool bOpen);
	void CheckClear();
	void ShowMemo(const FText& Memo, float Duration = 6.f);
	void HideMemo();

	UPROPERTY(Transient)
	TObjectPtr<APuzzleSign> MemoSign;

	FTimerHandle MemoHandle;
	float StageTime = 0.f;
	float InnerTime = 0.f;
	float SlotTime = 0.f;
	bool bShutterOpen = false;
	bool bPipesSolved = false;
	bool bSlotFilled = false;
	bool bPaintHintShown = false;
	bool bPipeHintShown = false;
	bool bSlotHintShown = false;
	bool bCleared = false;
};
