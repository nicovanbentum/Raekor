#include "pch.h"
#include "json.h"

namespace Raekor::JSON {

int SkipToken(Slice<jsmntok_t> inTokens, int inIdx) {
	const auto token_end = inTokens[inIdx].end;

	while (inTokens[inIdx].start < token_end) {
		inIdx++;

		if (inIdx >= inTokens.Length())
			return -1;
	}

	return inIdx;
}

} // raekor::json