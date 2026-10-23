#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoreStageDirector.generated.h"

class APuzzleBlueprintSlot;
class APuzzleCoreSocket;
class APuzzleDoor;
class APuzzleElevator;
class APuzzleGrabbable;
class APuzzleHandDevice;
class APuzzleRecorder;
class APuzzleSign;
class APuzzleToggleTarget;
class APuzzleRingDisc;
class UMaterialInstanceDynamic;
class UPointLightComponent;

/**
 * 5스테이지(L_05_Core, 중앙 코어) 진행 담당. 노션 "5스테이지 퍼즐 설계" 순서:
 * 원근법 구역) 3m 코어 축을 줄여 좁은문(60cm)을 지나 → 코어 ● 슬롯(규격 1.2m)에 맞게 다시 키워 끼움 (판정은 APuzzleBlueprintSlot)
 * 시점 구역) 2층 발코니 관측 원판 ① ◆● / ② ▲■ 의 고리를 돌려 그림을 둘 다 이으면 → 발코니 보관함이 열려 ▲ 부품 + 코드 ◆●▲■ 기록
 * 장치 구역) 4스테이지 손 장치로 코어 제어판 램프에 코드 입력 (틀리면 램프가 모두 꺼짐) → 장치가 풀리고 ■ 부품이 나옴
 * 코어 정지) 부품을 끼울 때마다 음성 기록 #2 / #3 / #4, 세 개가 모이면 맥박이 느려지며 나연의 마지막 음성
 *          → 마지막 음성이 끝난 뒤에 빛이 꺼지고 출구 엘리베이터가 열림
 * 막힘 방지: 코어 축을 들고 좁은문 앞 40초 / 발코니에서 90초 진행 없음 / 코드 없이 장치 구역 진입 → 지도 메모
 */
UCLASS()
class PCS_VR_API ACoreStageDirector : public AActor
{
	GENERATED_BODY()

public:
	ACoreStageDirector();

	// ---- 코어 ----
	/** 맥박처럼 빛나는 코어 (StaticMeshActor, M_VRP_Solid) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Core")
	TObjectPtr<AActor> CoreMesh;

	/** 코어 빛 (맥박에 맞춰 밝기가 바뀌고, 끝나면 꺼짐) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Core")
	TArray<TObjectPtr<AActor>> CoreLights;

	UPROPERTY(EditAnywhere, Category = "Stage5|Core")
	FLinearColor CoreColor = FLinearColor(0.3f, 0.55f, 1.f);

	/** ● 코어 축 슬롯 (원근법) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Core")
	TObjectPtr<APuzzleBlueprintSlot> AxisSlot;

	/** ▲ 부품 슬롯 */
	UPROPERTY(EditAnywhere, Category = "Stage5|Core")
	TObjectPtr<APuzzleCoreSocket> TriangleSocket;

	/** ■ 부품 슬롯 */
	UPROPERTY(EditAnywhere, Category = "Stage5|Core")
	TObjectPtr<APuzzleCoreSocket> SquareSocket;

	/** 슬롯 옆 표시 램프 (●▲■ 순서, 끼우면 켜짐. APuzzleSign) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Core")
	TArray<TObjectPtr<APuzzleSign>> SlotLabels;

	// ---- 원근법 구역 ----
	/** 이 태그가 붙은 원근 물체 = 코어 축 */
	UPROPERTY(EditAnywhere, Category = "Stage5|Perspective")
	FName AxisTag = TEXT("CoreAxis");

	/** 좁은문 위치 (막힘 방지: 코어 축을 들고 이 근처에 머무는 시간) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Perspective")
	FVector NarrowDoorLocation = FVector::ZeroVector;

	// ---- 시점 구역 ----
	/** 관측 원판 ① (◆ ●) */
	UPROPERTY(EditAnywhere, Category = "Stage5|View")
	TObjectPtr<APuzzleRingDisc> DiscA;

	/** 관측 원판 ② (▲ ■) */
	UPROPERTY(EditAnywhere, Category = "Stage5|View")
	TObjectPtr<APuzzleRingDisc> DiscB;

	/** 발코니 보관함 문 (원판 둘 다 맞추면 열림) */
	UPROPERTY(EditAnywhere, Category = "Stage5|View")
	TObjectPtr<APuzzleDoor> LockerDoor;

	/** 보관함 안 ▲ 부품 (열리기 전엔 숨김) */
	UPROPERTY(EditAnywhere, Category = "Stage5|View")
	TObjectPtr<APuzzleGrabbable> TrianglePart;

	/** 발코니 바닥 높이 (월드 Z). 이보다 머리가 높으면 발코니에 있는 것 */
	UPROPERTY(EditAnywhere, Category = "Stage5|View")
	float BalconyFloorZ = 400.f;

	// ---- 장치 구역 ----
	UPROPERTY(EditAnywhere, Category = "Stage5|Device")
	TObjectPtr<APuzzleHandDevice> Device;

	/** 코어 제어판 램프 4개 (●▲■◆, 장치의 Targets와 같은 순서) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Device")
	TArray<TObjectPtr<APuzzleToggleTarget>> PanelLamps;

	/** 제어판 아래 ■ 부품 (장치가 풀리기 전엔 숨김) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Device")
	TObjectPtr<APuzzleGrabbable> SquarePart;

	/** 장치 구역 바닥 범위 (월드 좌표, 코드 없이 들어오면 지도 메모) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Device")
	FVector DeviceZoneMin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Stage5|Device")
	FVector DeviceZoneMax = FVector::ZeroVector;

	/** 지도에 자동 기록되는 코드 (장치 구역 거치대 옆 판에 표시) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Device")
	TObjectPtr<APuzzleSign> CodeBoard;

	// ---- 음성 기록 / 엔딩 ----
	/** #2 시연 직전 (● 코어 축) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Story")
	TObjectPtr<APuzzleRecorder> LogAxis;

	/** #3 사고 직후 (▲ 부품) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Story")
	TObjectPtr<APuzzleRecorder> LogTriangle;

	/** #4 사고 다음 날 (■ 부품) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Story")
	TObjectPtr<APuzzleRecorder> LogSquare;

	/** 나연의 마지막 음성 */
	UPROPERTY(EditAnywhere, Category = "Stage5|Story")
	TObjectPtr<APuzzleRecorder> LogFinal;

	UPROPERTY(EditAnywhere, Category = "Stage5|Story")
	TObjectPtr<APuzzleElevator> ExitElevator;

	/** 출구 앞 세진의 쪽지 (엔딩에 보임) */
	UPROPERTY(EditAnywhere, Category = "Stage5|Story")
	TObjectPtr<APuzzleSign> ExitNote;

	// ---- 안내 / 막힘 방지 ----
	UPROPERTY(EditAnywhere, Category = "Stage5|Hints")
	float NarrowDoorHintDelay = 40.f;

	UPROPERTY(EditAnywhere, Category = "Stage5|Hints")
	float BalconyHintDelay = 90.f;

	UPROPERTY(EditAnywhere, Category = "Stage5|Hints", meta = (MultiLine = "true"))
	FText NarrowDoorHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage5|Hints", meta = (MultiLine = "true"))
	FText BalconyHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage5|Hints", meta = (MultiLine = "true"))
	FText NoCodeHintMemo;

	UPROPERTY(EditAnywhere, Category = "Stage5|Hints", meta = (MultiLine = "true"))
	FText CodeRecordedMemo;

	virtual void Tick(float DeltaTime) override;

	/** 헤드셋 없이 테스트 (콘솔 pcs.Stage5Test): 세 구역을 차례로 풀고 엔딩까지 스크린샷을 남긴다 */
	void RunSelfTest();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleViewSolved();

	UFUNCTION()
	void HandlePressed(int32 SymbolIndex);

	UFUNCTION()
	void HandleUnlocked();

	UFUNCTION()
	void HandleAxisFilled();

	UFUNCTION()
	void HandleTriangleFilled();

	UFUNCTION()
	void HandleSquareFilled();

	void PartFilled(int32 Index, APuzzleRecorder* Log);
	void PlayFinal();
	void Finish();
	void RevealPart(APuzzleGrabbable* Part);
	void HidePart(APuzzleGrabbable* Part);
	void UpdateCore(float DeltaTime);
	void UpdateHints(float DeltaTime);
	void UpdateCodeBoard();
	int32 CountProgress() const;
	static float LogDuration(const APuzzleRecorder* Log);
	void ShowMemo(const FText& Memo, float Duration = 6.f);
	void HideMemo();

	UPROPERTY(Transient)
	TObjectPtr<APuzzleSign> MemoSign;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoreMID;

	TArray<TWeakObjectPtr<UPointLightComponent>> CoreLightComps;
	TArray<float> CoreLightBase;

	TArray<int32> Inputs;
	FTimerHandle MemoHandle;
	FTimerHandle StoryHandle;
	float PulsePhase = 0.f;
	float FadeOut = 1.f;
	float NarrowDoorTime = 0.f;
	float BalconyTime = 0.f;
	int32 FilledCount = 0;
	bool bFilled[3] = { false, false, false };
	bool bHasCode = false;
	bool bNarrowHintShown = false;
	bool bBalconyHintShown = false;
	bool bNoCodeHintShown = false;
	bool bEnding = false;
	bool bFinished = false;
};
