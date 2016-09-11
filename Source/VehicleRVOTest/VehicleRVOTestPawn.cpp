// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VehicleRVOTest.h"
#include "VehicleRVOTestPawn.h"
#include "VehicleRVOTestWheelFront.h"
#include "VehicleRVOTestWheelRear.h"
#include "VehicleRVOTestHud.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Vehicles/WheeledVehicleMovementComponent4W.h"
#include "Engine/SkeletalMesh.h"
#include "Engine.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "SplinePath.h"

// Needed for VR Headset
#if HMD_MODULE_INCLUDED
#include "IHeadMountedDisplay.h"
#endif // HMD_MODULE_INCLUDED

PRAGMA_DISABLE_OPTIMIZATION

const FName AVehicleRVOTestPawn::LookUpBinding("LookUp");
const FName AVehicleRVOTestPawn::LookRightBinding("LookRight");

#define LOCTEXT_NAMESPACE "VehiclePawn"

AVehicleRVOTestPawn::AVehicleRVOTestPawn()
{
	// Car mesh
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/Vehicle/Sedan/Sedan_SkelMesh.Sedan_SkelMesh"));
	GetMesh()->SetSkeletalMesh(CarMesh.Object);

	static ConstructorHelpers::FClassFinder<UObject> AnimBPClass(TEXT("/Game/Vehicle/Sedan/Sedan_AnimBP"));
	GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);

	// Simulation
	UWheeledVehicleMovementComponent4W* Vehicle4W = CastChecked<UWheeledVehicleMovementComponent4W>(GetVehicleMovement());

	check(Vehicle4W->WheelSetups.Num() == 4);

	Vehicle4W->WheelSetups[0].WheelClass = UVehicleRVOTestWheelFront::StaticClass();
	Vehicle4W->WheelSetups[0].BoneName = FName("Wheel_Front_Left");
	Vehicle4W->WheelSetups[0].AdditionalOffset = FVector(0.f, -12.f, 0.f);

	Vehicle4W->WheelSetups[1].WheelClass = UVehicleRVOTestWheelFront::StaticClass();
	Vehicle4W->WheelSetups[1].BoneName = FName("Wheel_Front_Right");
	Vehicle4W->WheelSetups[1].AdditionalOffset = FVector(0.f, 12.f, 0.f);

	Vehicle4W->WheelSetups[2].WheelClass = UVehicleRVOTestWheelRear::StaticClass();
	Vehicle4W->WheelSetups[2].BoneName = FName("Wheel_Rear_Left");
	Vehicle4W->WheelSetups[2].AdditionalOffset = FVector(0.f, -12.f, 0.f);

	Vehicle4W->WheelSetups[3].WheelClass = UVehicleRVOTestWheelRear::StaticClass();
	Vehicle4W->WheelSetups[3].BoneName = FName("Wheel_Rear_Right");
	Vehicle4W->WheelSetups[3].AdditionalOffset = FVector(0.f, 12.f, 0.f);

	// Create a spring arm component
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm0"));
	SpringArm->TargetOffset = FVector(0.f, 0.f, 200.f);
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 600.0f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 7.f;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;

	// Create camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 90.f;

	// Create In-Car camera component 
	InternalCameraOrigin = FVector(0.0f, -40.0f, 120.0f);

	InternalCameraBase = CreateDefaultSubobject<USceneComponent>(TEXT("InternalCameraBase"));
	InternalCameraBase->SetRelativeLocation(InternalCameraOrigin);
	InternalCameraBase->SetupAttachment(GetMesh());

	InternalCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InternalCamera"));
	InternalCamera->bUsePawnControlRotation = false;
	InternalCamera->FieldOfView = 90.f;
	InternalCamera->SetupAttachment(InternalCameraBase);

	//Setup TextRenderMaterial
	static ConstructorHelpers::FObjectFinder<UMaterial> TextMaterial(TEXT("Material'/Engine/EngineMaterials/AntiAliasedTextMaterialTranslucent.AntiAliasedTextMaterialTranslucent'"));
	
	UMaterialInterface* Material = TextMaterial.Object;

	// Create text render component for in car speed display
	InCarSpeed = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarSpeed"));
	InCarSpeed->SetTextMaterial(Material);
	InCarSpeed->SetRelativeLocation(FVector(70.0f, -75.0f, 99.0f));
	InCarSpeed->SetRelativeRotation(FRotator(18.0f, 180.0f, 0.0f));
	InCarSpeed->SetupAttachment(GetMesh());
	InCarSpeed->SetRelativeScale3D(FVector(1.0f, 0.4f, 0.4f));

	// Create text render component for in car gear display
	InCarGear = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
	InCarGear->SetTextMaterial(Material);
	InCarGear->SetRelativeLocation(FVector(66.0f, -9.0f, 95.0f));	
	InCarGear->SetRelativeRotation(FRotator(25.0f, 180.0f,0.0f));
	InCarGear->SetRelativeScale3D(FVector(1.0f, 0.4f, 0.4f));
	InCarGear->SetupAttachment(GetMesh());
	
	// Colors for the incar gear display. One for normal one for reverse
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	// Colors for the in-car gear display. One for normal one for reverse
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	bInReverseGear = false;

	bEnableAutoDrive = false;
	SteeringTarget = FVector::ZeroVector;
	SplinePath = nullptr;
}

void AVehicleRVOTestPawn::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);

	// set up gameplay key bindings
	check(InputComponent);

	InputComponent->BindAxis("MoveForward", this, &AVehicleRVOTestPawn::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AVehicleRVOTestPawn::MoveRight);
	InputComponent->BindAxis("LookUp");
	InputComponent->BindAxis("LookRight");

	InputComponent->BindAction("Handbrake", IE_Pressed, this, &AVehicleRVOTestPawn::OnHandbrakePressed);
	InputComponent->BindAction("Handbrake", IE_Released, this, &AVehicleRVOTestPawn::OnHandbrakeReleased);
	InputComponent->BindAction("SwitchCamera", IE_Pressed, this, &AVehicleRVOTestPawn::OnToggleCamera);

	InputComponent->BindAction("ResetVR", IE_Pressed, this, &AVehicleRVOTestPawn::OnResetVR); 
}

void AVehicleRVOTestPawn::MoveForward(float Val)
{
//	if ( !bEnableAutoDrive )
		GetVehicleMovementComponent()->SetThrottleInput(Val);
}

void AVehicleRVOTestPawn::MoveRight(float Val)
{
	if ( !bEnableAutoDrive || (FMath::Abs(Val) > 0 ))
		GetVehicleMovementComponent()->SetSteeringInput(Val);
}

void AVehicleRVOTestPawn::OnHandbrakePressed()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(true);
}

void AVehicleRVOTestPawn::OnHandbrakeReleased()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
}

void AVehicleRVOTestPawn::OnToggleCamera()
{
	EnableIncarView(!bInCarCameraActive);
}

void AVehicleRVOTestPawn::EnableIncarView(const bool bState, const bool bForce)
{
	if ((bState != bInCarCameraActive) || ( bForce == true ))
	{
		bInCarCameraActive = bState;
		
		if (bState == true)
		{
			OnResetVR();
			Camera->Deactivate();
			InternalCamera->Activate();
		}
		else
		{
			InternalCamera->Deactivate();
			Camera->Activate();
		}
		
		InCarSpeed->SetVisibility(bInCarCameraActive);
		InCarGear->SetVisibility(bInCarCameraActive);
	}
}


void AVehicleRVOTestPawn::Tick(float Delta)
{
	Super::Tick(Delta);

	// Setup the flag to say we are in reverse gear
	bInReverseGear = GetVehicleMovement()->GetCurrentGear() < 0;
	
	// Update the strings used in the hud (incar and onscreen)
	UpdateHUDStrings();

	// Set the string in the incar hud
	SetupInCarHUD();

	bool bHMDActive = false;
#if HMD_MODULE_INCLUDED
	if ((GEngine->HMDDevice.IsValid() == true) && ((GEngine->HMDDevice->IsHeadTrackingAllowed() == true) || (GEngine->IsStereoscopic3D() == true)))
	{
		bHMDActive = true;
	}
#endif // HMD_MODULE_INCLUDED
	if (bHMDActive == false)
	{
		if ( (InputComponent) && (bInCarCameraActive == true ))
		{
			FRotator HeadRotation = InternalCamera->RelativeRotation;
			HeadRotation.Pitch += InputComponent->GetAxisValue(LookUpBinding);
			HeadRotation.Yaw += InputComponent->GetAxisValue(LookRightBinding);
			InternalCamera->RelativeRotation = HeadRotation;
		}
	}

	// Auto Drive and Avoidance
	if (bEnableAutoDrive)
	{
		UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
		AvoidanceManager->AvoidanceDebugForAll(true);
		AvoidanceManager->LockTimeAfterClean = 0.001; // default of 0.01 causes problems because my framerate is so fast my vectors are always behind.

		float SteeringValue = 0.0f;

		if (SplinePath != nullptr)
		{
			USplineComponent *SplineComponent = SplinePath->SplineComponent;
			if (SplineComponent != nullptr)
			{
				float LookAheadDistance = 1000.0f;

				FVector VehicleLocation = GetActorLocation();
				FVector ClosestLocationOnSpline = SplineComponent->FindLocationClosestToWorldLocation(VehicleLocation, ESplineCoordinateSpace::Type::World);
				
				float inputKeyClosest = SplineComponent->FindInputKeyClosestToWorldLocation(ClosestLocationOnSpline);
				FVector ClosestToInputKey = SplineComponent->GetLocationAtSplineInputKey(inputKeyClosest, ESplineCoordinateSpace::Type::World);
			
				int32 ClosestPointIndex = SplinePath->GetClosestPointIndexGivenWorldLocation(VehicleLocation);
				if (ClosestPointIndex != INDEX_NONE)
				{
					FVector LocationAtClosePoint = SplineComponent->GetLocationAtSplinePoint(ClosestPointIndex, ESplineCoordinateSpace::Type::World);
					DrawDebugSphere(GWorld, LocationAtClosePoint, 100, 10, FColor::Green);

					
				}

				DrawDebugSphere(GWorld, ClosestLocationOnSpline, 100, 10, FColor::Yellow);
				DrawDebugSphere(GWorld, ClosestToInputKey, 200, 10, FColor::Blue);
			}
		}

		if (SteeringTarget != FVector::ZeroVector)
		{
			FVector VehicleDirection = GetActorRotation().Vector();
			FVector VehicleLocation = GetActorLocation();
			FVector SteeringTargetLocation = SteeringTarget;// SteeringTarget->GetActorLocation();
			FVector SteeringDirection = (SteeringTargetLocation - VehicleLocation).GetSafeNormal();
			float angleDiff = FMath::RadiansToDegrees(SteeringDirection.HeadingAngle() - VehicleDirection.HeadingAngle());
			angleDiff = FMath::UnwindDegrees(angleDiff);

			// Lerp from 0-90 to 0-0.5;
			const float maxSteeringAmount = 1.0f;// 0.5f;
			const float maxAngleAmouont = 10.0f;// 90.0f;
			SteeringValue = FMath::Clamp((maxSteeringAmount / maxAngleAmouont)*angleDiff, -maxSteeringAmount, maxSteeringAmount);


			GetVehicleMovementComponent()->SetSteeringInput(SteeringValue);

			FVector debugStringLoc = VehicleLocation;
			debugStringLoc.Z += 250;
			FString debugSteering = FString::Printf(TEXT("AngleDiff: %f SteeringVal: %f"), angleDiff, SteeringValue);
			DrawDebugString(GWorld, debugStringLoc, debugSteering, NULL, FColor::White, 0);
			DrawDebugSphere(GWorld, SteeringTargetLocation, 100, 10, FColor::Red);
		}

//		GetVehicleMovementComponent()->SetSteeringInput(1.0f);
///		GetVehicleMovementComponent()->SetThrottleInput(1.0f);
	}
}

void AVehicleRVOTestPawn::BeginPlay()
{
	Super::BeginPlay();

	bool bEnableInCar = false;
#if HMD_MODULE_INCLUDED
	bEnableInCar = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
#endif // HMD_MODULE_INCLUDED
	EnableIncarView(bEnableInCar,true);
}

void AVehicleRVOTestPawn::OnResetVR()
{
#if HMD_MODULE_INCLUDED
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->ResetOrientationAndPosition();
		InternalCamera->SetRelativeLocation(InternalCameraOrigin);
		GetController()->SetControlRotation(FRotator());
	}
#endif // HMD_MODULE_INCLUDED
}

void AVehicleRVOTestPawn::UpdateHUDStrings()
{
	float KPH = FMath::Abs(GetVehicleMovement()->GetForwardSpeed()) * 0.036f;
	int32 KPH_int = FMath::FloorToInt(KPH);

	// Using FText because this is display text that should be localizable
	SpeedDisplayString = FText::Format(LOCTEXT("SpeedFormat", "{0} km/h"), FText::AsNumber(KPH_int));
	
	if (bInReverseGear == true)
	{
		GearDisplayString = FText(LOCTEXT("ReverseGear", "R"));
	}
	else
	{
		int32 Gear = GetVehicleMovement()->GetCurrentGear();
		GearDisplayString = (Gear == 0) ? LOCTEXT("N", "N") : FText::AsNumber(Gear);
	}	
}

void AVehicleRVOTestPawn::SetupInCarHUD()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if ((PlayerController != nullptr) && (InCarSpeed != nullptr) && (InCarGear != nullptr) )
	{
		// Setup the text render component strings
		InCarSpeed->SetText(SpeedDisplayString);
		InCarGear->SetText(GearDisplayString);
		
		if (bInReverseGear == false)
		{
			InCarGear->SetTextRenderColor(GearDisplayColor);
		}
		else
		{
			InCarGear->SetTextRenderColor(GearDisplayReverseColor);
		}
	}
}

#undef LOCTEXT_NAMESPACE
