#include "config.h"
#include "dns.h"
#include "httpserver.h"
#include "certificate/cert.h"

#include <fstream>
#include <filesystem>
#include <tuple>

#include <spdlog/spdlog.h>

import lsf;

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

        subject_identify si;

        auto si_tu = std::forward_as_tuple(cs_.ca_pkey_path_, cs_.ca_cert_path_);

        // XXX 注意,这些目录不能是~开头的
        auto replace_path = [](auto &&i)
        {
            if (i.starts_with("~"))
            {
                auto path_prefix = std::getenv("HOME");
                if (path_prefix != nullptr)
                {
                    i = path_prefix + i.substr(1);
                }else{
                    log::warn("没有HOME系统变量,不处理 ~");
                }
            }
        };

        std::apply([replace_path](auto &&...argv)
                   { ((replace_path(argv), ...)); },
                   si_tu);

        if (cs_.ca_pkey_path_.empty() || cs_.ca_cert_path_.empty())
        { // 必须传递路径,否则生成的无法保存
            log::error("未找到配置:ca_pkey_path_或ca_cert_path_");
            return false;
        }
        else
        {
            if (std::filesystem::exists(cs_.ca_pkey_path_) && std::filesystem::exists(cs_.ca_cert_path_))
            { // 加载
                // std::apply([&si](auto &&...argv)
                //            { (([&si](auto &i)
                //                {
                //                    std::ifstream ifs(i);
                //                    if (ifs.is_open())
                //                    {
                //                        char buff[1024];
                //                        while (ifs)
                //                        {
                //                            auto rn = ifs.read(buff, sizeof(buff)).gcount();
                //                            si.pkey_pem_.append(buff, rn);
                //                        }
                //                        ifs.close();
                //                    }
                //                    else
                //                    {
                //                        log::error("config::config_to: 加载文件 {} 失败", i);
                //                        return false;
                //                    }
                //                }(argv)&&
                //                ...)); },
                //            si_tu);
                std::ifstream ifs(cs_.ca_pkey_path_);
                if (ifs.is_open())
                {
                    char buff[1024];
                    while (ifs)
                    {
                        auto rn = ifs.read(buff, sizeof(buff)).gcount();
                        si.pkey_pem_.append(buff, rn);
                    }
                    ifs.close();
                }
                else
                {
                    log::error("config::config_to: 加载文件 {} 失败", cs_.ca_pkey_path_);
                    return false;
                }

                std::ifstream ifs2(cs_.ca_cert_path_);
                if (ifs2.is_open())
                {
                    char buff[1024];
                    while (!ifs2.eof())
                    {
                        auto rn = ifs2.read(buff, sizeof(buff)).gcount();
                        si.cert_pem_.append(buff, rn);
                    }
                    ifs2.close();
                }
                else
                {
                    log::error("config::config_to: 加载文件 {} 失败", cs_.ca_cert_path_);
                    return false;
                }
            }
            else
            { // 重新生成
                auto cert = make_x509();
                auto pkey = make_pkey();
                set_version(cert);
                set_serialNumber(cert);
                set_validity(cert);
                set_issuer(cert);
                set_subject(cert);
                // add_SAN(cert);
                add_ca_BS(cert);
                add_AKI(cert);
                add_SKI(cert);
                add_ca_key_usage(cert);
                set_pubkey(cert, pkey);
                sign(cert, pkey);
                si.pkey_pem_ = make_pem_str(pkey);
                si.cert_pem_ = make_pem_str(cert);

                std::filesystem::path p1(cs_.ca_pkey_path_);
                std::filesystem::create_directories(p1.parent_path());
                std::ofstream ofs(cs_.ca_pkey_path_);
                if (ofs.is_open())
                {
                    ofs << si.pkey_pem_;
                    ofs.close();
                    log::warn("config::config_to: 保存{}成功",cs_.ca_pkey_path_);
                }
                else
                {
                    log::error("config::config_to: 打开 {} 失败", cs_.ca_pkey_path_);
                    return false;
                }

                std::filesystem::path p2(cs_.ca_cert_path_);
                std::filesystem::create_directories(p2.parent_path());
                std::ofstream ofs2(cs_.ca_cert_path_);
                if (ofs2.is_open())
                {
                    ofs2 << si.cert_pem_;
                    ofs2.close();
                    log::warn("config::config_to: 保存{}成功",cs_.ca_cert_path_);
                }
                else
                {
                    log::error("config::config_to: 打开 {} 失败", cs_.ca_cert_path_);
                    return false;
                }
            }
        }
        mhs.set_ca(std::move(si));
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

    std::vector<proxy_service> config::get_proxy_service() const
    {
        return cs_.proxy_service_;
    }

    void hcpp::config::save_callback(std::function<void(config &)> sc)
    {
        save_callback_.push_back(sc);
    }

} // namespace hcpp