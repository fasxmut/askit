//
// Copyright (c) 2026 cppfx.xyz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "url.hpp"
#include "string.hpp"

sopv::url::url(const std::string & url_string__):
	__url_keep{url_string__}
{
}

namespace sopv::cpp_src
{
	class parser final
	{
	private:
		const std::string __url_keep;
	private:
		sopv::url::schema __schema;
		sopv::url::protocol __protocol;
		std::string __host;
		std::string __port;
		std::string __target;
	public:
		parser(const std::string & url_string__):
			__url_keep{url_string__}
		{
		}
	public:
		[[maybe_unused]] sopv::cpp_src::parser & parse()
		{
			return *this;
		}
	private:
		auto extract_schema() const
		{
			std::string head_20;
			if (__url_keep.size() <= 20)
				head_20 = __url_keep;
			else
				head_20 = __url_keep.substr(0, 20);
			head_20 = sopv::string{head_20}.lower();
			using std::string_literals::operator""s;
			if (head_20.starts_with("http://"))
			{
				return std::tuple{sopv::url::schema::http, __url_keep.substr("http://"s.size())};
			}
		}
	};
}

