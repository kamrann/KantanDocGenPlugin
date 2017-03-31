// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"


class SKantanDocGenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKantanDocGenWidget)
	{}

	SLATE_END_ARGS()

	void Construct(const SKantanDocGenWidget::FArguments& InArgs);

protected:
	bool ValidateSettingsForGeneration() const;
	FReply OnGenerateDocs();

protected:
	
};


