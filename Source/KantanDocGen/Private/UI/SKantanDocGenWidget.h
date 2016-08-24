// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"


class SKantanDocGenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKantanDocGenWidget)
	{}

		//SLATE_ARGUMENT(TWeakObjectPtr< UKeyStepEditComponent >, Comp)
		//SLATE_EVENT(FOnElementSelected, OnElementSelected)

	SLATE_END_ARGS()

	void Construct(const SKantanDocGenWidget::FArguments& InArgs);

protected:
	bool ValidateSettingsForGeneration() const;
	FReply OnGenerateDocs();

protected:
	
};


