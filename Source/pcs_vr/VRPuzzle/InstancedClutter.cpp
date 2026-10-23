#include "InstancedClutter.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AInstancedClutter::AInstancedClutter()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
}

void AInstancedClutter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AInstancedClutter::BeginPlay()
{
	Super::BeginPlay();
	// 컴포넌트는 저장하지 않으므로(Transient) 게임을 시작할 때 다시 만든다 (패키징한 게임에서는 OnConstruction이 다시 불리지 않음)
	Build();
}

void AInstancedClutter::Build()
{
	// 묶음이 바뀔 때마다 컴포넌트를 새로 만든다 (에디터에서 값을 바꿔도, 맵을 불러와도 같은 결과)
	for (UInstancedStaticMeshComponent* Old : Instances)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	Instances.Reset();

	const FTransform ActorInv = GetActorTransform().Inverse();
	for (int32 g = 0; g < Groups.Num(); ++g)
	{
		const FClutterGroup& Group = Groups[g];
		if (!Group.Mesh || Group.Transforms.Num() == 0)
		{
			continue;
		}
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		ISM->SetMobility(EComponentMobility::Static);
		ISM->SetupAttachment(Root);
		ISM->SetStaticMesh(Group.Mesh);
		if (Group.Material)
		{
			ISM->SetMaterial(0, Group.Material);
		}
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
		ISM->SetCastShadow(false);
		ISM->SetGenerateOverlapEvents(false);
		// 작은 조각은 멀리서 안 보이므로 15m 밖에서는 그리지 않음
		ISM->SetCullDistances(0, 1500);
		ISM->RegisterComponent();
		TArray<FTransform> Local;
		Local.Reserve(Group.Transforms.Num());
		for (const FTransform& World : Group.Transforms)
		{
			Local.Add(World * ActorInv);
		}
		ISM->AddInstances(Local, false);
		Instances.Add(ISM);
	}
}
