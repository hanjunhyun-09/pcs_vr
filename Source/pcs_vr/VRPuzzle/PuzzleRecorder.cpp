#include "PuzzleRecorder.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PuzzleTextWidget.h"
#include "PuzzleUtils.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRecorder, Log, All);

namespace
{
	const FLinearColor ButtonIdle(0.9f, 0.15f, 0.1f);
	const FLinearColor ButtonPlaying(0.2f, 0.9f, 0.3f);
}

APuzzleRecorder::APuzzleRecorder()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 본체: 20 x 12 x 6cm 휴대용 녹음기
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetStaticMesh(CubeMesh.Object);
	Body->SetRelativeLocation(FVector(0.f, 0.f, 3.f));
	Body->SetRelativeScale3D(FVector(12.f, 20.f, 6.f) / 100.f);

	// 재생 버튼: 윗면 앞쪽, 지름 4cm
	PlayButton = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayButton"));
	PlayButton->SetupAttachment(Root);
	PlayButton->SetStaticMesh(CylinderMesh.Object);
	PlayButton->SetRelativeLocation(FVector(2.f, 5.f, 6.5f));
	PlayButton->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.01f));
	PlayButton->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Interact = CreateDefaultSubobject<UPuzzleInteractComponent>(TEXT("Interact"));
	Interact->SetupAttachment(Root);
	Interact->SetRelativeLocation(FVector(2.f, 5.f, 8.f));
	Interact->InteractRadius = 15.f;

	// 자막: 녹음기 위 45cm, 폭 90cm
	Subtitle = CreateDefaultSubobject<UWidgetComponent>(TEXT("Subtitle"));
	Subtitle->SetupAttachment(Root);
	Subtitle->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	Subtitle->SetRelativeScale3D(FVector(0.1f));   // 10px = 1cm
	Subtitle->SetWidgetSpace(EWidgetSpace::World);
	Subtitle->SetWidgetClass(UPuzzleTextWidget::StaticClass());
	Subtitle->SetDrawSize(FVector2D(900.f, 220.f));
	Subtitle->SetBlendMode(EWidgetBlendMode::Transparent);
	Subtitle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Subtitle->SetCanEverAffectNavigation(false);
}

void APuzzleRecorder::BeginPlay()
{
	Super::BeginPlay();

	PuzzleUtils::ApplySolidColor(Body, FLinearColor(0.08f, 0.08f, 0.09f));
	ButtonMID = PuzzleUtils::ApplySolidColor(PlayButton, ButtonIdle, 1.2f);
	Interact->OnInteract.AddDynamic(this, &APuzzleRecorder::HandleInteract);
	Subtitle->InitWidget();
	SetSubtitle(FText::GetEmpty(), false);
}

void APuzzleRecorder::HandleInteract(AActor* InteractInstigator)
{
	Play();
}

void APuzzleRecorder::Play()
{
	if (LineIndex >= 0)
	{
		return;   // 재생 중
	}
	UE_LOG(LogRecorder, Log, TEXT("%s: 재생 (%s)"), *GetName(), *Title.ToString());
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
	PuzzleUtils::SetSolidColor(ButtonMID, ButtonPlaying, 1.5f);
	Interact->bInteractEnabled = false;
	OnPlayed.Broadcast();

	// 제목 먼저, 그다음 한 줄씩
	LineIndex = 0;
	SetSubtitle(Title, true);
	GetWorldTimerManager().SetTimer(LineHandle, this, &APuzzleRecorder::ShowLine, 2.5f, false);
}

void APuzzleRecorder::ShowLine()
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		// 끝: 다시 들을 수 있게
		LineIndex = -1;
		SetSubtitle(FText::GetEmpty(), false);
		PuzzleUtils::SetSolidColor(ButtonMID, ButtonIdle, 1.2f);
		Interact->bInteractEnabled = true;
		return;
	}
	SetSubtitle(Lines[LineIndex], true);
	++LineIndex;
	GetWorldTimerManager().SetTimer(LineHandle, this, &APuzzleRecorder::ShowLine, LineDuration, false);
}

void APuzzleRecorder::SetSubtitle(const FText& Text, bool bVisible)
{
	if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(Subtitle->GetUserWidgetObject()))
	{
		W->SetContent(Text, 34, FLinearColor(1.f, 0.95f, 0.8f), FLinearColor(0.f, 0.f, 0.f, 0.75f));
	}
	Subtitle->SetVisibility(bVisible);
}
