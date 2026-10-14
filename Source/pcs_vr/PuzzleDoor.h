#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleDoor.generated.h"

class UStaticMeshComponent;

/**
 * 옆으로 미끄러지는 문 (노션 기준: 폭 120cm, 높이 220cm).
 * 액터 위치 = 문 아래쪽 가운데. 문은 액터의 Y축(오른쪽) 방향으로 열린다 (벽 속으로 들어감).
 */
UCLASS()
class PCS_VR_API APuzzleDoor : public AActor
{
	GENERATED_BODY()

public:
	APuzzleDoor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float Width = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float Height = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float Thickness = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bStartOpen = false;

	/** 열리고 닫히는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (ClampMin = "0.05"))
	float MoveTime = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FLinearColor DoorColor = FLinearColor(0.32f, 0.34f, 0.36f);

	UFUNCTION(BlueprintCallable, Category = "Door")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "Door")
	void Close();

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bTargetOpen; }

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void ApplyPanel(float OpenAmount);

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> Panel;

	bool bTargetOpen = false;
	float OpenAmount = 0.f;
};
