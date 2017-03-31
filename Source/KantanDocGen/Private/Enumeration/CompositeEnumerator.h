// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "ISourceObjectEnumerator.h"


template < typename TChildEnum >
class FCompositeEnumerator: public ISourceObjectEnumerator
{
public:
	FCompositeEnumerator(
		TArray< FName > const& InNames
	)
	{
		CurEnumIndex = 0;
		TotalSize = 0;
		Completed = 0;

		Prepass(InNames);
	}

public:
	virtual UObject* GetNext() override
	{
		while(CurEnumIndex < ChildEnumList.Num())
		{
			if(auto Obj = ChildEnumList[CurEnumIndex]->GetNext())
			{
				return Obj;
			}
			else
			{
				Completed += ChildEnumList[CurEnumIndex]->EstimatedSize();
				ChildEnumList[CurEnumIndex].Reset();
				++CurEnumIndex;
				continue;
			}
		}

		return nullptr;
	}

	virtual float EstimateProgress() const override
	{
		if(CurEnumIndex < ChildEnumList.Num())
		{
			return (float)(Completed + ChildEnumList[CurEnumIndex]->EstimateProgress() * ChildEnumList[CurEnumIndex]->EstimatedSize()) / TotalSize;
		}
		else
		{
			return 1.0f;
		}
	}

	virtual int32 EstimatedSize() const override
	{
		return TotalSize;
	}

protected:
	void Prepass(TArray< FName > const& Names)
	{
		for(auto Name : Names)
		{
			auto Child = MakeUnique< TChildEnum >(Name);
			TotalSize += Child->EstimatedSize();

			ChildEnumList.Add(MoveTemp(Child));
		}
	}

protected:
	TArray< TUniquePtr< ISourceObjectEnumerator > > ChildEnumList;
	int32 CurEnumIndex;
	int32 TotalSize;
	int32 Completed;
};


