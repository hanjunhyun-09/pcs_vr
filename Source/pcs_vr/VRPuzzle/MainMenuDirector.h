#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MainMenuDirector.generated.h"

class APuzzleButton;
class APuzzleSign;

/**
 * 메인 화면 진행 담당 (L_MainMenu).
 * [게임 시작] 버튼 → 화면이 어두워지고 StartLevel(인트로)을 연다.
 * [종료] 버튼 → 게임을 끈다.
 * 버튼은 VR에서는 손을 대고 트리거, 데스크톱에서는 바라보고 E 키 (APuzzleButton 그대로).
 */
UCLASS()
class PCS_VR_API AMainMenuDirector : public AActor
{
	GENERATED_BODY()

public:
	AMainMenuDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TObjectPtr<APuzzleButton> StartButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TObjectPtr<APuzzleButton> QuitButton;

	/** 버튼을 누르면 "시작하는 중…" 등으로 바뀌는 안내판 (없어도 됨) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TObjectPtr<APuzzleSign> HintSign;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FName StartLevel = TEXT("/Game/VRPuzzle/Maps/L_00_Intro");

	/** 맵이 열릴 때 검은 화면에서 밝아지는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu", meta = (ClampMin = "0.0"))
	float FadeInTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu", meta = (ClampMin = "0.0"))
	float FadeOutTime = 1.2f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleStart();

	UFUNCTION()
	void HandleQuit();

	void OpenStartLevel();
	void QuitNow();
	void Fade(float From, float To, float Time, bool bHold);

	FTimerHandle ActionHandle;
	bool bBusy = false;
};
