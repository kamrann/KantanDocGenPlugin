#pragma once
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/EngineVersionComparison.h"
#if UE_VERSION_NEWER_THAN (4, 25, 0)
	#include "Misc/TVariant.h"
namespace DocGen
{
	template<typename T, typename... Ts>
	using Variant = TVariant<T,Ts...>;
}
#else
	#include "mpark/variant.h"
namespace DocGen
{
	template<typename... Ts>
	using Variant = mpark::variant;
}
#endif
