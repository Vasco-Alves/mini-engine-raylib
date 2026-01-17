#include "Registry.hpp"

namespace me::detail {

	static Registry gReg;      // single engine-owned instance

	Registry& Reg() {
		return gReg;
	}

} // namespace me::detail
