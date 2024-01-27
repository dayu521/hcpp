/*
 * This file is part of harddns.
 *
 * (C) 2016-2019 by Sebastian Krahmer,
 *                  sebastian [dot] krahmer [at] gmail [dot] com
 *
 * harddns is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * harddns is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with harddns. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SRC_DNS_DNSHTTPS
#define SRC_DNS_DNSHTTPS

#ifndef harddns_https_h
#define harddns_https_h

#include "asio_coroutine_net.h"
#include "hmemory.h"

#include <stdint.h>
#include <string>
#include <map>
#include <memory>

namespace harddns
{
	using std::string;
	using namespace asio;
	using tcp_socket = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;

	string make_query(const string &name, uint16_t qtype);

	class dnshttps
	{

	public:
		struct answer_t
		{
			std::string name;
			uint16_t qtype, qclass;
			uint32_t ttl;
			std::string rdata;
		};

		// we need it ordered
		using dns_reply = std::map<unsigned int, answer_t>;

	public:
		static int parse_rfc8484(const std::string &, uint16_t, dns_reply &, std::string &, const std::string &, std::string::size_type, size_t);

		int parse_json(const std::string &, uint16_t, dns_reply &, std::string &, const std::string &, std::string::size_type, size_t);

	public:
		dnshttps(std::shared_ptr<hcpp::memory> mem,std::string dns_host):mem_(mem),dns_host_(dns_host)
		{
		}

		virtual ~dnshttps()
		{
			// don't delete ssl
		}

		awaitable<int> get(const std::string &, uint16_t, dns_reply &, std::string &);

	private:
		std::shared_ptr<hcpp::memory> mem_;
		std::string dns_host_;
	};

}

#endif

#endif /* SRC_DNS_DNSHTTPS */
