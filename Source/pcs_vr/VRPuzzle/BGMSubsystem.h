#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "BGMSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * 맵별 배경음 (노션 "배경음 (BGM)").
 * 레벨이 시작될 때 맵 이름을 보고 /Game/VRPuzzle/Audio/BGM 의 곡을 2D로 반복 재생한다.
 *
 *   L_MainMenu                 BGM_00_MainMenu
 *   L_00_Intro, L_01_Isolation BGM_01_Isolation   (인트로 → 격리실은 같은 곡이 끊기지 않고 이어짐)
 *   L_02_Perspective           BGM_02_Perspective
 *   L_03_Observation           BGM_03_Observation
 *   L_04_Device                BGM_04_Device
 *   L_05_Core                  BGM_05_Core
 *   L_06_Ending                BGM_06_Ending
 *   그 밖의 맵 (테스트 맵 등)   배경음 없음
 *
 * 곡은 레벨 전환을 넘어 살아남는 오디오 컴포넌트로 재생해서, 다음 맵이 같은 곡이면 그대로 이어 가고
 * 다른 곡이면 FadeTime 동안 이전 곡을 줄이며 새 곡을 키운다.
 * 반복(Looping)은 Sound Wave 에셋에 켜 둔다 (Scripts/import_bgm.py).
 */
UCLASS()
class PCS_VR_API UBGMSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** 맵이 시작될 때 UBGMWorldSubsystem이 부른다 */
	void HandleLevelStarted(UWorld& World);

	/** 지금 곡을 FadeOutTime 동안 줄이며 멈춘다 (예: 엔딩 연출에서 음악을 끄고 싶을 때) */
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void StopBGM(float FadeOutTime = 1.5f);

	/** 지금 맵에 맞는 곡을 다시 튼다 (StopBGM 뒤에 되돌릴 때) */
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void ResumeBGM();

	/** 배경음 전체 음량 (0 ~ 1). 음성 기록이 묻히지 않도록 0.5에서 시작 (노션 메모) */
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void SetBGMVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "BGM")
	float GetBGMVolume() const { return Volume; }

	/** 맵 이름(L_02_Perspective 등)에 맞는 곡 경로. 없으면 빈 경로 */
	static FSoftObjectPath GetTrackForMap(const FString& MapName);

	/** 곡이 바뀔 때 이전 곡이 줄고 새 곡이 커지는 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	float FadeTime = 1.5f;

private:
	void PlayTrack(UWorld& World, const FSoftObjectPath& Track);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Current;

	FSoftObjectPath CurrentTrack;
	FString CurrentMap;
	float Volume = 0.5f;
};

/** 게임/PIE 월드가 시작될 때 UBGMSubsystem에 알리는 역할만 한다 */
UCLASS()
class PCS_VR_API UBGMWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
