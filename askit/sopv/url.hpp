//
// Copyright (c) 2026 cppfx.xyz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <string>

namespace sopv
{
	// sopv::url only takes a url string input,
	//		additional info should not be passed to it,
	//			such as method, conent-type.
	// And sopv::url parses to the string result only,
	//		the network request, connection should not be processed by it.
	class url
	{
	protected:
	// input
		const std::string __url_keep;
	protected:
		using self_type = sopv::url;
	public:
		enum class schema
		{
			unknown,
			http,	// url prefix:		http://
			https,	// url prefix:		https://
			socket,	// url prefix:		socket://
			websocket	// url prefix:		websocket://
		};
		enum class protocol
		{
			unknown,
			v4_ip,
			v6_ip,
			domain
		};
	protected:
	// output
		self_type::schema __schema; // no default, if such, set to unknown.
		self_type::protocol __protocol; // no default, if such, set to unknown.
		std::string __host;
		std::string __port;
		std::string __target;
	public:
		url() = delete;
		url(const std::string & url_string__);
	};
}	// namespace sopv

