#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LabGameSubsystem.generated.h"

class STextBlock;
class SWidget;
class UTextRenderComponent;
class UCameraComponent;

// Runs automatically in every game level. Handles health, ammo, targets, ADS and the main menu.
UCLASS()
class ULabGameSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULabGameSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	UFUNCTION()
	void OnPlayerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);
	UFUNCTION()
	void OnPlayerOverlap(AActor* OverlappedActor, AActor* OtherActor);
	UFUNCTION()
	void OnTargetHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	void HandlePlayerTouch(AActor* OtherActor);
	void OnActorSpawned(AActor* Actor);
	void UpdateHud();

	bool bIsMenu = false;
	bool bIsGameLevel = false;
	bool bLeavingLevel = false;

	int32 Health = 100;
	int32 Ammo = 20;
	double LastBadHitTime = -100.0;

	TWeakObjectPtr<APawn> BoundPawn;
	TWeakObjectPtr<UCameraComponent> Camera;
	float DefaultFOV = 90.f;

	TMap<TWeakObjectPtr<AActor>, int32> TargetHealth;
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UTextRenderComponent>> TargetText;

	TSharedPtr<SWidget> RootWidget;
	TSharedPtr<STextBlock> HealthText;
	TSharedPtr<STextBlock> AmmoText;
	FDelegateHandle SpawnHandle;
};
