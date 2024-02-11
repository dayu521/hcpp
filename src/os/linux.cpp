#include "common.h"

namespace hcpp
{
    void set_platform_default_verify_store(ssl::context& ctx)
    {
        // ctx_->add_verify_path(X509_get_default_cert_dir());
        ctx.set_default_verify_paths();
    }
} // namespace hcpp
