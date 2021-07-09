// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DocGenCommandlet.generated.h"

/**
 * 
 */
UCLASS()
class UDocGenCommandlet : public UCommandlet
{
	GENERATED_BODY()
	
	UDocGenCommandlet();

 int32 Main(const FString& Params) override;
 virtual void CreateCustomEngine(const FString& Params) override;
 TArray<UClass*> GetAllOutputFormatFactories();
};
