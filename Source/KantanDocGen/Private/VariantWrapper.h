#pragma once
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 25
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
