// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "SplinePath.generated.h"

UCLASS()
class VEHICLERVOTEST_API ASplinePath : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASplinePath();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;


	UPROPERTY(Category = SplineComponent, EditAnywhere, BlueprintReadWrite)
	class USplineComponent *SplineComponent;
	

	int32 GetClosestPointIndexGivenWorldLocation(const FVector &WorldLocation);
private:
	
};
