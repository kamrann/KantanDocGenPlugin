// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

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
				//std::forward< TArgs >(Args)...);
		};

		FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(NullaryFunc, TStatId(), nullptr, ENamedThreads::GameThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);

		return Result;
	}

}

