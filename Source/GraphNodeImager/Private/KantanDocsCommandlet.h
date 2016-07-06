// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "Classes/Commandlets/Commandlet.h"
#include "KantanDocsCommandlet.generated.h"


UCLASS()
class UKantanDocsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(FString const& Params) override;
};


