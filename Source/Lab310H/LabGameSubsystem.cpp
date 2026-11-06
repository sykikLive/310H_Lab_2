#include "LabGameSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// Blueprint class names used in the levels.
	const TCHAR* BadClass = TEXT("Bad_Blueprint_C");
	const TCHAR* TargetClass = TEXT("Target_Blueprint_C");
	const TCHAR* HealthPickupClass = TEXT("HealthPickup_Blueprint_C");
	const TCHAR* AmmoPickupClass = TEXT("AmmoPickup_Blueprint_C");
	const TCHAR* ProjectileClass = TEXT("BP_FirstPersonProjectile_C");

	const int32 MaxHealth = 100;
	const int32 MaxAmmo = 20;
	const int32 TargetMaxHealth = 100;

	bool IsClass(const AActor* Actor, const TCHAR* ClassName)
	{
		return Actor && Actor->GetClass()->GetName() == ClassName;
	}

	// Bad = the Bad blueprint, or anything placed in an "Obstacles" outliner folder.
	bool IsBad(const AActor* Actor)
	{
		if (IsClass(Actor, BadClass))
		{
			return true;
		}
#if WITH_EDITOR
		if (Actor && !IsClass(Actor, TargetClass) && !IsClass(Actor, HealthPickupClass) && !IsClass(Actor, AmmoPickupClass))
		{
			return Actor->GetFolderPath().ToString().Contains(TEXT("Obstacles"));
		}
#endif
		return false;
	}
}

void ULabGameSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(&InWorld);
	bIsMenu = LevelName == TEXT("Menu");
	bIsGameLevel = LevelName == TEXT("Lvl1") || LevelName == TEXT("Lvl2");

	UGameViewportClient* Viewport = InWorld.GetGameViewport();
	APlayerController* PC = InWorld.GetFirstPlayerController();

	if (bIsMenu && Viewport)
	{
		TWeakObjectPtr<UWorld> WeakWorld(&InWorld);

		RootWidget = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor::Black)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SButton)
					.OnClicked_Lambda([WeakWorld]()
					{
						if (WeakWorld.IsValid())
						{
							UGameplayStatics::OpenLevel(WeakWorld.Get(), TEXT("Lvl1"));
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Play")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SButton)
					.OnClicked_Lambda([WeakWorld]()
					{
						if (WeakWorld.IsValid())
						{
							UKismetSystemLibrary::QuitGame(WeakWorld.Get(), WeakWorld->GetFirstPlayerController(), EQuitPreference::Quit, false);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Quit")))
					]
				]
			];

		Viewport->AddViewportWidgetContent(RootWidget.ToSharedRef());

		if (PC)
		{
			PC->SetShowMouseCursor(true);
			PC->SetInputMode(FInputModeUIOnly());
		}
		return;
	}

	if (!bIsGameLevel)
	{
		return;
	}

	// HUD: health bottom-left, ammo bottom-right.
	if (Viewport)
	{
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 24);
		RootWidget = SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(20)
			[
				SAssignNew(HealthText, STextBlock).Font(Font)
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(20)
			[
				SAssignNew(AmmoText, STextBlock).Font(Font)
			];
		Viewport->AddViewportWidgetContent(RootWidget.ToSharedRef());
		UpdateHud();
	}

	if (PC)
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}

	// Targets: health + "100/100" text above each one.
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AActor* Target = *It;
		if (!IsClass(Target, TargetClass))
		{
			continue;
		}

		TargetHealth.Add(Target, TargetMaxHealth);
		Target->OnActorHit.AddDynamic(this, &ULabGameSubsystem::OnTargetHit);

		FVector Origin, Extent;
		Target->GetActorBounds(false, Origin, Extent);

		UTextRenderComponent* Text = NewObject<UTextRenderComponent>(Target);
		Text->SetupAttachment(Target->GetRootComponent());
		Text->RegisterComponent();
		Text->SetWorldLocation(Origin + FVector(0, 0, Extent.Z + 20.f));
		Text->SetWorldScale3D(FVector::OneVector);
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetWorldSize(26.f);
		Text->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), TargetMaxHealth, TargetMaxHealth)));
		TargetText.Add(Target, Text);
	}

	// Ammo: every gun projectile uses one ammo; with no ammo the projectile is removed.
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ULabGameSubsystem::OnActorSpawned));
}

void ULabGameSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
		if (RootWidget.IsValid() && World->GetGameViewport())
		{
			World->GetGameViewport()->RemoveViewportWidgetContent(RootWidget.ToSharedRef());
		}
	}
	RootWidget.Reset();
	HealthText.Reset();
	AmmoText.Reset();
	Super::Deinitialize();
}

void ULabGameSubsystem::Tick(float DeltaTime)
{
	if (!bIsGameLevel || bLeavingLevel)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	// Bind to the player pawn once it exists.
	if (Pawn && BoundPawn.Get() != Pawn)
	{
		BoundPawn = Pawn;
		Pawn->OnActorHit.AddDynamic(this, &ULabGameSubsystem::OnPlayerHit);
		Pawn->OnActorBeginOverlap.AddDynamic(this, &ULabGameSubsystem::OnPlayerOverlap);
		Camera = Pawn->FindComponentByClass<UCameraComponent>();
		if (Camera.IsValid())
		{
			DefaultFOV = Camera->FieldOfView;
		}
	}

	// Standing on a bad object doesn't fire hit events, so check the floor too.
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		AActor* FloorActor = Character->GetCharacterMovement()->CurrentFloor.HitResult.GetActor();
		if (IsBad(FloorActor))
		{
			HandlePlayerTouch(FloorActor);
			if (bLeavingLevel)
			{
				return;
			}
		}
	}

	// ADS: hold right mouse to zoom in linearly.
	if (PC && Camera.IsValid())
	{
		const float TargetFOV = PC->IsInputKeyDown(EKeys::RightMouseButton) ? DefaultFOV - 20.f : DefaultFOV;
		Camera->SetFieldOfView(FMath::FInterpConstantTo(Camera->FieldOfView, TargetFOV, DeltaTime, 100.f));
	}

	// Keep target health text facing the player.
	if (Camera.IsValid())
	{
		const FVector CamLoc = Camera->GetComponentLocation();
		for (auto& Pair : TargetText)
		{
			if (UTextRenderComponent* Text = Pair.Value.Get())
			{
				FRotator Rot = (CamLoc - Text->GetComponentLocation()).Rotation();
				Rot.Pitch = 0.f;
				Text->SetWorldRotation(Rot);
			}
		}
	}
}

void ULabGameSubsystem::OnPlayerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	HandlePlayerTouch(OtherActor);
}

void ULabGameSubsystem::OnPlayerOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	HandlePlayerTouch(OtherActor);
}

void ULabGameSubsystem::HandlePlayerTouch(AActor* OtherActor)
{
	if (bLeavingLevel || !IsValid(OtherActor))
	{
		return;
	}

	if (IsClass(OtherActor, HealthPickupClass))
	{
		Health = FMath::Min(Health + 25, MaxHealth);
		OtherActor->Destroy();
	}
	else if (IsClass(OtherActor, AmmoPickupClass))
	{
		Ammo = FMath::Min(Ammo + 10, MaxAmmo);
		OtherActor->Destroy();
	}
	else if (IsBad(OtherActor))
	{
		// Hit events fire every frame while touching, so only take damage once per second.
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - LastBadHitTime < 1.0)
		{
			return;
		}
		LastBadHitTime = Now;
		Health = FMath::Max(Health - 20, 0);
		if (Health <= 0)
		{
			bLeavingLevel = true;
			UGameplayStatics::OpenLevel(GetWorld(), FName(*UGameplayStatics::GetCurrentLevelName(GetWorld())));
		}
	}
	UpdateHud();
}

void ULabGameSubsystem::OnTargetHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bLeavingLevel || !IsClass(OtherActor, ProjectileClass))
	{
		return;
	}
	OtherActor->Destroy();

	int32* HP = TargetHealth.Find(SelfActor);
	if (!HP)
	{
		return;
	}

	*HP -= 25;
	if (*HP > 0)
	{
		if (UTextRenderComponent* Text = TargetText.FindRef(SelfActor).Get())
		{
			Text->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), *HP, TargetMaxHealth)));
		}
		return;
	}

	TargetHealth.Remove(SelfActor);
	TargetText.Remove(SelfActor);
	SelfActor->Destroy();

	if (TargetHealth.Num() == 0)
	{
		bLeavingLevel = true;
		const FString LevelName = UGameplayStatics::GetCurrentLevelName(GetWorld());
		UGameplayStatics::OpenLevel(GetWorld(), LevelName == TEXT("Lvl1") ? TEXT("Lvl2") : TEXT("Menu"));
	}
}

void ULabGameSubsystem::OnActorSpawned(AActor* Actor)
{
	if (!IsClass(Actor, ProjectileClass))
	{
		return;
	}

	if (Ammo <= 0)
	{
		// The gun is still finishing the spawn here, so disable it now and destroy it next tick.
		Actor->SetActorEnableCollision(false);
		Actor->SetActorHiddenInGame(true);
		TWeakObjectPtr<AActor> WeakActor(Actor);
		GetWorld()->GetTimerManager().SetTimerForNextTick([WeakActor]()
		{
			if (WeakActor.IsValid())
			{
				WeakActor->Destroy();
			}
		});
		return;
	}
	Ammo--;
	UpdateHud();
}

void ULabGameSubsystem::UpdateHud()
{
	if (HealthText.IsValid())
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HEALTH: %d"), Health)));
	}
	if (AmmoText.IsValid())
	{
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("AMMO: %d"), Ammo)));
	}
}
