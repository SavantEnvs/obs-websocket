#include "fuzz_session.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	return RunSession(data, size, SessionKind::IdentifiedMsgPack);
}
