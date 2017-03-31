// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"


namespace DocGenThreads
{

	template < typename TLambda >
	inline auto RunOnGameThread(TLambda Func) -> void
	{
		FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(Func, TStatId(), nullptr, ENamedThreads::GameThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
	}

	template < typename TLambda, typename... TArgs >
	inline auto RunOnGameThreadRetVal(TLambda Func, TArgs&... Args) -> decltype(Func(Args...))
	{
		typedef decltype(Func(Args...)) TResult;

		TResult Result;
		auto NullaryFunc = [&]
		{
			Result = Func(Args...);
		};

		FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(NullaryFunc, TStatId(), nullptr, ENamedThreads::GameThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);

		return Result;
	}

}

