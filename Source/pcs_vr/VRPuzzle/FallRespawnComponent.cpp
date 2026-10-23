#include "FallRespawnComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFallRespawn, Log, All);

void UFallRespawnComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		HomeTransform = Owner->GetActorTransform();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CheckHandle, this, &UFallRespawnComponent::CheckFallen, CheckInterval, true);
	}
}

void UFallRespawnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UFallRespawnComponent::CheckFallen()
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->GetActorLocation().Z < HomeTransform.GetLocation().Z - FallDistance)
	{
		RespawnNow();
	}
}

void UFallRespawnComponent::RespawnNow()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (Prim)
	{
		Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	Owner->SetActorLocationAndRotation(HomeTransform.GetLocation(), HomeTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogFallRespawn, Log, TEXT("%s 가 떨어져서 처음 위치로 되돌렸습니다."), *Owner->GetName());
}
