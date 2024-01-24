/*
 * This file is part of harddns.
 *
 * (C) 2016-2023 by Sebastian Krahmer,
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

#include <cstring>
#include <limits>
#include <string>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <map>
#include <chrono>
#include <bit>

#include "dnshttps.h"
#include "net-headers.h"

// #include <arpa/inet.h>

#include <asio.hpp>

/// @brief
namespace harddns
{

	using namespace std;
	using namespace net_headers;
	namespace // misc.h
	{
		int host2qname(const std::string &, std::string &);

		int qname2host(const std::string &, std::string &, std::string::size_type idx = 0);

		bool valid_name(const std::string &);

		string lcs(const string &s)
		{
			string rs = s;
			transform(rs.begin(), rs.end(), rs.begin(), [](unsigned char c)
					  { return tolower(c); });
			return rs;
		}

		const uint8_t dns_max_label = 63;

		/* "foo.bar" -> "\003foo\003bar\000"
		 * "foo.bar." -> "\003foo\003bar\000"
		 * automatically splits labels larger than 63 byte into
		 * sub-domains
		 */
		int host2qname(const string &host, string &result)
		{
			string split_host = "";
			string::size_type pos1 = 0, pos2 = 0;

			result = "";

			for (; pos1 < host.size();)
			{
				pos2 = host.find(".", pos1);
				if (pos2 == string::npos)
				{
					split_host += host.substr(pos1);
					break;
				}

				if (pos2 - pos1 > dns_max_label)
				{
					split_host += host.substr(pos1, dns_max_label);
					pos1 += dns_max_label;
				}
				else
				{
					split_host += host.substr(pos1, pos2 - pos1);
					pos1 = pos2 + 1;
				}

				split_host += ".";
			}

			if (split_host.size() >= 2048)
				return -1;

			char buf[4096] = {0};
			memcpy(buf + 1, split_host.c_str(), split_host.size());

			// now, substitute dots by cnt
			string::size_type last_dot = 0;
			for (; last_dot < split_host.size();)
			{
				uint8_t i = 0;
				while (buf[last_dot + i] != '.' && buf[last_dot + i] != 0)
					++i;
				buf[last_dot] = i - 1;
				last_dot += i;

				// end of string without trailing "." ?
				if (buf[last_dot] == 0)
					break;
				// end with trailing "." ?
				if (buf[last_dot + 1] == 0)
				{
					buf[last_dot] = 0;
					break;
				}
			}

			result = string(buf, last_dot + 1);
			return last_dot + 1;
		}
		bool valid_name(const string &name)
		{
			size_t l = name.size();
			if (l > 254 || l < 2)
				return 0;

			for (size_t i = 0; i < l; ++i)
			{
				if (name[i] >= '0' && name[i] <= '9')
					continue;
				if (name[i] >= 'a' && name[i] <= 'z')
					continue;
				if (name[i] >= 'A' && name[i] <= 'Z')
					continue;
				if (name[i] == '-' || name[i] == '.')
					continue;

				return 0;
			}

			return 1;
		}
		/*  "\003foo\003bar\000" -> foo.bar.
		 */
		int qname2host(const string &msg, string &result, string::size_type start_idx)
		{
			string::size_type i = start_idx, r = 0;
			uint8_t len = 0, compress_depth = 0;

			result = "";
			string s = "";
			try
			{
				s.reserve(msg.length());
			}
			catch (...)
			{
				return -1;
			}

			while ((len = msg[i]) != 0)
			{
				if (len > dns_max_label)
				{
					// start_idx of 0 means we just have a qname string, not an entire DNS packet,
					// so we cant uncompress compressed labels
					if (start_idx == 0 || ++compress_depth > 10)
						return -1;
					// compressed?
					if (len & 0xc0)
					{
						if (i + 1 >= msg.size())
							return -1;
						i = msg[i + 1] & 0xff;
						if (i < 0 || i >= msg.size())
							return -1;
						// actually += 2, but the return will add 1
						// only for the first compression
						if (compress_depth <= 1)
							r += 1;
						continue;
					}
					else
						return -1;
				}
				if (len + i + 1 > msg.size())
					return -1;
				s += msg.substr(i + 1, len);
				s += ".";

				i += len + 1;

				if (compress_depth == 0)
					r += len + 1;
			}

			result = s;
			if (result.size() == 0)
				return 0;

			// RFC1035
			if (result.size() > 255)
			{
				result = "";
				return -1;
			}

			return r + 1;
		}
	}
	namespace // base64.h
	{

		std::string &b64url_encode(const std::string &, std::string &);

		std::string &b64url_encode(const char *, size_t, std::string &);

		static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

		using namespace std;

		/* The base64 routines have been taken from the Samba 3 source (GPL)
		 * and have been C++-ified
		 */

		string &b64url_encode(const string &src, string &dst)
		{
			unsigned int bits = 0;
			int char_count = 0, i = 0;

			dst = "";
			if (src.size() >= numeric_limits<unsigned int>::max() / 2)
				return dst;

			dst.reserve(src.size() + src.size() / 3 + 10);
			string::size_type len = src.size();
			while (len--)
			{
				unsigned int c = (unsigned char)src[i++];
				bits += c;
				char_count++;
				if (char_count == 3)
				{
					dst += b64[bits >> 18];
					dst += b64[(bits >> 12) & 0x3f];
					dst += b64[(bits >> 6) & 0x3f];
					dst += b64[bits & 0x3f];
					bits = 0;
					char_count = 0;
				}
				else
				{
					bits <<= 8;
				}
			}
			if (char_count != 0)
			{
				bits <<= 16 - (8 * char_count);
				dst += b64[bits >> 18];
				dst += b64[(bits >> 12) & 0x3f];
				if (char_count == 1)
				{
					// dst += '=';
					// dst += '=';
				}
				else
				{
					dst += b64[(bits >> 6) & 0x3f];
					// dst += '=';
				}
			}
			return dst;
		}

		string &b64url_encode(const char *src, size_t srclen, string &dst)
		{
			unsigned int bits = 0;
			int char_count = 0, i = 0;

			dst = "";
			if (srclen >= numeric_limits<unsigned int>::max() / 2)
				return dst;

			dst.reserve(srclen + srclen / 3 + 10);
			while (srclen--)
			{
				unsigned int c = (unsigned char)src[i++];
				bits += c;
				char_count++;
				if (char_count == 3)
				{
					dst += b64[bits >> 18];
					dst += b64[(bits >> 12) & 0x3f];
					dst += b64[(bits >> 6) & 0x3f];
					dst += b64[bits & 0x3f];
					bits = 0;
					char_count = 0;
				}
				else
				{
					bits <<= 8;
				}
			}
			if (char_count != 0)
			{
				bits <<= 16 - (8 * char_count);
				dst += b64[bits >> 18];
				dst += b64[(bits >> 12) & 0x3f];

				if (char_count == 1)
				{
					// dst += '=';
					// dst += '=';
				}
				else
				{
					dst += b64[(bits >> 6) & 0x3f];
					// dst += '=';
				}
			}
			return dst;
		}

	}

	namespace
	{
		inline uint16_t htons(uint16_t hostshort)
		{
			if constexpr (std::endian::native == std::endian::little)
			{
				return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
			}
			return hostshort;
		}
	}

	// construct a DNS query for rfc8484
	string make_query(const string &name, uint16_t qtype)
	{
		auto tv2 = std::chrono::system_clock::now().time_since_epoch().count();

		string dns_query = "", qname = "", b64query = "";

		uint16_t qclass = htons(1);

		// https://www.rfc-editor.org/rfc/rfc1035#section-4.1.1
		dnshdr qhdr;
		qhdr.q_count = htons(1);
		qhdr.qr = 0;
		qhdr.rd = 1;
		qhdr.id = tv2 % 0xffff;

		host2qname(name, qname);
		if (!qname.size())
			return b64query;
		dns_query = string(reinterpret_cast<char *>(&qhdr), sizeof(qhdr));
		// https://www.rfc-editor.org/rfc/rfc1035#section-4.1.2
		dns_query += qname;
		dns_query += string(reinterpret_cast<char *>(&qtype), sizeof(uint16_t));
		dns_query += string(reinterpret_cast<char *>(&qclass), sizeof(uint16_t));

		b64url_encode(dns_query, b64query);
		return b64query;
	}

	// https://developers.google.com/speed/public-dns/docs/dns-over-https
	// https://developers.cloudflare.com/1.1.1.1/dns-over-https/
	// https://www.quad9.net/doh-quad9-dns-servers
	// https://tools.ietf.org/html/rfc8484

	awaitable<int> dnshttps::get(const string &name, uint16_t qtype, dns_reply &result, string &raw)
	{
		// don't:
		// result.clear();
		raw = "";

		if (!valid_name(name))
			co_return -1;

		// printf(">>>> %s %s %s %s\n", cfg->second.ip.c_str(), cfg->second.get.c_str(), cfg->second.host.c_str(), cfg->second.cn.c_str());

		string req = "GET /dns-query?dns=", reply = "", tmp;

		if (true)
		{
			string b64 = make_query(name, qtype);
			if (!b64.size())
				co_return -1;
			req += b64;
		}
		else
		{
			req += name;
			// qtype值 https://www.rfc-editor.org/rfc/rfc1035#section-3.2.3
			// https://www.rfc-editor.org/rfc/rfc1035#section-3.2.2
			if (qtype == htons(dns_type::A))
				req += "&type=A";
			// ip6扩展 https://www.rfc-editor.org/rfc/rfc3596
			else if (qtype == htons(dns_type::AAAA))
				req += "&type=AAAA";
			else if (qtype == htons(dns_type::NS))
				req += "&type=NS";
			else if (qtype == htons(dns_type::MX))
				req += "&type=MX";
			else
				co_return -1;
		}

		req += " HTTP/1.1\r\nHost: " + dns_host_;
		req += "\r\nUser-Agent: harddns 0.58 github.com/stealth/harddns\r\nConnection: Keep-Alive\r\n";

		if (true)
			req += "Accept: application/dns-message\r\n";
		else
			req += "Accept: application/dns-json\r\n";

		req += "\r\n\r\n";

		// printf(">>>> %s\n", req.c_str());

		// maybe closed due to error or not initialized in the first place

		std::cout << req << std::endl;

		auto n = co_await async_write(socket_, buffer(req));

		string::size_type idx = string::npos, content_idx = string::npos;
		size_t cl = 0;
		const int maxtries = 3;
		bool has_answer = 0;

		asio::streambuf bufff;

		auto fr = co_await async_read_until(socket_, bufff, "\r\n\r\n");

		tmp.resize(bufff.size());
		buffer_copy(buffer(tmp), bufff.data());
		bufff.consume(bufff.size());
		reply += tmp;
		if (reply.find("Transfer-Encoding: chunked\r\n") != string::npos && reply.find("\r\n0\r\n\r\n") != string::npos)
		{
			has_answer = 1;
		}

		// 找到消息体大小
		if (cl == 0 && (idx = reply.find("Content-Length:")) != string::npos)
		{
			idx += 15;
			cl = strtoul(reply.c_str() + idx, nullptr, 10);
			if (cl > 65535)
			{
				has_answer = 0;
			}
			else
			{
				has_answer = 1;
			}
		}

		if (!has_answer)
		{
			co_return -1;
		}
		auto tocl = cl - (reply.size() - fr);
		content_idx = fr;
		auto cli = 0;
		while (cli < tocl)
		{
			std::string bf2;
			bf2.resize(2048);
			auto bf2n = co_await socket_.async_read_some(buffer(bf2));
			reply += bf2.substr(0, bf2n);
			cli += bf2n;
		}

		if (reply.size() - fr == cl)
		{
			std::cout << "读取数量正确" << std::endl;
		}

		std::cout << reply << std::endl;

		auto ibb=0;
		for (auto &&i : reply.substr(fr))
			std::cout << (((unsigned int)i) & 255) << " "; //("<<ibb++<<")
		std::cout << std::endl;
		auto r = parse_rfc8484(name, qtype, result, raw, reply, content_idx, cl);
		// 	// else
		// 	// 	r = parse_json(name, qtype, result, raw, reply, content_idx, cl);

		// 	if (r >= 0)
		// 		co_return r;
		// 	continue;
		// }
		std::cout << "------" << std::endl;
		for (auto &&i : result)
		{
			auto [name, qtype, qclass, ttl, rdata] = i.second;
			std::cout << name << " ";
			// std::cout << qtype << " ";
			// std::cout << qclass << " ";
			std::cout << ttl << " ";
			std::cout << rdata << " " << std::endl;
		}

		co_return 0;
	}

	/// @brief
	/// @param name 	查询名字
	/// @param type
	/// @param result 	解析的结果放到这里
	/// @param raw 		原始消息也放到这里,暂时对于rfc8484没有使用
	/// @param reply 	消息
	/// @param content_idx 消息开始索引
	/// @param cl 	消息大小
	/// @return
	int dnshttps::parse_rfc8484(const string &name, uint16_t type, dns_reply &result, string &raw, const string &reply, string::size_type content_idx, size_t cl)
	{
		string dns_reply = "", tmp = "";
		string::size_type idx = string::npos, aidx = string::npos;
		bool has_answer = 0;
		unsigned int acnt = 0;

		if (cl > 0 && content_idx != string::npos)
		{
			dns_reply = reply.substr(content_idx);
			if (dns_reply.size() < cl)
				return -1;
		}
		else
		{
			// parse chunked encoding
			idx = reply.find("\r\n\r\n");
			if (idx == string::npos || idx + 4 >= reply.size())
				return -1;
			idx += 4;
			for (;;)
			{
				string::size_type nl = reply.find("\r\n", idx);
				if (nl == string::npos || nl + 2 > reply.size())
					return -1;
				cl = strtoul(reply.c_str() + idx, nullptr, 16);

				// end of chunk?
				if (cl == 0)
					break;

				if (cl > 65535 || nl + 2 + cl + 2 > reply.size())
					return -1;
				idx = nl + 2;
				dns_reply += reply.substr(idx, cl);
				idx += cl + 2;
			}
		}

		// For rfc8484, do not pass around the raw (binary) message, which would potentially
		// be used for logging. Unused by now.
		raw = "rfc8484 answer";

		if (dns_reply.size() < sizeof(dnshdr) + 5)
			return -1;

		const dnshdr *dhdr = reinterpret_cast<const dnshdr *>(dns_reply.c_str());

		// 表示响应
		if (dhdr->qr != 1)
			return -1;
		// 没有错误
		if (dhdr->rcode != 0)
			return -1;

		string aname = "", cname = "", fqdn = "";
		// 跳过head部分
		idx = sizeof(dnshdr);
		// 提取qname放到tmp
		int qnlen = qname2host(dns_reply, tmp, idx);
		// 测试问题部分
		if (qnlen <= 0 || idx + qnlen + 2 * sizeof(uint16_t) >= dns_reply.size())
			return -1;
		// 解析的qname
		fqdn = lcs(tmp);
		if (lcs(string(name + ".")) != fqdn)
			return -1;

		// 跳过问题部分
		idx += qnlen + 2 * sizeof(uint16_t);
		aidx = idx;

		uint16_t rdlen = 0, qtype = 0, qclass = 0;
		uint32_t ttl = 0;

		// first of all, find all CNAMEs for desired name
		map<string, int> fqdns{{fqdn, 1}};

		if (false)
		{
			// XXX 没有处理扩展部分 rfc6891
			for (int i = 0;; ++i)
			{
				if (idx >= dns_reply.size())
					break;

				// also handles compressed labels
				if ((qnlen = qname2host(dns_reply, tmp, idx)) <= 0)
					return -1;
				aname = lcs(tmp);

				// 10 -> qtype, qclass, ttl, rdlen
				if (idx + qnlen + 10 >= dns_reply.size())
					return -1;
				idx += qnlen;
				qtype = *reinterpret_cast<const uint16_t *>(dns_reply.c_str() + idx);
				idx += sizeof(uint16_t);
				qclass = *reinterpret_cast<const uint16_t *>(dns_reply.c_str() + idx);
				idx += sizeof(uint16_t);
				ttl = *reinterpret_cast<const uint32_t *>(dns_reply.c_str() + idx);
				idx += sizeof(uint32_t);
				rdlen = ntohs(*reinterpret_cast<const uint16_t *>(dns_reply.c_str() + idx));
				idx += sizeof(uint16_t);

				if (idx + rdlen > dns_reply.size() || qclass != htons(1) || rdlen == 0)
					return -1;

				if (qtype == htons(dns_type::CNAME))
				{
					if (qname2host(dns_reply, tmp, idx) <= 0)
						return -1;
					cname = lcs(tmp);

					if (fqdns.count(aname) > 0)
					{
						fqdns[cname] = 1;

						// For NSS module, to have fqdn aliases w/o decoding avail
						result[acnt++] = {"NSS CNAME", 0, 0, ntohl(ttl), cname};
					}
				}

				idx += rdlen;
			}
		}
		idx = aidx;
		for (int i = 0;; ++i)
		{
			if (idx >= dns_reply.size())
				break;

			// unlike in CNAME parsing loop, do not convert answer to lowercase,
			// as we want to put original name into answer
			if ((qnlen = qname2host(dns_reply, aname, idx)) < 0)
				return -1;

			// 10 -> qtype, qclass, ttl, rdlen
			if (idx + qnlen + 10 >= dns_reply.size())
				return -1;
			idx += qnlen;
			qtype = *reinterpret_cast<const uint16_t *>(dns_reply.c_str() + idx);
			idx += sizeof(uint16_t);
			qclass = *reinterpret_cast<const uint16_t *>(dns_reply.c_str() + idx);
			idx += sizeof(uint16_t);
			ttl = ntohl(*reinterpret_cast<const uint32_t *>(dns_reply.c_str() + idx));
			idx += sizeof(uint32_t);
			rdlen = ntohs(*reinterpret_cast<const uint16_t *>(dns_reply.c_str() + idx));
			idx += sizeof(uint16_t);

			if (idx + rdlen > dns_reply.size() || qclass != htons(1) || rdlen == 0)
				return -1;

			// Need to call host2qname() on orig embedded answer name,
			// because it may contain compression
			string qname = "";
			if (host2qname(aname, qname) <= 0)
				return -1;
			string_view sanas = aname;
			if (sanas.back() == '.')
				sanas.remove_suffix(1);
			answer_t dns_ans{std::string(sanas), qtype, qclass, ttl};

			if (qtype == htons(dns_type::A) && fqdns.count(lcs(aname)) > 0)
			{
				if (rdlen != 4)
					return -1;
				auto s = (char8_t *)dns_reply.substr(idx, 4).data();
				for (int i = 0; i < 4; i++)
				{
					dns_ans.rdata += std::to_string(s[i]) + ".";
				}
				dns_ans.rdata.pop_back();
				result[acnt++] = dns_ans;
				has_answer = 1;
			}
			else if (qtype == htons(dns_type::AAAA) && fqdns.count(lcs(aname)) > 0)
			{
				if (rdlen != 16)
					return -1;
				auto s = (uint16_t *)dns_reply.substr(idx, 16).data();
				for (int i = 0; i < 8; i++)
				{
					dns_ans.rdata += std::to_string(s[i]) + ":";
				}
				// dns_ans.rdata = dns_reply.substr(idx, 16);
				result[acnt++] = dns_ans;
				has_answer = 1;
			}
			else if (qtype == htons(dns_type::CNAME))
			{
				string qcname = "";
				// uncompress cname answer
				if (qname2host(dns_reply, cname, idx) <= 0)
					return -1;
				if (host2qname(cname, qcname) <= 0)
					return -1;
				dns_ans.rdata = qcname;
				result[acnt++] = dns_ans;
			}
			else if (qtype == htons(dns_type::NS) && qtype == type)
			{
				// XXX: handle decompression
				dns_ans.rdata = dns_reply.substr(idx, rdlen);
				result[acnt++] = dns_ans;
				has_answer = 1;
			}
			else if (qtype == htons(dns_type::MX) && qtype == type)
			{
				dns_ans.rdata = dns_reply.substr(idx, rdlen);
				result[acnt++] = dns_ans;
				has_answer = 1;
			}

			idx += rdlen;
		}

		return has_answer ? 1 : 0;
	}

	/*
			int dnshttps::parse_json(const string &name, uint16_t type, dns_reply &result, string &raw, const string &reply, string::size_type content_idx, size_t cl)
			{
				bool has_answer = 0;

				string::size_type idx = string::npos, idx2 = string::npos, aidx = string::npos;
				string json = "", tmp = "";
				unsigned int acnt = 0;

				if (cl > 0 && content_idx != string::npos) {
					json = reply.substr(content_idx);
					if (json.size() < cl)
						return build_error("Incomplete read from json server.", -1);
				} else {
					// parse chunked encoding
					idx = reply.find("\r\n\r\n");
					if (idx == string::npos || idx + 4 >= reply.size())
						return build_error("Invalid reply (1).", -1);
					idx += 4;
					for (;;) {
						string::size_type nl = reply.find("\r\n", idx);
						if (nl == string::npos || nl + 2 > reply.size())
							return build_error("Invalid reply.", -1);
						cl = strtoul(reply.c_str() + idx, nullptr, 16);

						// end of chunk?
						if (cl == 0)
							break;

						if (cl > 65535 || nl + 2 + cl + 2 > reply.size())
							return build_error("Invalid reply.", -1);
						idx = nl + 2;
						json += reply.substr(idx, cl);
						idx += cl + 2;
					}
				}

				raw = json;
				json = lcs(raw);

				//printf(">>>> %s @ %s\n", name.c_str(), raw.c_str());

				// Who needs boost property tree json parsers??
				// Turns out, C++ data structures were not really made for JSON. Maybe CORBA...
				json.erase(remove(json.begin(), json.end(), ' '), json.end());

				if (json.find("\"status\":0") == string::npos)
					return 0;
				if ((idx = json.find("\"answer\":[")) == string::npos)
					return 0;
				idx += 10;
				aidx = idx;

				string::size_type brace_open = 0, brace_close = 0;

				// first of all, find all CNAMEs
				string s = lcs(name);
				map<string, int> fqdns{{s, 1}};
				for (int level = 0; level < 10; ++level) {

					if (!valid_name(s))
						return build_error("Invalid DNS name.", -1);

					// Some servers remove the trailing "." in FQDNs in their answer,
					// and some add it -.-
					// So check for both versions of the answer FQDN (looking for cname answers)

					string cname = "\"name\":\"" + s;
					if (s[s.size() - 1] != '.')
						cname += ".";
					cname += "\",\"type\":5";
					if ((idx = json.find(cname, aidx)) == string::npos) {
						cname = "\"name\":\"" + s;
						if (cname[cname.size() - 1] == '.')
							cname.erase(cname.size() - 1, 1);
						cname += "\",\"type\":5";
						if ((idx = json.find(cname, aidx)) == string::npos)
							break;
					}
					if ((brace_close = json.find("}", idx)) == string::npos)
						return 0;
					if ((brace_open = json.rfind("{", idx)) == string::npos)
						return 0;
					if (brace_open < aidx || brace_close < aidx)
						return 0;

					// guaranteed that { comes before }

					string inner_json = json.substr(brace_open, brace_close - brace_open + 1);

					uint32_t ttl = 0;
					if ((idx = inner_json.find("\"ttl\":")) == string::npos)
						ttl = 600;
					else
						ttl = strtoul(inner_json.c_str() + idx + 6, nullptr, 10);

					if ((idx = inner_json.find("\"data\":\"")) == string::npos)
						break;
					idx += 8;
					if ((idx2 = inner_json.find("\"", idx)) == string::npos)
						break;
					tmp = inner_json.substr(idx, idx2 - idx);
					if (!valid_name(tmp))
						return build_error("Invalid DNS name.", -1);

					if (tmp[tmp.size() - 1] == '.')
						tmp.erase(tmp.size() - 1, 1);

					string qname = "", cqname = "";
					if (host2qname(s, qname) <= 0)
						break;
					if (host2qname(tmp, cqname) <= 0)
						break;

					result[acnt++] = {qname, htons(dns_type::CNAME), htons(1), htonl(ttl), cqname};

					// for NSS module, to have fqdn alias w/o decoding avail
					result[acnt++] = {"NSS CNAME", 0, 0, ttl, tmp};

					if (fqdns.count(s) > 0)
						fqdns[tmp] = 1;

					//syslog(LOG_INFO, ">>>> CNAME %s -> %s\n", s.c_str(), tmp.c_str());
					s = tmp;

					// aidx guaranteed to stay valid by above checks
					json.erase(brace_open, brace_close - brace_open + 1);
				}

				// now for the other records for original name and all CNAMEs
				for (auto it = fqdns.begin(); it != fqdns.end(); ++it) {

					if (!valid_name(it->first))
						continue;

					for (;;) {

						char data[16] = {0};

						// Some servers remove the trailing "." in FQDNs in their answer,
						// and some add it -.-
						// So check for both versions of the answer FQDN

						string ans = "\"name\":\"" + it->first;
						if ((it->first)[it->first.size() - 1] != '.')
							ans += ".";
						ans += "\",\"type\":";
						if ((idx = json.find(ans, aidx)) == string::npos) {

							ans = "\"name\":\"" + it->first;
							if (ans[ans.size() - 1] == '.')
								ans.erase(ans.size() - 1, 1);
							ans += "\",\"type\":";
							if ((idx = json.find(ans, aidx)) == string::npos)
								break;
						}

						idx += ans.size();
						uint16_t atype = (uint16_t)strtoul(json.c_str() + idx, nullptr, 10);

						if ((brace_close = json.find("}", idx)) == string::npos)
							return 0;
						if ((brace_open = json.rfind("{", idx)) == string::npos)
							return 0;
						if (brace_open < aidx || brace_close < aidx)
							return 0;

						// guaranteed that { comes before }

						string inner_json = json.substr(brace_open, brace_close - brace_open + 1);

						uint32_t ttl = 0;
						if ((idx = inner_json.find("\"ttl\":")) == string::npos)
							ttl = 600;
						else
							ttl = strtoul(inner_json.c_str() + idx + 6, nullptr, 10);

						if ((idx = inner_json.find("\"data\":\"")) == string::npos)
							break;
						idx += 8;
						if ((idx2 = inner_json.find("\"", idx)) == string::npos)
							break;
						tmp = inner_json.substr(idx, idx2 - idx);

						string qname = "";
						if (host2qname(it->first, qname) <= 0)
							break;

						answer_t dns_ans{qname, htons(atype), htons(1), htonl(ttl)};

						if (atype == dns_type::A) {
							if (inet_pton(AF_INET, tmp.c_str(), data) == 1) {
								dns_ans.rdata = string(data, 4);
								result[acnt++] = dns_ans;
								has_answer = 1;
							}
						} else if (atype == dns_type::AAAA) {
							if (inet_pton(AF_INET6, tmp.c_str(), data) == 1) {
								dns_ans.rdata = string(data, 16);
								result[acnt++] = dns_ans;
								has_answer = 1;
							}
						} else if (atype == dns_type::NS) {
							if (!valid_name(tmp))
								return build_error("Invalid DNS name.", -1);

							if (tmp[tmp.size() - 1] == '.')
								tmp.erase(tmp.size() - 1, 1);

							if (host2qname(tmp, qname) <= 0)
								break;
							dns_ans.rdata = qname;
							result[acnt++] = dns_ans;
							has_answer = 1;
						} else if (type == dns_type::MX) {
						}

						// aidx guaranteed to stay valid by above checks
						json.erase(brace_open, brace_close - brace_open + 1);
					}
				}

				return has_answer ? 1 : 0;
			}
			*/

}
