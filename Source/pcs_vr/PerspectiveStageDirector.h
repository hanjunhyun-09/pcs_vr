#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerspectiveStageDirector.generated.h"

class APerspectiveCube;
class APuzzleButton;
class APuzzleDoor;
class APuzzlePressurePlate;
class APuzzleSign;

/**
 * 2스테이지(L_02_Perspective, 원근 실험실) 진행 담당. 노션 "2스테이지 퍼즐 설계" 순서:
 * 2-1) 높은 선반 (키우기): 큐브를 키워 밟고 선반 위로 → 통로 전원 스위치 → 통로 불이 켜지며 거대 상자가 드러남
 *      (불이 켜지기 전엔 거대 상자를 레이로 잡을 수 없음)
 * 2-2) 거대 상자 (줄이기): 멀리서 레이로 잡아 끌어오면 작아짐 → 통로가 열림
 * 2-3) 압력판 (둘 다): 줄인 상자를 난간에서 압력판을 보며 놓으면 커져서 눌림 → 출구 문 열림 + 지도 메모
 * 막힘 방지: 2-1 60초 / 2-2 상자 앞 40초 / 2-3 60초 진행 없으면 지도 메모 힌트
 */
UCLASS()
class PCS_VR_API APerspectiveStageDirector : public AActor
{
	GENERATED_BODY()

public:
	APerspectiveStageDirector();

	// ---- 레벨에 놓인 부품 ----
	UPROPERTY(EditAnywhere, Category = "Stage2|Refs")
	TObjectPtr<APuzzleButton> ShelfSwitch;

	/** 통로 B 조명 (스위치를 누르기 전엔 꺼져 있음) */
	UPROPERTY(EditAnywhere, Category = "Stage2|Refs")
	TArray<TObjectPtr<AActor>> CorridorLights;

	UPROPERTY(EditAnywhere, Category = "Stage2|Refs")
	TObjectPtr<APerspectiveCube> GiantBox;

	UPROPERTY(EditAnywhere, Category = "Stage2|Refs")
	TObjectPtr<APuzzlePressurePlate> Plate;

	UPROPERTY(EditAnywhere, Category = "Stage2|Refs")
	TObjectPtr<APuzzleDoor> ExitDoor;

	// ---- 안내 / 막힘 방지 ----
	/** 엘리베이터 문이 열린 뒤 목표를 알려 주는 메모 (도착 후 초) */
	UPROPERTY(EditAnywhere, Category = "Stage2|Hints")
	float ArrivalMemoDelay = 3.f;

	/** 2-1: 이 시간 안에 큐브를 1m 이상 못 키우면 "멀리 보고 놓으면 커져" */
	UPROPERTY(EditAnywhere, Category = "Stage2|Hints")
	float ShelfHintDelay = 60.f;

	/** 2-1: 큐브를 1m 이상 키운 뒤 이 시간 동안 스위치 근처에 못 가면 "밟고 올라가" */
	UPROPERTY(EditAnywhere, Category = "Stage2|Hints")
	float ClimbHintDelay = 30.f;

	/** 거대 상자 근처(BoxHintRadius 안)에 이만큼 머물면 힌트 */
	UPROPERTY(EditAnywhere, Category = "Stage2|Hints")
	float BoxHintDelay = 40.f;

	UPROPERTY(EditAnywhere, Category = "Stage2|Hints")
	float BoxHintRadius = 400.f;

	UPROPERTY(EditAnywhere, Category = "Stage2|Hints")
	float PlateHintDelay = 60.f;

	/** 거대 상자가 기준 크기의 이 배율 이하가 되면 "줄였다"로 본다 */
	UPROPERTY(EditAnywhere, Category = "Stage2|Hints")
	float BoxShrunkRatio = 2.f;

	UPROPERTY(EditAnywhere, Category = "Stage2|Hints", meta = (MultiLine = "true"))
	FText ArrivalMemo;

	UPROPERTY(EditAnywhere, Category = "Stage2|Hints", meta = (MultiLine = "true"))
	FText ShelfHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage2|Hints", meta = (MultiLine = "true"))
	FText ClimbHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage2|Hints", meta = (MultiLine = "true"))
	FText BoxHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage2|Hints", meta = (MultiLine = "true"))
	FText PlateHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage2|Hints", meta = (MultiLine = "true"))
	FText ClearMemo;

	/** 통로 스위치 전에 거대 상자를 잡으려 했을 때 */
	UPROPERTY(EditAnywhere, Category = "Stage2|Hints", meta = (MultiLine = "true"))
	FText LockedBoxMemo;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleShelfSwitch();

	UFUNCTION()
	void HandlePlatePressed();

	UFUNCTION()
	void HandleBoxRefused();

	void ShowMemo(const FText& Memo, float Duration = 6.f);
	void HideMemo();
	void ShowArrivalMemo();
	/** 2-1 진행 살피기: 큐브를 키웠는지, 스위치 근처에 갔는지에 따라 힌트 */
	void UpdateShelfHints(float DeltaTime);
	void ShowPlateHint();
	void SetCorridorLights(bool bOn);

	UPROPERTY(Transient)
	TObjectPtr<APuzzleSign> MemoSign;

	TMap<TWeakObjectPtr<AActor>, float> CorridorLightIntensity;
	FTimerHandle ArrivalHandle;
	float StageTime = 0.f;
	float GrownCubeTime = 0.f;
	bool bCubeGrown = false;
	bool bShelfHintShown = false;
	bool bClimbHintShown = false;
	FTimerHandle PlateHintHandle;
	FTimerHandle MemoHandle;
	bool bSwitchPressed = false;
	bool bBoxShrunk = false;
	bool bBoxHintShown = false;
	bool bCleared = false;
	float NearBoxTime = 0.f;
	double LastRefusedMemoTime = -1000.0;
};
