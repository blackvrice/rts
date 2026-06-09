// Dedicated translation unit so the compile-time self-checks (static_assert) in
// the header are evaluated by the build. The fixed-point types are header-only;
// this file intentionally defines no symbols.
#include "core/sim/Fixed.hpp"
