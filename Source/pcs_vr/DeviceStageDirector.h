#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeviceStageDirector.generated.h"

class APuzzleDoor;
class APuzzleElevator;
class APuzzleHandDevice;
class APuzzleSign;
class APuzzleToggleTarget;

/**
 * 4스테이지(L_04_Device, 장치 시험실) 진행 담당. 노션 "4스테이지 퍼즐 설계" 순서:
 * 시작) 거치대에 두 손을 넣으면 장치가 잠기고, 들어온 문이 닫히고, 방 불이 꺼진다
 * 퍼즐 1) 어둠 속 실험: 눌러보며 ● 조명 / ▲ 경고등 / ■ 모니터 / ◆ 환풍기를 알아낸다
 *        - 환풍기를 처음 켜면 책상 위 종이가 날아가며 찢어진 기호표가 드러난다
 *        - 조명 OFF + 경고등 ON일 때만 칠판의 숨은 글씨 "경고 → 빛 → 바람 → 화면"
 * 퍼즐 2) 해제 절차: 마지막 4번이 ▲ ● ◆ ■ → 장치가 풀림 (판정은 APuzzleHandDevice)
 * 성공) 장비가 모두 꺼졌다 조명만 켜지고, 중앙 코어행 엘리베이터가 열림 + 지도 메모
 * 막힘 방지: 60초 동안 입력 없음 / 장착 후 2분 동안 숨은 글씨를 못 찾음 → 지도 메모 힌트
 */
UCLASS()
class PCS_VR_API ADeviceStageDirector : public AActor
{
	GENERATED_BODY()

public:
	ADeviceStageDirector();

	// ---- 레벨에 놓인 부품 ----
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleHandDevice> Device;

	/** ● 천장 조명 */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleToggleTarget> RoomLight;

	/** ▲ 빨간 경고등 */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleToggleTarget> WarningLight;

	/** ■ 모니터 */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleToggleTarget> Monitor;

	/** ◆ 환풍기 */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleToggleTarget> Fan;

	/** 칠판의 보통 글씨 (조명이 켜져 있을 때만 보임) */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TArray<TObjectPtr<APuzzleSign>> BoardTexts;

	/** 칠판의 숨은 글씨 (조명 OFF + 경고등 ON) */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleSign> HiddenText;

	/** 종이 밑에 깔린 찢어진 기호표 (환풍기를 켜면 드러남) */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleSign> SymbolTable;

	/** 책상 위 종이들 (환풍기 바람에 날아감) */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TArray<TObjectPtr<AActor>> Papers;

	/** 모니터의 참가자 신호 상태 글자 (켜져 있는 동안 날짜가 늘어남) */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleSign> MonitorText;

	/** 장치를 차면 닫히는 문 (들어온 문) */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleDoor> EntryDoor;

	/** 성공하면 도착하는 중앙 코어행 엘리베이터 */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TObjectPtr<APuzzleElevator> ExitElevator;

	/** 피험자를 찍던 관찰 카메라의 빨간 녹화 표시등 (20년째 깜빡임, Statik의 '누가 보고 있다' 느낌) */
	UPROPERTY(EditAnywhere, Category = "Stage4|Refs")
	TArray<TObjectPtr<AActor>> RecLamps;

	// ---- 안내 / 막힘 방지 ----
	UPROPERTY(EditAnywhere, Category = "Stage4|Hints")
	float IdleHintDelay = 60.f;

	UPROPERTY(EditAnywhere, Category = "Stage4|Hints")
	float HiddenHintDelay = 120.f;

	UPROPERTY(EditAnywhere, Category = "Stage4|Hints", meta = (MultiLine = "true"))
	FText IdleHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage4|Hints", meta = (MultiLine = "true"))
	FText HiddenHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage4|Hints", meta = (MultiLine = "true"))
	FText ClearMemo;

	virtual void Tick(float DeltaTime) override;

	/** 헤드셋 없이 테스트 (콘솔 pcs.Stage4Test): 책상 앞에 서서 장착 → 숨은 글씨 → 환풍기 → 해제 절차를 차례로 실행하고 스크린샷을 남긴다 */
	void RunSelfTest();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleEquipped();

	UFUNCTION()
	void HandlePressed(int32 SymbolIndex);

	UFUNCTION()
	void HandleUnlocked();

	UFUNCTION()
	void HandleToggled(APuzzleToggleTarget* Target, bool bOn);

	void UpdateBoard();
	void BlowPapers();
	void UpdatePapers(float DeltaTime);
	void UpdateMonitorText();
	void RestoreRoomLight();
	void OpenExit();
	void ShowMemo(const FText& Memo, float Duration = 6.f);
	void HideMemo();
	TArray<APuzzleToggleTarget*> GetTargets() const;

	UPROPERTY(Transient)
	TObjectPtr<APuzzleSign> MemoSign;

	struct FFlyingPaper
	{
		TWeakObjectPtr<AActor> Actor;
		FVector Velocity;
		FVector SpinAxis;
		float SpinRate = 0.f;
	};
	TArray<FFlyingPaper> Flying;
	float FlyTime = 0.f;

	FTimerHandle MemoHandle;
	FTimerHandle ClearHandle;
	float IdleTime = 0.f;
	float EquippedTime = 0.f;
	float MonitorDays = 7305.f;
	float RecBlinkTime = 0.f;
	bool bEquipped = false;
	bool bPapersBlown = false;
	bool bHiddenSeen = false;
	bool bIdleHintShown = false;
	bool bHiddenHintShown = false;
	bool bCleared = false;
};
