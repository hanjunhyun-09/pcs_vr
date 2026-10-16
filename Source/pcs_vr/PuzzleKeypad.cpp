#include "PuzzleKeypad.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "PuzzleTextWidget.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "VRInteractionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeypad, Log, All);

namespace
{
	// 3열 x 4줄. 빈 칸은 버튼 없음
	const TCHAR* const KeyLayout[] = {
		TEXT("1"), TEXT("2"), TEXT("3"),
		TEXT("4"), TEXT("5"), TEXT("6"),
		TEXT("7"), TEXT("8"), TEXT("9"),
		TEXT("C"), TEXT("0"), TEXT(""),
	};
	// VR 손으로 누르기 쉽게 큼직하게: 버튼 9cm, 간격 11cm (판 40 x 66cm)
	constexpr float KeyPitch = 11.f;
	constexpr float KeySize = 9.f;
	constexpr float KeyTopZ = 5.f;      // 첫 줄 버튼 가운데 높이 (판 가운데 기준)
	constexpr float KeyFrontX = 4.f;    // 버튼 앞면 (벽에서 4cm)
	constexpr float HoverDepth = 10.f;  // 버튼 앞 이만큼까지 손끝이 오면 "올라감"
	constexpr float PushDepth = 1.5f;   // 버튼 앞면에서 이만큼 밀어 넣으면 눌림
	const FLinearColor KeyColor(0.7f, 0.72f, 0.75f);
	const FLinearColor KeyHoverColor(0.2f, 0.75f, 1.f);
	const FLinearColor KeyPressColor(0.3f, 1.f, 0.4f);

	FVector KeyCenter(int32 LayoutIndex)
	{
		const int32 Col = LayoutIndex % 3;
		const int32 Row = LayoutIndex / 3;
		// 앞(+X)에서 키패드를 보는 사람의 왼쪽 = 로컬 +Y. 첫 열(1·4·7)이 보는 사람 왼쪽에 오게
		return FVector(KeyFrontX - 1.f, (1 - Col) * KeyPitch, KeyTopZ - Row * KeyPitch);
	}
}

void UPuzzleKeypadKey::BeginPlay()
{
	Super::BeginPlay();
	OnInteract.AddDynamic(this, &UPuzzleKeypadKey::HandleSelf);
}

void UPuzzleKeypadKey::HandleSelf(AActor* InteractInstigator)
{
	if (APuzzleKeypad* Keypad = Cast<APuzzleKeypad>(GetOwner()))
	{
		Keypad->PressKey(Symbol);
	}
}

APuzzleKeypad::APuzzleKeypad()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 벽에 붙은 판: 두께 2cm, 폭 40cm, 높이 66cm (위쪽 14cm는 표시창)
	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(Root);
	Panel->SetStaticMesh(CubeMesh.Object);
	Panel->SetRelativeLocation(FVector(1.f, 0.f, -3.f));
	Panel->SetRelativeScale3D(FVector(2.f, 40.f, 66.f) / 100.f);

	Display = CreateDefaultSubobject<UWidgetComponent>(TEXT("Display"));
	Display->SetupAttachment(Root);
	Display->SetRelativeLocation(FVector(2.2f, 0.f, 21.f));
	Display->SetRelativeScale3D(FVector(0.05f));   // 20px = 1cm → 32 x 10cm
	Display->SetWidgetSpace(EWidgetSpace::World);
	Display->SetWidgetClass(UPuzzleTextWidget::StaticClass());
	Display->SetDrawSize(FVector2D(640.f, 200.f));
	Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Display->SetCanEverAffectNavigation(false);

	for (int32 i = 0; i < UE_ARRAY_COUNT(KeyLayout); ++i)
	{
		if (FCString::Strlen(KeyLayout[i]) == 0)
		{
			continue;
		}
		const FVector Center = KeyCenter(i);

		UStaticMeshComponent* KeyMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("KeyMesh_%s"), KeyLayout[i]));
		KeyMesh->SetupAttachment(Root);
		KeyMesh->SetStaticMesh(CubeMesh.Object);
		KeyMesh->SetRelativeLocation(Center);
		KeyMesh->SetRelativeScale3D(FVector(2.f, KeySize, KeySize) / 100.f);
		KeyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		KeyMeshes.Add(KeyMesh);

		// 버튼 앞면의 숫자 (6cm)
		UTextRenderComponent* Label = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("KeyLabel_%s"), KeyLayout[i]));
		Label->SetupAttachment(Root);
		Label->SetRelativeLocation(FVector(KeyFrontX + 0.2f, Center.Y, Center.Z));
		Label->SetText(FText::FromString(KeyLayout[i]));
		Label->SetWorldSize(6.f);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetTextRenderColor(FColor(20, 22, 26));
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		KeyLabels.Add(Label);

		// 데스크톱 E 키용. VR 손은 Tick에서 손끝으로 직접 고르므로 손 거리로는 안 잡히게 아주 작게
		UPuzzleKeypadKey* Key = CreateDefaultSubobject<UPuzzleKeypadKey>(*FString::Printf(TEXT("Key_%s"), KeyLayout[i]));
		Key->SetupAttachment(Root);
		Key->SetRelativeLocation(FVector(KeyFrontX, Center.Y, Center.Z));
		Key->InteractRadius = 0.01f;
		Key->Symbol = KeyLayout[i];
		Keys.Add(Key);
	}
}

void APuzzleKeypad::BeginPlay()
{
	Super::BeginPlay();

	PuzzleUtils::ApplySolidColor(Panel, FLinearColor(0.1f, 0.11f, 0.12f));
	for (UStaticMeshComponent* KeyMesh : KeyMeshes)
	{
		KeyMIDs.Add(PuzzleUtils::ApplySolidColor(KeyMesh, KeyColor, 0.3f));
	}
	Display->InitWidget();
	UpdateDisplay();
}

void APuzzleKeypad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bSolved)
	{
		return;
	}
	UpdateHand(LeftHand, TEXT("MotionControllerLeftGrip"), true);
	UpdateHand(RightHand, TEXT("MotionControllerRightGrip"), false);
	RefreshKeyColors();
}

int32 APuzzleKeypad::FindKeyAt(const FVector& LocalTip) const
{
	if (LocalTip.X < KeyFrontX - 6.f || LocalTip.X > KeyFrontX + HoverDepth)
	{
		return INDEX_NONE;
	}
	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		const FVector Rel = Keys[i]->GetRelativeLocation();
		if (FMath::Abs(LocalTip.Y - Rel.Y) <= KeyPitch * 0.5f && FMath::Abs(LocalTip.Z - Rel.Z) <= KeyPitch * 0.5f)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void APuzzleKeypad::UpdateHand(FHandState& State, const TCHAR* GripName, bool bLeft)
{
	const USceneComponent* Grip = PuzzleUtils::FindPawnComponent(PuzzleUtils::GetPlayerPawn(this), GripName);
	const UVRInteractionSubsystem* Interaction = GetWorld()->GetSubsystem<UVRInteractionSubsystem>();
	const bool bTriggerDown = Interaction && Interaction->IsTriggerDown(bLeft);
	if (!Grip)
	{
		State = FHandState();
		return;
	}

	// 손끝(컨트롤러 앞쪽)을 먼저 보고, 안 맞으면 손 가운데로
	const FTransform& KeypadTM = GetActorTransform();
	FVector LocalPoint = KeypadTM.InverseTransformPosition(Grip->GetComponentLocation() + Grip->GetForwardVector() * TipOffset);
	int32 Hovered = FindKeyAt(LocalPoint);
	if (Hovered == INDEX_NONE)
	{
		LocalPoint = KeypadTM.InverseTransformPosition(Grip->GetComponentLocation());
		Hovered = FindKeyAt(LocalPoint);
	}
	State.Hovered = Hovered;

	// 밀어 넣기: 버튼 앞면을 지나 들어가면 한 번 눌림, 앞으로 빼야 다시 눌림
	const bool bInside = Hovered != INDEX_NONE && LocalPoint.X < KeyFrontX - PushDepth;
	if (bInside && !State.bPushed)
	{
		State.bPushed = true;
		PressKey(Keys[Hovered]->Symbol);
	}
	else if (State.bPushed && (Hovered == INDEX_NONE || LocalPoint.X > KeyFrontX + 2.f))
	{
		State.bPushed = false;
	}

	// 트리거: 손끝이 올라간 버튼을 누름
	if (bTriggerDown && !State.bTriggerWasDown && Hovered != INDEX_NONE && !State.bPushed)
	{
		PressKey(Keys[Hovered]->Symbol);
	}
	State.bTriggerWasDown = bTriggerDown;
}

void APuzzleKeypad::PressKey(const FString& Symbol)
{
	if (bSolved)
	{
		return;
	}
	FlashIndex = Keys.IndexOfByPredicate([&](const UPuzzleKeypadKey* K) { return K && K->Symbol == Symbol; });
	FlashUntil = GetWorld()->GetTimeSeconds() + 0.2;
	RefreshKeyColors();

	if (bShowingError)
	{
		// "오류" 표시 중에 누르면 바로 새로 입력
		GetWorldTimerManager().ClearTimer(ErrorHandle);
		bShowingError = false;
		Input.Reset();
	}

	if (Symbol == TEXT("C"))
	{
		Input.Reset();
		UpdateDisplay();
		return;
	}
	Input += Symbol;
	UE_LOG(LogKeypad, Log, TEXT("%s: %s 누름 → 입력 %s"), *GetName(), *Symbol, *Input);

	if (Input.Len() < Code.Len())
	{
		UpdateDisplay();
		return;
	}

	if (Input == Code)
	{
		bSolved = true;
		UE_LOG(LogKeypad, Log, TEXT("%s: 비밀번호 맞음"), *GetName());
		UpdateDisplay();
		for (UPuzzleKeypadKey* Key : Keys)
		{
			Key->bInteractEnabled = false;
		}
		FlashIndex = INDEX_NONE;
		for (UMaterialInstanceDynamic* MID : KeyMIDs)
		{
			PuzzleUtils::SetSolidColor(MID, KeyPressColor, 1.f);
		}
		OnSolved.Broadcast();
	}
	else
	{
		// 틀려도 벌 없음: 잠깐 "오류" 표시 후 지움
		UE_LOG(LogKeypad, Log, TEXT("%s: 틀림 (%s)"), *GetName(), *Input);
		bShowingError = true;
		UpdateDisplay();
		OnWrongCode.Broadcast();
		GetWorldTimerManager().SetTimer(ErrorHandle, this, &APuzzleKeypad::ClearInput, 1.2f, false);
	}
}

void APuzzleKeypad::ClearInput()
{
	bShowingError = false;
	Input.Reset();
	UpdateDisplay();
}

void APuzzleKeypad::UpdateDisplay()
{
	UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(Display->GetUserWidgetObject());
	if (!W)
	{
		return;
	}
	const FLinearColor Bg(0.02f, 0.04f, 0.03f, 1.f);
	if (bSolved)
	{
		W->SetContent(FText::FromString(TEXT("승인")), 72, FLinearColor(0.3f, 1.f, 0.4f), Bg);
		return;
	}
	if (bShowingError)
	{
		W->SetContent(FText::FromString(TEXT("오류")), 72, FLinearColor(1.f, 0.25f, 0.2f), Bg);
		return;
	}
	// 입력한 숫자 + 남은 자리 "_"
	FString Shown;
	for (int32 i = 0; i < Code.Len(); ++i)
	{
		if (i > 0)
		{
			Shown += TEXT(" ");
		}
		Shown += i < Input.Len() ? Input.Mid(i, 1) : TEXT("_");
	}
	W->SetContent(FText::FromString(Shown), 72, FLinearColor(0.5f, 1.f, 0.7f), Bg);
}

void APuzzleKeypad::RefreshKeyColors()
{
	if (bSolved)
	{
		return;
	}
	const bool bFlashing = GetWorld()->GetTimeSeconds() < FlashUntil;
	for (int32 i = 0; i < KeyMIDs.Num(); ++i)
	{
		if (bFlashing && i == FlashIndex)
		{
			PuzzleUtils::SetSolidColor(KeyMIDs[i], KeyPressColor, 2.5f);
		}
		else if (i == LeftHand.Hovered || i == RightHand.Hovered)
		{
			PuzzleUtils::SetSolidColor(KeyMIDs[i], KeyHoverColor, 1.5f);
		}
		else
		{
			PuzzleUtils::SetSolidColor(KeyMIDs[i], KeyColor, 0.3f);
		}
	}
}
