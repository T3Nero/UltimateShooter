// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterAnimInstance.h"
#include "ParentCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

// BeginPlay()
void UShooterAnimInstance::NativeInitializeAnimation()
{
	Character = Cast<AParentCharacter>(TryGetPawnOwner());
}

// Tick()
void UShooterAnimInstance::UpdateAnimationProperties(float DeltaTime)
{
	if (Character == nullptr)
	{
		Character = Cast<AParentCharacter>(TryGetPawnOwner());
	}
	if (Character)
	{
		// Get Speed of Character from Velocity (X/Y Axis)
		FVector Velocity{ Character->GetVelocity() };
		Velocity.Z = 0;
		Speed = Velocity.Size();

		// Is Character in the air?
		bIsInAir = Character->GetCharacterMovement()->IsFalling();

		// Is Character Accelerating?
		if (Character->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0)
		{
			bIsAccelerating = true;
		}
		else
		{
			bIsAccelerating = false;
		}

		FRotator AimRotation = Character->GetBaseAimRotation();
		/*FString RotationMessage = FString::Printf(TEXT("Base Aim Rotation: %f"), AimRotation.Yaw);*/

		FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(Character->GetVelocity());
		/*FString MovementMessage = FString::Printf(TEXT("Base Aim Rotation: %f"), MovementRotation.Yaw);*/

				// Adds a Debug Print Message (Print String)
		//if (GEngine)
		//{
		//	GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::White, RotationMessage);
		//	GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::White, MovementMessage);
		//}

		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;

		if (Character->GetVelocity().Size() > 0.f)
		{
			LastMovementOffsetYaw = MovementOffsetYaw;
		}

		bAiming = Character->GetAiming();
	}
}
