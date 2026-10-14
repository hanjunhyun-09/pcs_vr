#include "PuzzleSign.h"

#include "Components/WidgetComponent.h"
#include "PuzzleTextWidget.h"

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
}

void APuzzleSign::SetSignText(const FText& NewText)
{
	Text = NewText;
	if (UPuzzleTextWidget* W = GetTextWidget())
	{
		W->SetText(Text);
	}
}

void APuzzleSign::SetSignVisible(bool bVisible)
{
	Widget->SetVisibility(bVisible, true);
}
