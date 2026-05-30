// Copyright Epic Games, Inc. All Rights Reserved.


#include "TacticalAIPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "TacticalAI.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "AI/Components/PlayerCrowdAgentComponent.h"
#include "GameFramework/Pawn.h"

void ATacticalAIPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogTacticalAI, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ATacticalAIPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ATacticalAIPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}


void ATacticalAIPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// 플레이어가 빙의한 Pawn = Leader = crowd agent ON.
	SetAvoidanceRole(ECrowdAvoidanceRole::Leader);
	UE_LOG(LogTemp, Warning, TEXT("[Avoidance] Player possess -> Agent ON"))
}

void ATacticalAIPlayerController::OnUnPossess()
{
	// Super가 Pawn 참조를 끊기 전에 agent 끔.
	if (APawn* Old = GetPawn())
	{
		if (auto* Agent = Old->FindComponentByClass<UPlayerCrowdAgentComponent>())
		{
			Agent->SetObstacleActive(false);
		}
	}
	Super::OnUnPossess();
}

void ATacticalAIPlayerController::SetAvoidanceRole(ECrowdAvoidanceRole CrowdRole)
{
	APawn* P = GetPawn();
	if (!P) return;
	if (auto* Agent = P->FindComponentByClass<UPlayerCrowdAgentComponent>())
	{
		// 플레이어 입장에서 의미 있는 건 Leader뿐. 그 외엔 agent 끔(방어).
		Agent->SetObstacleActive(CrowdRole == ECrowdAvoidanceRole::Leader);
	}
}