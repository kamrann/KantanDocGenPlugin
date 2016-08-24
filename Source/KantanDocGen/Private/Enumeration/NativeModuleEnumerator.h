// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "ISourceObjectEnumerator.h"


class FNativeModuleEnumerator: public ISourceObjectEnumerator
{
public:
	FNativeModuleEnumerator(
		FName const& InModuleName
	);

public:
	virtual UObject* GetNext() override;
	virtual float EstimateProgress() const override;
	virtual int32 EstimatedSize() const override;

protected:
	void Prepass(FName const& ModuleName);

protected:
	TArray< TWeakObjectPtr< UObject > > ObjectList;
	int32 CurIndex;
};


