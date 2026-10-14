#include "DesktopModeSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "DesktopPawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogDesktopMode, Log, All);

static TAutoConsoleVariable<int32> CVarForceDesktop(
	TEXT("vrpuzzle.ForceDesktop"),
	0,
	TEXT("1이면 헤드셋이 있어도 데스크톱 테스트 모드로 시작한다 (다음 레벨부터 적용)."));

bool UDesktopModeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UDesktopModeSubsystem::IsDesktopMode()
{
	if (CVarForceDesktop.GetValueOnGameThread() != 0 || FParse::Param(FCommandLine::Get(), TEXT("desktop")))
	{
		return true;
	}
	// HMD가 있어도 스테레오 렌더링이 꺼져 있으면(에디터 일반 Play 등) 데스크톱으로 본다
	const bool bHMD = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
	const bool bStereo = GEngine && GEngine->IsStereoscopic3D();
	return !(bHMD && bStereo);
}

void UDesktopModeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bChecked)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	// VR Preview가 스테레오를 켤 시간을 조금 준다
	if (!Pawn || World->GetTimeSeconds() < 0.1)
	{
		return;
	}
	bChecked = true;

	if (!IsDesktopMode())
	{
		UE_LOG(LogDesktopMode, Log, TEXT("VR 모드로 실행합니다."));
		return;
	}
	if (Pawn->IsA<ADesktopPawn>() || Pawn->IsA<ACharacter>())
	{
		// 이미 데스크톱용 캐릭터
		return;
	}
	SwapToDesktopPawn(PC, Pawn);
}

void UDesktopModeSubsystem::SwapToDesktopPawn(APlayerController* PC, APawn* OldPawn)
{
	UWorld* World = GetWorld();
	UClass* PawnClass = DesktopPawnClass.IsNull() ? nullptr : DesktopPawnClass.LoadSynchronous();
	if (!PawnClass)
	{
		PawnClass = ADesktopPawn::StaticClass();
	}

	// VRPawn의 루트는 바닥 높이 → 캡슐 반 높이만큼 올려서 스폰
	float HalfHeight = 92.f;
	if (const ACharacter* CDO = Cast<ACharacter>(PawnClass->GetDefaultObject()))
	{
		HalfHeight = CDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	}
	const float Yaw = OldPawn->GetActorRotation().Yaw;
	const FVector Location = OldPawn->GetActorLocation() + FVector(0.f, 0.f, HalfHeight + 2.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* NewPawn = World->SpawnActor<APawn>(PawnClass, Location, FRotator(0.f, Yaw, 0.f), Params);
	if (!NewPawn)
	{
		UE_LOG(LogDesktopMode, Warning, TEXT("데스크톱 폰을 스폰하지 못했습니다."));
		return;
	}

	PC->UnPossess();
	OldPawn->Destroy();
	PC->Possess(NewPawn);
	PC->SetControlRotation(FRotator(0.f, Yaw, 0.f));
	UE_LOG(LogDesktopMode, Log, TEXT("헤드셋이 꺼져 있어 데스크톱 테스트 모드로 시작합니다 (%s)."), *PawnClass->GetName());
}
