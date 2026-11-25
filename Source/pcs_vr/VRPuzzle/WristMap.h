#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "WristMap.generated.h"

class APuzzleSign;

/** 손목 지도에 남은 오세진 메모 한 개 */
struct FWristMemo
{
	FText Text;
	FString Stage;   // "격리실", "원근" 등 (메모가 나온 스테이지)
};

/**
 * 손목 지도의 기록 (노션 "1스테이지 퍼즐 설계" · "맵 구조 설계").
 * 스테이지 진행 담당이 ShowMemo로 띄운 지도 메모를 모아 두고, 5스테이지에서 알아낸 코어 정지 코드를 적어 둔다.
 * 게임 인스턴스에 있어서 엘리베이터로 다음 맵에 가도 남는다. 메인 화면이나 인트로가 시작되면 새 게임이라 비운다.
 */
UCLASS()
class PCS_VR_API UWristMapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWristMapSubsystem* Get(const UObject* WorldContext);

	/** 지도 메모를 남긴다. 같은 글이 이미 있으면 맨 위(최신)로 올린다 */
	void AddMemo(const FText& Memo, const UObject* WorldContext);

	/** 5스테이지: 관측 발코니에서 읽은 코어 정지 코드 */
	void SetCoreCode(const FString& Code);

	void ResetForNewGame();

	const TArray<FWristMemo>& GetMemos() const { return Memos; }   // 오래된 것 → 최신 순
	const FString& GetCoreCode() const { return CoreCode; }

	/** 기록이 바뀔 때마다 올라감 (지도판이 글자를 다시 그릴지 판단) */
	int32 GetVersion() const { return Version; }

	/** 맵 이름 → 지도에 쓰는 스테이지 이름. 스테이지 맵이 아니면 빈 문자열 */
	static FString StageLabelForMap(const FString& MapName);

private:
	TArray<FWristMemo> Memos;
	FString CoreCode;
	int32 Version = 0;
};

/**
 * 손목 지도판을 띄우는 역할 (1 ~ 5스테이지 맵에서만).
 * - VR: 왼손을 눈앞으로 들어 올리면 손목 위에 지도판이 나타나고, 늘 눈 쪽을 향한다
 * - 데스크톱(또는 키보드가 있을 때): Tab을 누르고 있는 동안 눈앞에 보인다
 */
UCLASS()
class PCS_VR_API UWristMapWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UWristMapWorldSubsystem, STATGROUP_Tickables);
	}

	/** 손이 시선에서 이 각도(도) 안에 들어오면 보이고 */
	float ShowAngle = 25.f;
	/** 이 각도 밖으로 나가면 숨긴다 (경계에서 깜빡이지 않게 ShowAngle보다 넓게) */
	float HideAngle = 35.f;
	/** 눈에서 손까지 이 거리(cm) 안일 때만 */
	float MaxHandDistance = 75.f;

	/** 디버그(pcs.WristMapShot): Tab을 누른 것처럼 눈앞에 띄워 둔다 */
	bool bForceShow = false;

private:
	void RefreshText();
	void SetPanelVisible(bool bVisible);

	UPROPERTY(Transient)
	TObjectPtr<APuzzleSign> Panel;

	FString StageLabel;
	int32 ShownVersion = -1;
	bool bEnabled = false;
	bool bPanelVisible = false;
};
