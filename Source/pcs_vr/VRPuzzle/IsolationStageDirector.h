#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IsolationStageDirector.generated.h"

class APerspectiveCube;
class APuzzleButton;
class APuzzleCardReader;
class APuzzleDoor;
class APuzzleElevator;
class APuzzleLever;
class APuzzleSign;
class APuzzleTeleportPad;

/**
 * 1스테이지(L_01_Isolation) 진행 담당. 노션 "1스테이지 퍼즐 설계" 순서 그대로:
 * 0) 출입문 카드 리더기에 내 카드 → 출입문 열림
 * 1) 격리실에 들어오면 뒤의 문이 닫힘 → 발판 ◎ 까지 텔레포트
 * 2) 배전함 레버 → 조명 깜빡이며 켜짐, 일정표·명단·이름표 보임, 잠긴 문 리더기 전원
 * 3) 내 카드는 거부 / 바닥의 관리자 카드 → 봉쇄 해제 버튼 튀어나옴 → 버튼 → 문 열림
 *    (30초 안에 못 찾으면 지도 메모 힌트)
 * 4) 엘리베이터 앞 큐브를 멀리 보며 놓아 키우면 엘리베이터 도착 (안 키워도 40초 뒤 도착)
 */
UCLASS()
class PCS_VR_API AIsolationStageDirector : public AActor
{
	GENERATED_BODY()

public:
	AIsolationStageDirector();

	// ---- 레벨에 놓인 부품 ----
	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleCardReader> EntranceReader;

	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleDoor> EntranceDoor;

	/** 격리실로 들어오는 문 (들어오면 닫히고 잠김) */
	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleDoor> RoomEntryDoor;

	/** 이 액터의 박스(Bounds) 안에 플레이어 머리가 들어오면 "격리실 입장" */
	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<AActor> RoomEntryVolume;

	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleTeleportPad> Pad;

	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleLever> Lever;

	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleCardReader> ExitReader;

	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleButton> ExitButton;

	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleDoor> ExitDoor;

	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APerspectiveCube> Cube;

	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TObjectPtr<APuzzleElevator> Elevator;

	/** 레버를 내리면 켜지는 방 조명 (레벨에 넣어 둔 밝기가 켜졌을 때 밝기) */
	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TArray<TObjectPtr<AActor>> RoomLights;

	/** 레버를 내리면 꺼지는 비상등 */
	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TArray<TObjectPtr<AActor>> EmergencyLights;

	/** 레버를 내리면 보이기 시작하는 글자판 (일정표, 명단, 이름표) */
	UPROPERTY(EditAnywhere, Category = "Stage1|Refs")
	TArray<TObjectPtr<APuzzleSign>> RevealOnPower;

	// ---- 시간 / 대사 ----
	UPROPERTY(EditAnywhere, Category = "Stage1|Hints")
	float CardHintDelay = 30.f;

	UPROPERTY(EditAnywhere, Category = "Stage1|Hints")
	float ElevatorFallbackDelay = 40.f;

	UPROPERTY(EditAnywhere, Category = "Stage1|Hints", meta = (MultiLine = "true"))
	FText CardHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage1|Hints", meta = (MultiLine = "true"))
	FText CubeGrownMemo;

	/** 지도 메모를 눈앞에 띄운다 (손목 지도 위젯이 생기기 전 임시 방식) */
	UFUNCTION(BlueprintCallable, Category = "Stage1")
	void ShowMemo(const FText& Memo, float Duration = 6.f);

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void HandleEntranceAccepted();
	UFUNCTION() void HandlePadReached();
	UFUNCTION() void HandleLeverPulled();
	UFUNCTION() void HandleExitAccepted();
	UFUNCTION() void HandleExitDenied();
	UFUNCTION() void HandleExitButton();
	UFUNCTION() void HandleCubeGrown();

	void FlickerStep();
	void SetRoomLights(bool bOn);
	void ShowCardHint();
	void ElevatorFallback();
	void HideMemo();

	UPROPERTY(Transient)
	TObjectPtr<APuzzleSign> MemoSign;

	TMap<TWeakObjectPtr<AActor>, float> RoomLightIntensity;
	bool bEnteredRoom = false;
	int32 FlickerCount = 0;

	FTimerHandle FlickerHandle;
	FTimerHandle CardHintHandle;
	FTimerHandle ElevatorHandle;
	FTimerHandle MemoHandle;
};
