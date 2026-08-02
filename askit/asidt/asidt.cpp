//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <regex>
#include <iomanip>

int main(int argc, char ** argv)
{
	try
	{
		try
		{
			std::vector<std::string> args;
			for (int i=0; i<argc; ++i)
				args.emplace_back(argv[i]);
			std::clog << "args count: " << args.size() << std::endl;
			if (args.size() != 3 && args.size() != 2)
				throw std::runtime_error{"args error."};

			if (args.size() == 2u)
			{
				std::ifstream file{argv[1]};
				if (! file)
					throw std::runtime_error{"input file error."};
				std::string line;
				while (std::getline(file, line))
				{
					std::smatch sm;
					if (std::regex_search(line, sm,
						std::regex{"^\\ \\ \\ \\ [^ ]+"}))
					{
						args.emplace_back("4");
						break;
					}
					else if (std::regex_search(line, sm,
						std::regex{"^\\ \\ \\ \\ \\ \\ \\ \\ [^ ]+"}))
					{
						args.emplace_back("8");
						break;
					}
				}
			}
			std::clog << "Got tab size: " << std::stoi(args[2]) << std::endl;
			std::string indent;
			for (int i=0; i<std::stoi(args[2]); ++i)
			{
				indent.push_back(' ');
			}
			std::clog << "indent: " << std::quoted(indent) << std::endl;

			{
				std::ifstream file{args[1]};
				if (! file)
					throw std::runtime_error{"input file error. (2)"};
				std::string line;
				while (std::getline(file, line))
				{
					if (indent.empty())
					{
						std::cout << line << std::endl;
						continue;
					}
					// else
					int counter = 0;
					while (line.starts_with(indent))
					{
						++counter;
						line = line.substr(indent.size());
					}
					bool more = false;
					while (line.starts_with(" "))
					{
						more = true;
						line = line.substr(1);
					}
					if (more)
						++counter;
					for (int i=0; i<counter; ++i)
						std::cout << "\t";
					std::cout << line << std::endl;
				}
			}
		}
		catch (const std::exception & e)
		{
			std::cerr << "ERROR: " << e.what() << std::endl;
			throw 0;
		}
		catch (...)
		{
			std::cerr << "ERROR." << std::endl;
			throw 0;
		}
	}
	catch (...)
	{
		std::cerr << "----------------------------------------\n";
		std::cerr
			<< "Usage:\n"
			<< "indent <filename> [tab length]\n"
			<< "###\n"
			<< "### [tab length] is only auto-detected as 4 or 8 if not provided.\n"
			<< "###\n"
			<< std::endl
		;
		return 1;
	}
}

