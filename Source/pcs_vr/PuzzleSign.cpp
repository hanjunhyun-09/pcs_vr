#include "PuzzleSign.h"

#include "Components/WidgetComponent.h"
#include "PuzzleTextWidget.h"
#include "TimerManager.h"

APuzzleSign::APuzzleSign()
{
	PrimaryActorTick.bCanEverTick = false;

	Widget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget"));
	SetRootComponent(Widget);
	Widget->SetWidgetSpace(EWidgetSpace::World);
	Widget->SetWidgetClass(UPuzzleTextWidget::StaticClass());
	Widget->SetBlendMode(EWidgetBlendMode::Transparent);
	Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Widget->SetCanEverAffectNavigation(false);
	Widget->SetGenerateOverlapEvents(false);
	// 글자판은 거의 바뀌지 않으므로 매 프레임 다시 그리지 않고, 글자·색·보이기가 바뀔 때만 그린다 (VR 프레임 절약)
	Widget->SetManuallyRedraw(true);
}

void APuzzleSign::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLayout();
}

void APuzzleSign::ApplyLayout()
{
	Widget->SetDrawSize(DrawSize);
	Widget->SetRelativeScale3D(FVector(1.f / PixelsPerCm));
}

UPuzzleTextWidget* APuzzleSign::GetTextWidget() const
{
	return Cast<UPuzzleTextWidget>(Widget->GetUserWidgetObject());
}

void APuzzleSign::BeginPlay()
{
	Super::BeginPlay();
	ApplyLayout();
	Widget->InitWidget();
	if (UPuzzleTextWidget* W = GetTextWidget())
	{
		W->SetContent(Text, FontSize, TextColor, BackgroundColor);
	}
	SetSignVisible(bVisibleAtStart);

	// 처음 몇 초는 한글 글꼴이 늦게 준비될 수 있어서 몇 번 더 그려 둔다
	GetWorldTimerManager().SetTimer(WarmupHandle, this, &APuzzleSign::WarmupRedraw, 0.4f, true, 0.1f);
}

void APuzzleSign::WarmupRedraw()
{
	Widget->RequestRedraw();
	if (++WarmupCount >= 8)
	{
		GetWorldTimerManager().ClearTimer(WarmupHandle);
	}
}

void APuzzleSign::SetSignText(const FText& NewText)
{
	Text = NewText;
	if (UPuzzleTextWidget* W = GetTextWidget())
	{
		W->SetText(Text);
	}
	Widget->RequestRedraw();
}

void APuzzleSign::SetSignColor(const FLinearColor& NewTextColor)
{
	TextColor = NewTextColor;
	if (UPuzzleTextWidget* W = GetTextWidget())
	{
		W->SetContent(Text, FontSize, TextColor, BackgroundColor);
	}
	Widget->RequestRedraw();
}

void APuzzleSign::SetSignVisible(bool bVisible)
{
	Widget->SetVisibility(bVisible, true);
	Widget->RequestRedraw();
}
