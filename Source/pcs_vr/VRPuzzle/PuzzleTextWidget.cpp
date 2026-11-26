#include "PuzzleTextWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UPuzzleTextWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		Background->SetPadding(FMargin(24.f, 16.f));
		// 가로는 판 전체 너비를 준다 (가운데 정렬은 Label의 Justification이 맡음).
		// HAlign_Center면 자동 줄바꿈이 '이전 글자 너비'를 기준으로 일어나서, 글자를 바꿀 때(크레딧 넘기기)
		// 짧은 쪽 다음 긴 쪽이 잘게 쪼개지고 아래가 잘렸다.
		Background->SetHorizontalAlignment(HAlign_Fill);
		Background->SetVerticalAlignment(VAlign_Center);
		Background->SetContent(Label);
		Label->SetAutoWrapText(true);
		Label->SetJustification(ETextJustify::Center);
		WidgetTree->RootWidget = Background;
	}
	ApplyContent();
	return Super::RebuildWidget();
}

void UPuzzleTextWidget::SetContent(const FText& InText, int32 InFontSize, const FLinearColor& InTextColor, const FLinearColor& InBackground)
{
	Text = InText;
	FontSize = InFontSize;
	TextColor = InTextColor;
	BackgroundColor = InBackground;
	ApplyContent();
}

void UPuzzleTextWidget::SetText(const FText& InText)
{
	Text = InText;
	ApplyContent();
}

void UPuzzleTextWidget::ApplyContent()
{
	if (Label)
	{
		Label->SetText(Text);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(TextColor));
	}
	if (Background)
	{
		Background->SetBrushColor(BackgroundColor);
	}
}
