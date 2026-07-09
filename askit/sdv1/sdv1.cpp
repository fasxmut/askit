//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <iomanip>
#include <sstream>
#include <memory>
#include <boost/process.hpp>
#include <boost/asio.hpp>

using std::string_literals::operator""s;
namespace process = boost::process;
namespace asio = boost::asio;

namespace sdv1
{
	class args
	{
	private:
		std::vector<std::string> __args{};
		std::vector<std::string> __transformed_args{};
	private:
		std::string __file = "";
		std::string __destination = "";
		std::string __cmd = "";
		std::string __subcmd = "";
		bool __hide_zero = false;
	public:
		args(int argc, char ** argv)
		{
			for (int i=0; i<argc; ++i)
				__args.push_back(argv[i]);
		}
		const std::string get_help() const
		{
			std::ostringstream buffer;
			buffer
				<< "----------------------------------------------------------------------\n"
				<< "| Usage:\n"
				<< "|\n"
				<< "\n"
				<< "sdv1 <input data file> -h [-d <cmd> <subcmd> <destination>]\n"
				<< "\n"
				<< "-h        ---- hide zero\n"
				<< "-d <cmd> <subcmd> <destination>    ---- The cmd, subcmd, destination\n"
				<< "\n"
				<< "Required:\n"
				<< "<input data file>\n\n"
				<< "Optional:\n"
				<< "-h\n-d destination\n"
				<< "\n\n"
			;
			return buffer.str();
		}
	public:
		std::string file() const
		{
			return __file;
		}
		std::string destination() const
		{
			return __destination;
		}
		bool hide_zero() const
		{
			return __hide_zero;
		}
		std::string cmd() const
		{
			return __cmd;
		}
		std::string subcmd() const
		{
			return __subcmd;
		}
	public:
		void parse()
		{
			this->parse_args();
			this->parse_transformed_args();
		}
	private:
		void parse_args()
		{
			if (__args.size() < 2)
				throw std::runtime_error{this->get_help()};

			for (unsigned int i=1; i<__args.size(); ++i)
			{
				if (__args[i] == "-h")
				{
					__hide_zero = true;
				}
				else if (__args[i] == "-d")
				{
					{
						++i;
						if (i<__args.size())
							__cmd = __args[i];
						else
						{
							throw std::runtime_error{
								"Error: -d requires a cmd, a subcmd, a destination"s
									+
								"\n\n"
							};
						}
					}
					{
						++i;
						if (i<__args.size())
							__subcmd = __args[i];
						else
						{
							throw std::runtime_error{
								"Error: -d requires a cmd, a subcmd, a destination"s
									+
								"\n\n"
							};
						}
					}
					{
						++i;
						if (i<__args.size())
							__destination = __args[i];
						else
						{
							throw std::runtime_error{
								"Error: -d requires a cmd, a subcmd, a destination"s
									+
								"\n\n"
							};
						}
					}
				}
				else
				{
					__transformed_args.push_back(__args[i]);
				}
			}
		}
	private:
		void parse_transformed_args()
		{
			if (__transformed_args.size() == 0)
				throw std::runtime_error{
					"Error: input data file is required!\n\n"
				};

			if (__transformed_args.size() != 1)
				throw std::runtime_error{
					"Error: Additional args are not allowed!\n\n"s
				};
			__file = __transformed_args[0];
		}
	};

	class line_matcher final
	{
	private:
		const std::string __line;
	public:
		enum class type
		{
			ident,
			key,
			value,
			drop
		};
	private:
		sdv1::line_matcher::type __type;
		std::string __result;
	public:
		line_matcher(const std::string & line__):
			__line{line__},
			__type{sdv1::line_matcher::type::drop},
			__result{""}
		{
			this->parse();
		}
		virtual ~line_matcher() = default;
	public:
		std::tuple<sdv1::line_matcher::type, std::string> match() const
		{
			return {__type, __result};
		}
	private:
		void parse()
		{
			if (__line.empty())
			{
				__type = sdv1::line_matcher::type::drop;
				return;
			}
			else if (__line.contains("="))
			{
				std::string str = __line.substr(__line.find("=") + 2);
				if (str.empty())
					throw std::runtime_error{"Error: Found ident mark =, but value is empty!"};
				// else: set str to result
				__result = str;
				__type = sdv1::line_matcher::type::ident;
				return;
			}
			else if (__line.contains(":") && __line.contains("{") && __line.contains("_"))
			{
				auto pos1 = __line.find("\"") + 1;
				auto pos2 = __line.find("\"", pos1);
				std::string str = __line.substr(pos1, pos2-pos1);
				if (str.empty())
					throw std::runtime_error{"Error: Found key mark, but its value is empty!"};
				// else set
				__result = str;
				__type = sdv1::line_matcher::type::key;
				return;
			}
			else if (__line.contains("e\"") && __line.contains("n") && __line.contains("l"))
			{
				auto pos1 = __line.find("\"") + 1;
				pos1 = __line.find("\"", pos1) + 1;
				pos1 = __line.find("\"", pos1) + 1;
				auto pos2 = __line.find("\"", pos1);
				std::string str = __line.substr(pos1, pos2-pos1);
				if (str.empty())
					throw std::runtime_error{"Error: Fund value mark, but its value is empty!"};
				// else set
				__result = str;
				__type = sdv1::line_matcher::type::value;
				return;
			}
			// else
			__result = "";
			__type = sdv1::line_matcher::type::drop;
			return;
		}
	};

//////////////////////////////////////////////////////////////////////

	std::map<std::string, std::vector<std::pair<std::string, std::string>>> input{};
}	// namespace sdv1

int main(int argc, char ** argv)
{
	std::shared_ptr<sdv1::args> args = nullptr;
	try
	{
		args = std::make_shared<sdv1::args>(argc, argv);
		args->parse();

		std::ifstream file{args->file()};
		if (! file)
			throw std::runtime_error{
				"Open input data file error: "s
					+
				(std::ostringstream{} << std::quoted(args->file())).str()
					+
				"\nPlease check the input data file!"
			};

		std::string line;
		int key_count = 0;
		int value_count = 0;
		std::string ident = "", key = "", value = "";	// Current ident, key, value

		while (std::getline(file, line))
		{
			auto [type, result] = sdv1::line_matcher{line}.match();
			switch (type)
			{
			case sdv1::line_matcher::type::drop:
				continue;
			case sdv1::line_matcher::type::ident:
				ident = result;
				break;
			case sdv1::line_matcher::type::key:
				key = result;
				++key_count;
				break;
			case sdv1::line_matcher::type::value:
				value = result;
				++value_count;
				if (key_count != value_count)
					throw std::runtime_error{"Error: key_count and value_count do not match!"};

				// push now
				{
					if (ident.empty() || key.empty() || value.empty())
						throw std::runtime_error{"Check failed here: ident, key, value all are required not empty!"};
					if (sdv1::input.contains(ident))
					{
						sdv1::input[ident].emplace_back(key, value);
					}
					else
					{
						std::vector<std::pair<std::string, std::string>> kv_list{};
						kv_list.emplace_back(key, value);
						sdv1::input[ident] = kv_list;
					}
				}

				break;
			}
		}	// while ends

		if (args->destination().empty())
		{
			for (const auto & [ident, kv]: sdv1::input)
			{
				std::cout << ident << std::endl << std::endl;
				for (const auto & [key, value]: kv)
				{
					if (value == "0" && args->hide_zero() == true)
						continue;
					std::cout << key << " " << value << std::endl;
				}
				std::cout << std::endl;
			}
			return 0;
		}
		else
		{
			std::vector<std::vector<std::string>> cmd_list;
			// Stage 1: make cmd_list
			for (const auto & [ident, kv_list]:  sdv1::input)
			{
				for (const auto & [key, value]: kv_list)
				{
					if (value == "0")
						continue;
					std::vector<std::string> cmd{};
					cmd.push_back(args->cmd());
					cmd.push_back(args->subcmd());
					cmd.push_back(ident);
					cmd.push_back(key);
					cmd.push_back(args->destination());
					cmd.push_back(value);
					cmd_list.push_back(cmd);
				}
			}
			// Stage 2: execute cmd_list
			for (const auto & cmd: cmd_list)
			{
				for (const auto & sub: cmd)
					std::cout << sub << " ";
				std::cout << std::endl;
			}
			std::clog << "Do you want to execute all of them? (Y/n) ";
			std::string answer;
			std::getline(std::cin, answer);
			if (answer != "y" && answer != "Y")
				return 1;
			{
				asio::thread_pool pool{1024u};
				std::vector<std::shared_ptr<process::process>> procs{};
				for (const auto & cmd: cmd_list)
				{
					const std::string r_cmd = cmd[0];
					const std::vector<std::string> opts{cmd.begin()+1, cmd.end()};
					std::cout << r_cmd << std::endl;
					procs.push_back(
						std::make_shared<process::process>(
							pool.get_executor(),
							process::environment::find_executable(r_cmd),
							opts
						)
					);
				}
				for (auto & proc: procs)
					proc->wait();
				pool.join();
			}
		}
	}
	catch (const std::exception & e)
	{
		std::cerr
			<< "======================================================================\n"
			<< "==== c++ std::exception:\n\n"
			<< e.what()
			<< std::endl
		;
		return 1;
	}
}

