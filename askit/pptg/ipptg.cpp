//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// pptg:
//		Phrase Protector Table Generator

// ipptg:
//		Improved pptg

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <regex>
#include <sstream>
#include <random>
#include <format>
#include <chrono>
#include <thread>
#include <span>

using std::string_literals::operator""s;

namespace pptg
{
	using args_type = std::vector<std::string>;
	using data_type = std::vector<std::string>;

	constexpr int min_phrase_count = 5;
	constexpr int max_phrase_count = 100;

	std::mt19937 rnd{std::mt19937_64{std::mt19937_64{std::random_device{}()}()}()};
}	// namespace pptg

namespace pptg
{
	class tools
	{
	public:
		static bool is_word(const std::string & str)
		{
			std::regex re{"[a-zA-Z]+"};
			std::smatch sm;
			return std::regex_match(str, sm, re);
		}
	public:
		static bool contains(const pptg::data_type & rg, const std::string & item)
		{
			for (const auto & w: rg)
			{
				if (w == item)
					return true;
			}
			return false;
		}
		static int get_pos(const pptg::data_type & rg, const std::string & item)
		{
			for (int i=0; i<(int)rg.size(); ++i)
			{
				if (rg[i] == item)
					return i;
			}
			return -1;
		}
	};
}	// namespace pptg

namespace pptg
{
	class show_help
	{
	private:
		std::string __msg;
	public:
		show_help(const std::string & msg__ = ""):
			__msg{msg__}
		{
		}
	public:
		void show() const
		{
			if (! __msg.empty())
				std::cerr << "ERROR:\n" << __msg << std::endl;
			std::clog
				<< "Usage:\n"
				<< std::endl
				<< "ipptg <input words> <input random-words-file .txt> <output \"map-data.txt\">\n"
				<< "\t# output data file: map-data.txt\n"
				<< "\t# output \"key index\" to console\n"
				<< "\n"
				<< "ipptg recover <input \"key index\"> <input \"map-data.txt\">\n"
				<< "# output words to console\n"
				<< std::endl
			;
		}
	};

	class args
	{
	private:
		pptg::args_type __args;
	public:
		args(int argc, char ** argv)
		{
			this->make_args(argc, argv);
		}
	private:
		void make_args(int argc, char ** argv)
		{
			if (argc < 2)
				throw pptg::show_help();
			if ("recover"s != argv[1])
				this->make_transform_args(argc, argv);
			else
				this->make_recover_transform_args(argc, argv);
			this->not_empty_test();
		}
	private:
		void make_transform_args(int argc, char ** argv)
		{
			__args.push_back("transform");
			for (int i=1; i<argc; ++i)
				__args.push_back(argv[i]);
			if (__args.size() != 4)
				throw pptg::show_help{"Command args error."};
			// Test input "random-words-file"
			{
				std::ifstream file{__args[2]};
				if (! file)
					throw std::runtime_error{
						"Error: Can not open random words file: \""s
						+
						__args[2]
						+
						"\"\n\n"
					};
			}
		}
	private:
		void make_recover_transform_args(int argc, char ** argv)
		{
			__args.push_back("recover");
			for (int i=2; i<argc; ++i)
				__args.push_back(argv[i]);
			if (__args.size() != 3)
				throw pptg::show_help{"Command args error."};
		}
	private:
		void not_empty_test() const
		{
			if (__args.empty())
				throw pptg::show_help{"Command Args Error! (-1)"};
			int i = 0;
			for (const std::string & str: __args)
			{
				if (str.empty())
				{
					throw pptg::show_help{
						"...Command Args Error! ("s
						+
						std::to_string(i)
						+
						")\n"
					};
				}
				++i;
			}
		}
	public:
		const pptg::args_type & ref() const
		{
			return __args;
		}
	public:
		void log() const
		{
			for (const auto & w: this->ref())
				std::clog << std::quoted(w) << " ";
			std::clog << std::endl;
		}
	};
}	// namespace pptg

namespace pptg
{
	class task
	{
	public:
		virtual void start() = 0;
	};
}	// namespace pptg

namespace pptg
{
	class transform:
		virtual public pptg::task
	{
	private:
		const pptg::args_type & __args;
		pptg::data_type __backup;
		pptg::data_type __data;
		pptg::data_type __entropy;
	public:
		transform(const pptg::args_type & args__):
			__args{args__}
		{
		}
	public:
		void start() override
		{
			this->make_data();
			this->export_data();
			this->make_index();
		}
	private:
		void make_data()
		{
			std::stringstream io;
			io << __args[1];

		// Read input phrases to __backup
			std::string str = "";
			while (io >> str)
			{
				if (! pptg::tools::is_word(str))
				{
					throw std::runtime_error{
						"All input words must only contain Latin letters."
					};
				}
				__backup.push_back(str);
				if (__backup.size() > pptg::max_phrase_count)
				{
					throw std::runtime_error{
						"Input words too large, limit: "s
						+
						std::to_string(pptg::max_phrase_count)
						+
						"\n\n"
					};
				}
				str = "";
			}
			if (__backup.size() < pptg::min_phrase_count)
			{
				throw std::runtime_error{
					"Requires at least "s + std::to_string(pptg::min_phrase_count) + " words!\n\n"
				};
			}

		// entropy, shuffle, random
			{
				const int word_count = __backup.size();
				const int spread_count = word_count * 2;
				const int entropy_count = 2 * (word_count + spread_count);
				[[maybe_unused]]
				const int total = word_count + spread_count + entropy_count;

				std::vector<std::string> buffer;

				// Read to buffer
				{
					std::ifstream file{__args[2]};
					if (! file)
						throw std::runtime_error{"---, . Can not open random words file!"};
					else
						std::clog << "Open random words file OK.\n";

					std::string wd;
					while (std::getline(file, wd))
					{
						if (pptg::tools::is_word(wd))
						{
							if (
								! pptg::tools::contains(__backup, wd)
								&&
								! pptg::tools::contains(buffer, wd)
							)
							{
								buffer.push_back(wd);
							}
						}
						if (buffer.size() > spread_count + entropy_count + (pptg::rnd()%10+5))
							break;
					}
				}

				// Check buffer size
				if ((int)buffer.size() < spread_count + entropy_count)
				{
					const std::string e_msg = std::format(
						"random words file \"{}\" does not contain enough words,\n"
						"word_count: {}\n"
						"spread_count: {}\n"
						"entropy_count: {}\n"
						"Requires at least: {} + {} = {}   in random words file.\n"
						,
						__args[2],
						word_count,
						spread_count,
						entropy_count,
						spread_count,
						entropy_count,
						spread_count+entropy_count
					);
					throw std::runtime_error{e_msg};
				}

				std::this_thread::sleep_for(std::chrono::microseconds(pptg::rnd()%10000+2000));
				// Random shuffle buffer
				std::shuffle(buffer.begin(), buffer.end(), pptg::rnd);

				int pos;

				// Copy spread_count words from buffer to __data
				for (pos=0; pos<spread_count; ++pos)
				{
					__data.push_back(buffer[pos]);
				}

				// Copy all __backup to __data
				for (const auto & w: __backup)
				{
					__data.push_back(w);
				}

				// Random shuffle __data
				for (unsigned i=0; i<13u+pptg::rnd()%30; ++i)
				{
					std::this_thread::sleep_for(std::chrono::microseconds(pptg::rnd()%10000+2000));
					std::shuffle(__data.begin(), __data.end(), pptg::rnd);
				}

				// Merge to result.
				{
					std::vector<std::string> result;
					int index = 0;
					while (pos < (int)buffer.size())
					{
						result.push_back(buffer[pos++]);
						if (pptg::rnd()%2==0)
						{
							if (pos < (int)buffer.size())
								result.push_back(buffer[pos++]);
							if (index < (int)__data.size())
								result.push_back(__data[index++]);
						}
						else
						{
							if (index < (int)__data.size())
								result.push_back(__data[index++]);
							if (pos < (int)buffer.size())
								result.push_back(buffer[pos++]);
						}
					}

					// Test
					{
						if (index != (int)__data.size())
							throw std::runtime_error{"Program Error (1): __data size."};
						if (pos != (int)buffer.size())
							throw std::runtime_error{"Program Error (2): buffer size."};
						__data = {};
						__data = result;

						for (const auto & w: __data)
						{
							if (! pptg::tools::is_word(w))
								throw std::runtime_error{
									"Program Error (3): __data contains non-word."};
						}
					}
				}
			}
		}
	private:
		void export_data()
		{
			std::ofstream file{__args[3], std::ios::trunc};
			if (! file)
				throw std::runtime_error{
					"Can not open output data file for writing:\n\t\""s
					+
					__args[3]
					+
					"\""
				};
			file << "// Keep this file safe." << std::endl;
			file << "// Do not edit this file by hand." << std::endl;
			file << "// This file is generated by ipptg." << std::endl;
			for (const std::string & wd: __data)
			{
				file << wd << std::endl;
			}
			file.close();
			std::clog << "Export data OK!" << std::endl;
			std::cout << "\n\nBackup data file " << std::quoted(__args[3]) << " and keep it safe!\n\n";
		}
	private:
		void make_index()
		{
			std::stringstream io;
			for (const auto & w: __backup)
			{
				int pos = pptg::tools::get_pos(__data, w);
				if (pos == -1)
					throw std::runtime_error{
						"Program Error (4): word not found."
					};
				io << pos << " ";
			}
			std::cout << "\n\nWrite down this index sequence on paper and keep it safe:\n";
			std::cout << io.str() << std::endl << std::endl;
		}
	};
}	// namespace pptg

namespace pptg
{
	class recover_transform:
		virtual public pptg::task
	{
	private:
		const pptg::args_type & __args;
		std::vector<std::string> __data;
		std::vector<int> __index;
	public:
		recover_transform(const pptg::args_type & args__):
			__args{args__}
		{
		}
	public:
		void start() override
		{
			this->scan_data();
			this->make_index();
			this->recover();
		}
	private:
		void scan_data()
		{
			std::ifstream file{__args[2]};
			if (! file)
			{
				throw std::runtime_error{
					std::format(
						"Can not open data file: \"{}\"\n",
						__args[2]
					)
				};
			}
			std::string str;
			while (std::getline(file, str))
			{
				if (str.empty() || str.starts_with("//"))
					continue;
				if (! pptg::tools::is_word(str))
				{
					throw std::runtime_error{
						std::format(
							"data file \"{}\" contains an invalid word: \"{}\".",
							__args[2],
							str
						)
					};
				}
				else
				{
					__data.push_back(str);
				}
			}
		}
	private:
		void make_index()
		{
			std::stringstream io;
			io << __args[1];
			std::string s;
			while (io >> s)
			{
				int ii;
				try
				{
					ii = std::stoi(s);
					if (std::to_string(ii) != s)
						throw 0;
				}
				catch (...)
				{
					throw std::runtime_error{
						"Input index sequence error!"
					};
				}
				__index.push_back(ii);
			}
		}
	private:
		void recover()
		{
			std::stringstream io;
			for (int i=0; i<(int)__index.size(); ++i)
			{
				int pos = __index[i];
				if (pos >= (int)__data.size())
				{
					throw std::runtime_error{
						std::format(
							"Index out of range, index error or data error!\n"
							"Index: {}\n"
							"Size: {}\n",
							pos,
							__data.size()
						)
					};
				}
				io << __data[pos] << " ";
			}
			std::cout << "Recovered:\n" << io.str() << std::endl << std::endl;
		}
	};
}	// namespace pptg

int main(int argc, char ** argv)
{
	try
	{
		pptg::args args{argc, argv};
		//args.log();

		pptg::task * task = nullptr;

		if (args.ref()[0] == "transform")
			task = new pptg::transform(args.ref());
		else if (args.ref()[0] == "recover")
			task = new pptg::recover_transform(args.ref());
		else
			throw pptg::show_help{};

		task->start();
	}
	catch (const pptg::show_help & help)
	{
		help.show();
		return 1;
	}
	catch (const std::exception & e)
	{
		std::cerr << "ERROR:\n";
		std::cerr << e.what() << std::endl;
		return 1;
	}
}

