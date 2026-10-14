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
		Background->SetHorizontalAlignment(HAlign_Center);
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
