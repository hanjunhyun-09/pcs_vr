#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

/**
 * 트리거 상호작용 인터페이스 (노션의 BPI_Interact 역할).
 * 버튼, 레버, 카드 리더기, 디스펜서 등이 이 인터페이스로만 반응하게 만들면
 * VR 손(트리거)이든 데스크톱(E 키)이든 같은 함수만 부르면 된다.
 *
 * 블루프린트에서 쓰기: Class Settings → Interfaces → Add → "Interactable"
 *                    → 이벤트 그래프에서 "Event Interact" 구현
 * 액터뿐 아니라 컴포넌트에 붙여도 된다 (TryInteract가 둘 다 찾는다).
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class PCS_VR_API IInteractable
{
	GENERATED_BODY()

public:
	/** 트리거(VR) 또는 E 키(데스크톱)로 상호작용했을 때 호출된다. Interactor = 누른 플레이어 폰 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	void Interact(AActor* Interactor);
};

UCLASS()
class PCS_VR_API UInteractLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Target 액터와 그 컴포넌트 중 Interactable을 구현한 것을 모두 호출한다.
	 * 하나라도 호출했으면 true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interact")
	static bool TryInteract(AActor* Target, AActor* Interactor);

	/** Target 액터나 그 컴포넌트 중 Interactable을 구현한 것이 있는지 */
	UFUNCTION(BlueprintPure, Category = "Interact")
	static bool CanInteract(const AActor* Target);
};
