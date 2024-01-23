/*
 * This file is part of harddns.
 *
 * (C) 2019 by Sebastian Krahmer, sebastian [dot] krahmer [at] gmail [dot] com
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

/* some of the header definitions have been taken from various other
 * open-sourced include files
 */

#ifndef SRC_DNS_NET_HEADERS
#define SRC_DNS_NET_HEADERS

#ifndef SRC_NET_HEADERS
#define SRC_NET_HEADERS

#include <sys/types.h>
#include <stdint.h>

#include <bit>

namespace harddns
{
	template <auto endian>
	struct endian_base;

	// https://www.rfc-editor.org/rfc/rfc1035#section-4.1.1
	
	template <>
	struct endian_base<std::endian::big>
	{
		uint16_t id;
		/* fields in third byte */
		uint16_t qr : 1;	 /* response flag */
		uint16_t opcode : 4; /* purpose of message */
		uint16_t aa : 1;	 /* authoritive answer */
		uint16_t tc : 1;	 /* truncated message */
		uint16_t rd : 1;	 /* recursion desired */
							 /* fields in fourth byte */
		uint16_t ra : 1;	 /* recursion available */
		uint16_t : 3;		 /* checking disabled by resolver */
		uint16_t rcode : 4;	 /* response code */

		uint16_t q_count;
		uint16_t a_count;
		uint16_t rra_count;
		uint16_t ad_count;
		endian_base() : id(0),
						q_count(0), a_count(0), rra_count(0), ad_count(0)
		{
			qr = 0;
			opcode = 0;
			aa = 0;
			tc = 0;
			rd = 0;
			ra = 0;
			rcode = 0;
		}
	};

	template <>
	struct endian_base<std::endian::little>
	{
		uint16_t id;
		/* fields in third byte */
		uint16_t rd : 1;	 /* recursion desired */
		uint16_t tc : 1;	 /* truncated message */
		uint16_t aa : 1;	 /* authoritive answer */
		uint16_t opcode : 4; /* purpose of message */
		uint16_t qr : 1;	 /* response flag */
							 /* fields in fourth byte */
		uint16_t rcode : 4;	 /* response code */
		uint16_t : 3;		 /* checking disabled by resolver */
		uint16_t ra : 1;	 /* recursion available */

		uint16_t q_count;
		uint16_t a_count;
		uint16_t rra_count;
		uint16_t ad_count;

		endian_base() : id(0),
						q_count(0), a_count(0), rra_count(0), ad_count(0)
		{
			qr = 0;
			opcode = 0;
			aa = 0;
			tc = 0;
			rd = 0;
			ra = 0;
			rcode = 0;
		}
	};

	namespace net_headers
	{
		class dnshdr : public endian_base<std::endian::native>
		{
		public:
			dnshdr(const dnshdr &) = delete;
			dnshdr() = default;
		};

		enum dns_type : uint16_t
		{
			A = 1,
			NS = 2,
			CNAME = 5,
			SOA = 6,
			PTR = 12,
			HINFO = 13,
			MX = 15,
			TXT = 16,
			AAAA = 28,
			SRV = 33,
			DNAME = 39,
			OPT = 41,
			DNSKEY = 48,
			EUI64 = 109,
		};

	} // namespace

} // namespace

#endif /* SRC_NET_HEADERS */

#endif /* SRC_DNS_NET_HEADERS */
