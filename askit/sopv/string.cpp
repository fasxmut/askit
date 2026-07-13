//
// Copyright (c) 2026 cppfx.xyz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "string.hpp"
#include <sstream>
#include <iomanip>

std::string sopv::string::quoted() const
{
	return (std::ostringstream{} << std::quoted(__str)).str();
}
std::string sopv::string::strip() const
{
	if (__str.size() < 1u)
		return "";

	if (__str.size() == 1u)
	{
		if (__blanks.contains(__str[0]))
			return "";
		else
			return __str;
	}

	// first position of non-blank.
	std::uint32_t pos;
	for (pos=0; pos<__str.size(); ++pos)
	{
		if (! __blanks.contains(__str[pos]))
			break;
	}
	if (pos >= __str.size())
		return "";

	// last position of non-blank
	std::uint32_t epos;
	for (epos=__str.size()-1; epos>=0; --epos)
	{
		if (! __blanks.contains(__str[epos]))
			break;
	}
	if (epos < pos)
		return "";

	return __str.substr(pos, epos-pos+1u);
}
bool sopv::string::contains_blank_if_strip() const
{
	const std::string tmp = this->strip();
	for (char x: tmp)
	{
		if (__blanks.contains(x))
			return true;
	}
	return false;
}
bool sopv::string::is_number() const
{
	try
	{
		return std::to_string(std::stoll(__str)) == __str;
	}
	catch (...)
	{
		return false;
	}
}
bool sopv::string::is_non_negative_number() const
{
	try
	{
		return std::to_string(static_cast<std::uint64_t>(std::stoll(__str))) == __str;
	}
	catch (...)
	{
		return false;
	}
}
bool sopv::string::is_negative_number() const
{
	try
	{
		auto num = std::stoll(__str);
		if (num >= 0)
			return false;
		// else
		return std::to_string(num) == __str;
	}
	catch (...)
	{
		return false;
	}
}
void sopv::string::make_contract_assert_once() const
{
	static bool is_asserted = false;
	if (is_asserted)
		return;
	contract_assert(int{'a'-'z'} == -25);
	contract_assert(int{'A'-'Z'} == -25);
	is_asserted = true;
}
std::string sopv::string::upper() const
{
	this->make_contract_assert_once();
	std::string tmp;
	for (char x: __str)
	{
		if (x>='a' && x<='z')
			tmp.push_back(static_cast<char>(std::int64_t{x}-'a'+'A'));
		else
			tmp.push_back(x);
	}
	return tmp;
}
std::string sopv::string::lower() const
{
	this->make_contract_assert_once();
	std::string tmp;
	for (char x: __str)
	{
		if (x>='A' && x<='Z')
			tmp.push_back(static_cast<char>(std::int64_t{x}-'A'+'a'));
		else
			tmp.push_back(x);
	}
	return tmp;
}

