// Fill out your copyright notice in the Description page of Project Settings.


#include "ParentCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet//GameplayStatics.h"
#include "Sound//SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Particles/ParticleSystemComponent.h"
#include "DrawDebugHelpers.h"
#include "..\Public\ParentCharacter.h"

// Sets default values
AParentCharacter::AParentCharacter()
{
	// Create a Camera Boom (pulls in towards character if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Boom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 200.0f; // Distance behind the Character
	CameraBoom->bUsePawnControlRotation = true; // Rotate Spring Arm based on Character Movement
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 70.f);

	// Create Follow Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Follow Camera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach Camera to end of Boom
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to Arm

	// Don't rotate when controller rotates (Controller only affects Character)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure Character Movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in Direction of Input
	GetCharacterMovement()->RotationRate = FRotator(0, 540, 0);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Turn Rates for Aiming / Not Aiming	(Gamepad Sensitivity)
	HipTurnRate = 90.f;
	HipLookUpRate = 90.f;
	AimingTurnRate = 20.f;
	AimingLookUpRate = 20.f;

	// Mouse Sensitivity
	MouseHipTurnRate = 1.f;
	MouseHipLookUpRate = 1.f;
	MouseAimingTurnRate = 0.2f;
	MouseAimingLookUpRate = 0.2f;

	CameraDefaultFOV = 0.0f; // Set in BeginPlay()
	CameraCurrentFOV = 0.0f; // Set in BeginPlay()
	CameraZoomedFOV = 45.0f;
	ZoomeInterpSpeed = 20.0f;

	bAiming = false;
}

// Called when the game starts or when spawned
void AParentCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FollowCamera)
	{
		CameraDefaultFOV = GetFollowCamera()->FieldOfView;
		CameraCurrentFOV = CameraDefaultFOV;
	}

}

void AParentCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SetLookRates();
	CameraInterpZoom(DeltaTime);
	CalculateCrosshairSpread(DeltaTime);


}

void AParentCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// Find out which way is Forward / Backward
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X) };
		AddMovementInput(Direction, Value);
	}
}

void AParentCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// Find out which way is Right / Left
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y) };
		AddMovementInput(Direction, Value);
	}
}

void AParentCharacter::TurnAtRate(float Rate)
{
	// Calculates Turn Rate based on BaseTurnRate * FPS
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame
}

void AParentCharacter::Turn(float Value)
{
	float MouseTurnRate;
	if (bAiming)
	{
		MouseTurnRate = MouseAimingTurnRate;
	}
	else
	{
		MouseTurnRate = MouseHipTurnRate;
	}
	AddControllerYawInput(Value * MouseTurnRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame
}

void AParentCharacter::FireWeapon()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance->Montage_IsPlaying(HipFireMontage))
	{
		if (FireSound)
		{
			UGameplayStatics::PlaySound2D(this, FireSound);
		}
		if (AnimInstance && HipFireMontage)
		{
			AnimInstance->Montage_Play(HipFireMontage);
			AnimInstance->Montage_JumpToSection(FName("StartShooting"));
		}

		const USkeletalMeshSocket* GunSocket = GetMesh()->GetSocketByName("GunSocket");
		if (GunSocket)
		{
			const FTransform SocketTransform = GunSocket->GetSocketTransform(GetMesh());

			if (MuzzleFlash)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
			}

			FVector BeamEnd;
			bool bBeamEnd = GetBeamEndLocation(SocketTransform.GetLocation(), BeamEnd);
			if (bBeamEnd)
			{
				// Spawns a Smoke Beam Effect when Shooting
				if (BeamParticle)
				{
					UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticle, SocketTransform);
					if (Beam)
					{
						Beam->SetVectorParameter(FName("Target"), BeamEnd);
					}
				}
				if (ImpactEffect)
				{
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactEffect, BeamEnd);
				}
			}
		}
	}
}

void AParentCharacter::LookAtRate(float Rate)
{
	// Calculates Look Up/Down Rate based on BaseLookAtRate * FPS
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame
}

void AParentCharacter::LookUp(float Value)
{
	float MouseLookUpRate;
	if (bAiming)
	{
		MouseLookUpRate = MouseAimingLookUpRate;
	}
	else
	{
		MouseLookUpRate = MouseHipLookUpRate;
	}
	AddControllerPitchInput(Value * MouseLookUpRate * GetWorld()->GetDeltaSeconds()); // deg/sec * sec/frame
}

bool AParentCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FVector& OutBeamLocation)
{
	// Line Trace using Crosshairs World Position/Direction as Start/End Locations
	// Gets current size of the viewport
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	//// Get Screen Space Location of Crosshairs
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	CrosshairLocation.Y -= 50.f;

	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	//// Get world position and direction of crosshairs
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation, CrosshairWorldPosition, CrosshairWorldDirection);

	if (bScreenToWorld) // Was Deprojection successful?
	{
		FHitResult ScreenTracehit;
		const FVector TraceStart{ CrosshairWorldPosition };
		const FVector TraceEnd{ CrosshairWorldPosition + CrosshairWorldDirection * 50'000.0f };

		// Line Trace From Crosshair Position in World
		OutBeamLocation = TraceEnd;

		GetWorld()->LineTraceSingleByChannel(
			ScreenTracehit,
			TraceStart,
			TraceEnd,
			ECollisionChannel::ECC_Visibility);

		if (ScreenTracehit.bBlockingHit)
		{
			OutBeamLocation = ScreenTracehit.Location;
		}


		// Line Trace From Weapon Socket Location (Muzzle)
		FHitResult WeaponTraceHit;
		const FVector WeaponTraceStart{ MuzzleSocketLocation };
		const FVector WeaponTraceEnd{ OutBeamLocation };
		GetWorld()->LineTraceSingleByChannel(
			WeaponTraceHit,
			WeaponTraceStart,
			WeaponTraceEnd,
			ECollisionChannel::ECC_Visibility);

		if (WeaponTraceHit.bBlockingHit)
		{
			OutBeamLocation = WeaponTraceHit.Location;
		}
		return true;
	}
	return false;
}

void AParentCharacter::AimingButtonPressed()
{
	bAiming = true;
}

void AParentCharacter::AimingButtonReleased()
{
	bAiming = false;
}

void AParentCharacter::CameraInterpZoom(float DeltaTime)
{
	// Set CurrentCameraFOV using FInterpTo (Smooth transition)
	if (bAiming)
	{
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraZoomedFOV, DeltaTime, ZoomeInterpSpeed);
	}
	else if (!bAiming)
	{
		CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraDefaultFOV, DeltaTime, ZoomeInterpSpeed);
	}
	GetFollowCamera()->SetFieldOfView(CameraCurrentFOV);
}

void AParentCharacter::SetLookRates()
{
	if (bAiming)
	{
		BaseTurnRate = AimingTurnRate;
		BaseLookUpRate = AimingLookUpRate;
	}
	else if (!bAiming)
	{
		BaseTurnRate = HipTurnRate;
		BaseLookUpRate = HipLookUpRate;
	}
}

void AParentCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	FVector2D WalkSpeedRange{ 0.f, 600.f };
	FVector2D VelocityMultiplierRange{ 0.f, 1.f };
	FVector Velocity{ GetVelocity() };
	Velocity.Z = 0.f;

	CrosshairVelocity = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());
	CrosshairSpreadMultiplier = 0.2f + CrosshairVelocity;
}

// Called to bind functionality to input
void AParentCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AParentCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AParentCharacter::MoveRight);
	PlayerInputComponent->BindAxis("TurnRate", this, &AParentCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AParentCharacter::LookAtRate);
	PlayerInputComponent->BindAxis("Turn", this, &AParentCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AParentCharacter::LookUp);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("FireButton", IE_Pressed, this, &AParentCharacter::FireWeapon);
	PlayerInputComponent->BindAction("Aiming", IE_Pressed, this, &AParentCharacter::AimingButtonPressed);
	PlayerInputComponent->BindAction("Aiming", IE_Released, this, &AParentCharacter::AimingButtonReleased);

}

float AParentCharacter::GetCrosshairSpreadMultiplier() const
{
	return CrosshairSpreadMultiplier;
}

