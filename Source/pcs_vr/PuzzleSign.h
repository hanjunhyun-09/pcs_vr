#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleSign.generated.h"

class UPuzzleTextWidget;
class UWidgetComponent;

/**
 * 벽에 붙이는 글자판 (포스터, 일정표, 명단, 이름표, 안내문).
 * 액터의 앞(+X) 방향에서 읽힌다. 크기는 DrawSize(픽셀) / PixelsPerCm 로 정해진다.
 * 예) DrawSize 800 x 500, PixelsPerCm 10 → 80cm x 50cm
 */
UCLASS()
class PCS_VR_API APuzzleSign : public AActor
{
	GENERATED_BODY()

public:
	APuzzleSign();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign", meta = (MultiLine = "true"))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign", meta = (ClampMin = "8"))
	int32 FontSize = 36;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign")
	FLinearColor TextColor = FLinearColor(0.05f, 0.05f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign")
	FLinearColor BackgroundColor = FLinearColor(0.85f, 0.83f, 0.78f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign")
	FVector2D DrawSize = FVector2D(800.f, 500.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign", meta = (ClampMin = "1.0"))
	float PixelsPerCm = 10.f;

	/** false면 처음엔 안 보임 (전원이 들어오면 보이는 일정표·명단 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign")
	bool bVisibleAtStart = true;

	UFUNCTION(BlueprintCallable, Category = "Sign")
	void SetSignText(const FText& NewText);

	/** 글자 색만 바꾼다 (시점 맞추기 조각이 가까워질수록 밝아질 때 등) */
	UFUNCTION(BlueprintCallable, Category = "Sign")
	void SetSignColor(const FLinearColor& NewTextColor);

	UFUNCTION(BlueprintCallable, Category = "Sign")
	void SetSignVisible(bool bVisible);

	UWidgetComponent* GetWidgetComponent() const { return Widget; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void ApplyLayout();
	UPuzzleTextWidget* GetTextWidget() const;

	UPROPERTY(VisibleAnywhere, Category = "Sign")
	TObjectPtr<UWidgetComponent> Widget;
};
