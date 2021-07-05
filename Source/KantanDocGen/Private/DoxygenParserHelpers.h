#pragma once
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"

namespace Detail
{
	TMap<FString, TArray<FString>> ParseDoxygenTagsForString(const FString& RawDoxygenString)
	{
		TMap<FString, TArray<FString>> ParsedTags;
		int32 CurStrPos = 0;
		int32 RawStringLength = RawDoxygenString.Len();
		// parse the full function tooltip text, looking for tag lines
		do
		{
			CurStrPos = RawDoxygenString.Find("@", ESearchCase::IgnoreCase, ESearchDir::FromStart, CurStrPos);
			if (CurStrPos == INDEX_NONE) // if the tag wasn't found
			{
				break;
			}
			CurStrPos++;
			FString CurrentTagString;
			while (CurStrPos < RawStringLength && !FChar::IsWhitespace(RawDoxygenString[CurStrPos]))
			{
				CurrentTagString.AppendChar(RawDoxygenString[CurStrPos]);
				CurStrPos++;
			}

			// advance past whitespace
			while (CurStrPos < RawStringLength && FChar::IsWhitespace(RawDoxygenString[CurStrPos]))
			{
				++CurStrPos;
			}

			// advance past whitespace (get to the meat of the comment)
			// since many doxygen style @param use the format "@param <param name> - <comment>" we also strip - if it is
			// before we get to any other non-whitespace
			while (CurStrPos < RawStringLength &&
				   (FChar::IsWhitespace(RawDoxygenString[CurStrPos])))
			{
				++CurStrPos;
			}

			FString CurrentValueString;
			// collect the param/return-val description
			while (CurStrPos < RawStringLength && RawDoxygenString[CurStrPos] != TEXT('@'))
			{
				// advance past newline
				while (CurStrPos < RawStringLength && FChar::IsLinebreak(RawDoxygenString[CurStrPos]))
				{
					++CurStrPos;

					// advance past whitespace at the start of a new line
					while (CurStrPos < RawStringLength && FChar::IsWhitespace(RawDoxygenString[CurStrPos]))
					{
						++CurStrPos;
					}

					// replace the newline with a single space
					if (CurStrPos < RawStringLength && !FChar::IsLinebreak(RawDoxygenString[CurStrPos]))
					{
						CurrentValueString.AppendChar(TEXT(' '));
					}

					if (CurStrPos < RawStringLength && RawDoxygenString[CurStrPos] == TEXT('*'))
					{
						++CurStrPos;
						// skip whitespace after an asterisk in a multiline comment
						while (CurStrPos < RawStringLength && FChar::IsWhitespace(RawDoxygenString[CurStrPos]))
						{
							++CurStrPos;
						}
					}
				}

				if (CurStrPos < RawStringLength && RawDoxygenString[CurStrPos] != TEXT('@'))
				{
					CurrentValueString.AppendChar(RawDoxygenString[CurStrPos++]);
				}
			}

			// trim any trailing whitespace from the descriptive text
			CurrentValueString.TrimEndInline();
			CurrentValueString.RemoveFromEnd("/");
			CurrentValueString.RemoveFromEnd("*");
			CurrentValueString.TrimEndInline();

			TArray<FString>& ExistingValuesForCurrentTag = ParsedTags.FindOrAdd(CurrentTagString);
			ExistingValuesForCurrentTag.Add(CurrentValueString);

		} while (CurStrPos < RawStringLength);

		return ParsedTags;
	}
} // namespace Detail