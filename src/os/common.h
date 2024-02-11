#ifndef SRC_OS_COMMON
#define SRC_OS_COMMON

#include <asio/ssl.hpp>

namespace hcpp
{
    using namespace asio;

    void set_platform_default_verify_store(ssl::context& ctx);
} // namespace hcpp

#endif /* SRC_OS_COMMON */
