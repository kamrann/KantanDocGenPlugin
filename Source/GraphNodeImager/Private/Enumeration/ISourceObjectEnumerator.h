// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once


class ISourceObjectEnumerator
{
public:
	virtual UObject* GetNext() = 0;
	virtual float EstimateProgress() const = 0;
	virtual int32 EstimatedSize() const = 0;
};


