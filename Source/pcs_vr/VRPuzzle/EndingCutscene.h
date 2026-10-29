#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EndingCutscene.generated.h"

class ACameraActor;
class APuzzleSign;

/** 컷신의 한 장면: 정해 둔 카메라로 보여 주고, 필요하면 글자판을 띄우거나 물체를 움직인다 */
USTRUCT(BlueprintType)
struct FEndingShot
{
	GENERATED_BODY()

	/** 이 장면을 보여 줄 카메라 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	TObjectPtr<ACameraActor> Camera;

	/** 데스크톱에서만: 장면 동안 Camera → EndCamera로 천천히 이동. VR에서는 멀미 때문에 Camera에 고정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	TObjectPtr<ACameraActor> EndCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.5"))
	float Duration = 4.f;

	/** 장면 시작/끝에 FadeColor에서 밝아지고, 다시 FadeColor로 어두워지는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.0"))
	float FadeIn = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.0"))
	float FadeOut = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	FLinearColor FadeColor = FLinearColor::Black;

	/** 장면이 시작될 때 보이게 할 글자판 (쪽지, 크레딧) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	TObjectPtr<APuzzleSign> Sign;

	/** 비어 있지 않으면 장면 시간을 똑같이 나눠 Sign 글자를 차례로 바꾼다 (크레딧 넘기기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (MultiLine = "true"))
	TArray<FText> Pages;

	/** 장면 동안 MoveOffset만큼 옮길 물체 (출입문 열기 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	TObjectPtr<AActor> MoveActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	FVector MoveOffset = FVector::ZeroVector;
};

/**
 * 엔딩 컷신 진행 담당 (L_06_Ending).
 * 맵이 열리면 플레이어를 숨기고 Shots를 차례로 보여 준 뒤 NextLevel(메인 화면)로 돌아간다.
 * - 카메라 액터로 시점을 바꾸지만, VR에서는 머리 회전이 그대로 적용되고 카메라는 움직이지 않는다 (멀미 방지)
 * - 장면 사이는 페이드로 이어서 시점이 갑자기 바뀌는 느낌을 줄인다
 */
UCLASS()
class PCS_VR_API AEndingCutscene : public AActor
{
	GENERATED_BODY()

public:
	AEndingCutscene();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	TArray<FEndingShot> Shots;

	/** 맵이 열리고 첫 장면이 나오기까지 기다리는 시간 (엘리베이터에서 넘어온 검은 화면) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending", meta = (ClampMin = "0.0"))
	float StartDelay = 1.5f;

	/** 컷신이 끝나면 열 레벨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending")
	FName NextLevel = TEXT("/Game/VRPuzzle/Maps/L_MainMenu");

	UFUNCTION(BlueprintCallable, Category = "Ending")
	void StartCutscene();

protected:
	virtual void BeginPlay() override;

private:
	void PlayShot(int32 Index);
	void FadeOutShot();
	void FinishCutscene();
	void OpenNextLevel();
	void Fade(float From, float To, float Time, const FLinearColor& Color, bool bHold);
	static bool IsHeadsetActive();

	int32 ShotIndex = INDEX_NONE;
	float ShotTime = 0.f;
	int32 PageIndex = INDEX_NONE;
	FVector MoveStart = FVector::ZeroVector;
	FTransform CameraStart;
	bool bHeadset = false;
	bool bStarted = false;

	FTimerHandle StartHandle;
	FTimerHandle FadeHandle;
	FTimerHandle NextHandle;
};
