#include <lsf/json.h>
#include "config.h"
#include "dns.h"
#include "httpserver.h"

#include <fstream>
#include <filesystem>

#include <spdlog/spdlog.h>

namespace hcpp
{

    std::shared_ptr<config> hcpp::config::get_config(std::string_view config_path)
    {
        static auto p = std::shared_ptr<config>(new config(config_path));
        return p;
    }

    hcpp::config::config(std::string_view config_path)
    {
        if (!std::filesystem::exists(config_path))
        {
            spdlog::warn("文件不存在: {} 跳过初始化,使用默认设置", config_path);
            return;
        }
        lsf::Json j;

        // 主配置文件
        auto res = j.run(std::make_unique<lsf::FileSource>(std::string(config_path)));
        if (!res)
        {
            spdlog::error("{} : {}", config_path, j.get_errors());
            spdlog::error("{} : {}", config_path, j.get_errors());
            throw std::runtime_error("解析config出错");
        }
        lsf::json_to_struct_ignore_absence(*res, cs_);

        // 主机映射配置文件
        if (!cs_.host_mapping_path_.empty())
        {
            auto res = j.run(std::make_unique<lsf::FileSource>(cs_.host_mapping_path_));
            if (!res)
            {
                spdlog::error("{} : {}", cs_.host_mapping_path_, j.get_errors());
                throw std::runtime_error("解析host_mapping出错");
            }
            lsf::json_to_struct(*res, hm_);
        }

        // 代理服务配置文件
        if (!cs_.proxy_service_path_.empty())
        {
            auto res = j.run(std::make_unique<lsf::FileSource>(cs_.proxy_service_path_));
            if (!res)
            {
                spdlog::error("{} : {}", cs_.proxy_service_path_, j.get_errors());
                throw std::runtime_error("解析proxy_service出错");
            }
            decltype(cs_.proxy_service_) ps;
            lsf::json_to_struct(*res, ps);
            std::ranges::move(ps, std::back_inserter(cs_.proxy_service_));
        }
    }

    hcpp::config::~config()
    {
        try
        {
            hm_.clear();
            cs_ = {};

            for (auto &&i : save_callback_)
            {
                i(*this);
            }
            lsf::Json j;
            lsf::SerializeBuilder bu;

            if (!hm_.empty())
            {
                lsf::struct_to_jsonstr(hm_, bu);
                std::ofstream f(cs_.host_mapping_path_);
                if (!f.is_open())
                {
                    spdlog::error("打开{}文件失败", cs_.host_mapping_path_);
                    return;
                }
                else
                {
                    f << bu.get_jsonstring();
                }
            }
            bu.clear();
            spdlog::info("config保存成功");
        }
        catch (const std::exception &e)
        {
            spdlog::error("config析构异常: {}", e.what());
        }
    }

    bool config::config_to(std::shared_ptr<slow_dns> dns)
    {
        for (auto &&i : cs_.proxy_service_)
        {
            if (i.doh_)
            {
                dns->add_doh_filter(i.host_);
            }
        }
        dns->load_hm(hm_);
        dns->load_dp(cs_.dns_provider_);
        return true;
    }

    bool config::config_to(std::shared_ptr<mimt_https_server> mhs)
    {
        return config_to(*mhs);
    }

    bool config::config_to(mimt_https_server &mhs)
    {
        for (auto &&i : cs_.proxy_service_)
        {
            if (i.mitm_)
            {
                mhs.tunnel_set_.insert({i.host_, i.svc_});
            }
        }
        return true;
    }

    uint16_t config::get_port() const
    {
        if (cs_.port_ > 65535 || cs_.port_ < 0)
        {
            spdlog::warn("端口号有问题,将被不确定转换: {}", cs_.port_);
        }
        return cs_.port_;
    }

    void hcpp::config::save_callback(std::function<void(config &)> sc)
    {
        save_callback_.push_back(sc);
    }

} // namespace hcpp