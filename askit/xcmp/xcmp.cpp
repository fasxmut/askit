//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <string>
#include <regex>
#include <iostream>
#include <vector>

namespace xcmp
{
	class is_number
	{
	private:
		bool __result{false};
	public:
		virtual ~is_number() = default;
		is_number() = delete;
	public:
		is_number(const std::string & str)
		{
			std::regex re{"[0-9]*\\.?[0-9]*"};
			std::smatch sm;
			__result = std::regex_match(str, sm, re);
		}
	public:
		bool result() const
		{
			return __result;
		}
	};

	using size_type = unsigned long;

	class compare
	{
	private:
		std::string __result{"na"};
	public:
		compare() = delete;
		virtual ~compare() = default;
	public:
		compare(const std::string & s1, const std::string & s2)
		{
			this->start(s1, s2);
		}
	public:
		std::string result() const
		{
			return __result;
		}
	private:
		void start(const std::string & s1, const std::string & s2)
		{
			if (s1.empty() || s2.empty())
			{
				__result = "na";
				return;
			}
			__result = this->compare_two(s1, s2);
			return;
		}
	private:
		std::string compare_two(const std::string & s1, const std::string & s2) const
		{
			if (xcmp::is_number{s1}.result() && xcmp::is_number{s2}.result())
			{
				return this->compare_numbers(s1, s2);
			}
			else
			{
				return this->compare_strings(s1, s2);
			}
		}
	private:
		std::vector<std::string> split_number(const std::string & str) const
		{
			std::vector<std::string> zzff(2u);
			{
				std::regex re{"([0-9]*)\\.?([0-9]*)"};
				std::smatch results;
				{
					bool status = std::regex_match(str, results, re);
					contract_assert(status);
				}
				zzff[0] = results[1];
				zzff[1] = results[2];
			}
			{
				xcmp::size_type pos = 0;
				for (; pos<zzff[0].size(); ++pos)
				{
					if (zzff[0][pos] != '0')
						break;
				}
				if (pos >= zzff[0].size())
					zzff[0] = "0";
				else
					zzff[0] = zzff[0].substr(pos);
			}
			{
				while (zzff[1].ends_with("0"))
					zzff[1] = zzff[1].substr(0, zzff[1].size() - 1);
				if (zzff[1].empty())
					zzff[1] = "0";
			}
			return zzff;
		}
		std::string compare_numbers(const std::string & s1, const std::string & s2) const
		{
			auto v1 = this->split_number(s1);
			contract_assert(v1.size() == 2u);
			auto v2 = this->split_number(s2);
			contract_assert(v2.size() == 2u);

			std::string result = this->compare_strings(v1[0], v2[0]);
			if (result == "eq")
				result = this->compare_strings(v1[1], v2[1]);

			return result;
		}
	private:
		std::string compare_strings(const std::string & s1, const std::string & s2) const
		{
			const xcmp::size_type size1 = s1.size();
			const xcmp::size_type size2 = s2.size();
			const xcmp::size_type size = ((size1 < size2) ? size1 : size2);
			for (xcmp::size_type i=0; i<size; ++i)
			{
				if (s1[i] < s2[i])
				{
					return "lt";
				}
				else if (s1[i] > s2[i])
				{
					return "gt";
				}
			}

			if (size1 < size2)
			{
				return "lt";
			}
			else if (size1 > size2)
			{
				return "gt";
			}
			else
			{
				return "eq";
			}
		}
	};
}

int main(int argc, char ** argv)
{
	if (argc != 3)
	{
		std::cout << "na" << std::endl;
		return 0;
	}

	const std::string result =
		xcmp::compare{std::string{argv[1]}, std::string{argv[2]}}.result();

	if (
		result == "eq"
		||
		result == "lt"
		||
		result == "gt"
		||
		result == "na"
	)
	{
		std::cout << result << std::endl;
		return 0;
	}
	else
	{
		const std::string error = "\nProgram Error, please report bug!\n\n";
		std::cerr << error;
		std::cout << error;
		return 1;
	}
}

