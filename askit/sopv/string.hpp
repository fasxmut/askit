//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <string>

namespace sopv
{
	class string
	{
	protected:
		const std::string __blanks = " \n\t\r\v";
		std::string __str;
	public:
		virtual ~string() = default;
	public:
		string(const std::string & str__):
			__str{str__}
		{
		}
	public:
		std::string quoted() const;
		std::string strip() const;
		bool contains_blank_if_strip() const;
		bool is_number() const;
		bool is_non_negative_number() const;
		bool is_negative_number() const;
	protected:
		void make_contract_assert_once() const;
	public:
		std::string upper() const;
		std::string lower() const;
	};	// class string
}	// namespace sopv

