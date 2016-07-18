// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "ISourceObjectEnumerator.h"


class FContentPathEnumerator: public ISourceObjectEnumerator
{
public:
	FContentPathEnumerator(
		FName const& InPath
	);

public:
	virtual UObject* GetNext() override;
	virtual float EstimateProgress() const override;
	virtual int32 EstimatedSize() const override;

protected:
	void Prepass(FName const& Path);

protected:
	TArray< FAssetData > AssetList;
	int32 CurIndex;
};


