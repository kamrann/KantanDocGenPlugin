// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class UObject;

class ISourceObjectEnumerator
{
public:
	virtual UObject* GetNext() = 0;
	virtual float EstimateProgress() const = 0;
	virtual int32 EstimatedSize() const = 0;

	virtual ~ISourceObjectEnumerator() {}
};


