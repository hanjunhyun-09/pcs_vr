#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PuzzleTextWidget.generated.h"

class UBorder;
class UTextBlock;

/**
 * 배경 + 글자 한 줄(또는 여러 줄)만 있는 간단한 위젯.
 * 포스터, 카드 리더기 화면, 지도 메모처럼 월드에 글자를 띄울 때 쓴다.
 * 위젯 블루프린트 없이 C++에서 바로 만들어지고, 한글도 표시된다.
 */
UCLASS()
class PCS_VR_API UPuzzleTextWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetContent(const FText& InText, int32 InFontSize, const FLinearColor& InTextColor, const FLinearColor& InBackground);
	void SetText(const FText& InText);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void ApplyContent();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	FText Text;
	int32 FontSize = 32;
	FLinearColor TextColor = FLinearColor::White;
	FLinearColor BackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.6f);
};
