#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleRecorder.generated.h"

class UMaterialInstanceDynamic;
class USoundBase;
class UStaticMeshComponent;
class UWidgetComponent;

/**
 * 책상 위 녹음기 (노션: 음성 기록, 퍼즐과 상관없이 언제든 들을 수 있음).
 * 버튼에 손을 대고 트리거 → Sound가 있으면 재생하고, 녹음기 위에 자막(Lines)을 차례로 띄운다.
 * 음성 파일이 아직 없으면 자막만 나온다. 재생 중에는 다시 눌리지 않는다.
 * 액터 위치 = 녹음기 바닥 가운데, 앞(+X) = 버튼·자막을 보는 쪽.
 */
UCLASS()
class PCS_VR_API APuzzleRecorder : public AActor
{
	GENERATED_BODY()

public:
	APuzzleRecorder();

	/** 예) 음성 기록 #1 · 2006.11.14 14:07 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder")
	FText Title;

	/** 자막 한 줄씩 (화자: 대사) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder", meta = (MultiLine = "true"))
	TArray<FText> Lines;

	/** 자막 한 줄이 떠 있는 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder", meta = (ClampMin = "0.5"))
	float LineDuration = 4.5f;

	/** 음성 파일 (VO_...). 없으면 자막만 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(BlueprintAssignable, Category = "Recorder")
	FPuzzleSimpleEvent OnPlayed;

	UFUNCTION(BlueprintCallable, Category = "Recorder")
	void Play();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleInteract(AActor* InteractInstigator);

	void ShowLine();
	void SetSubtitle(const FText& Text, bool bVisible);

	UPROPERTY(VisibleAnywhere, Category = "Recorder")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Recorder")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Recorder")
	TObjectPtr<UStaticMeshComponent> PlayButton;

	UPROPERTY(VisibleAnywhere, Category = "Recorder")
	TObjectPtr<UPuzzleInteractComponent> Interact;

	UPROPERTY(VisibleAnywhere, Category = "Recorder")
	TObjectPtr<UWidgetComponent> Subtitle;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ButtonMID;

	int32 LineIndex = -1;
	FTimerHandle LineHandle;
};
