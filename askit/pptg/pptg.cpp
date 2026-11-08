//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// pptg:
//		Phrase Protector Table Generator

#include <iostream>
#include <random>
#include <vector>
#include <sstream>
#include <functional>
#include <algorithm>
#include <regex>
#include <fstream>
#include <chrono>
#include <thread>

using std::string_literals::operator""s;

namespace pptg
{
	std::mt19937 rnd{std::mt19937_64{std::mt19937_64{std::random_device{}()}()}()};
}

namespace pptg
{
	class input
	{
	private:
		using self_type = pptg::input;
	public:
		using format = std::vector<std::vector<std::string>>;
	private:
		const self_type::format __data;
		const int __size{0};
	public:
		input(int argc, char ** argv)
		{
			this->make_input(argc, argv);
		}
	private:
		void make_input(int argc, char ** argv)
		{
			if (argc < 4)
				throw std::runtime_error{
					"pptg <input words> <input words> ... <random words file> <output html file>\n"s
					+
					"Requires at least one <input words> and a rand words file\n"
				};
			auto & data = const_cast<self_type::format &>(__data);

			// Make input
			for (int i=1; i<argc-2; ++i)
			{
				std::stringstream list;
				list << argv[i];
				if (list.str().empty())
					continue;
				else
					data.emplace_back();
				std::string word;
				while (list >> word)
				{
					if (word.empty())
						continue;
					else
						data.back().push_back(word);
				}
			}

			// Test data and make count: __size
			{
				int count = 0;
				for (const auto & l: __data)
				{
					if (l.empty())
						throw std::runtime_error{"Input error (1)"};
					int test = 0;
					for (const auto & w: l)
					{
						if (w.empty())
							throw std::runtime_error{"Input error (2)"};
						++count;
						++test;
						if (! self_type::is_latin_word(w))
							throw std::runtime_error{"Input error: only Latin words are allowed!"};
					}
					if (test < 1)
						throw std::runtime_error{"Input error (3)"};
				}
				const_cast<int &>(__size) = count;
			}
		}
	public:
		void log() const
		{
			std::clog << "Input: ============>\n";
			for (const auto & l: __data)
			{
				for (const auto & w: l)
					std::clog << w << " ";
				std::clog << std::endl;
			}
			std::clog << __size << "\n\n";
		}
	public:
		const self_type::format & ref() const
		{
			return __data;
		}
	public:
		static bool is_latin_word(const std::string & word)
		{
			std::regex re{"[a-zA-Z]+"};
			std::smatch sm;
			return std::regex_match(word, sm, re);
		}
	};	// class input
}	// namespace pptg

namespace pptg
{
	class matrix
	{
	private:
		using self_type = pptg::matrix;
	public:
		using format = std::vector<std::string>;
	private:
		const unsigned __rows;
		const unsigned __columns;
		const unsigned __size;
		self_type::format __data;
	private:
		pptg::input::format __lookup;
	public:
		matrix(
			const unsigned rows__,
			const unsigned columns__
		):
			__rows{rows__},
			__columns{columns__},
			__size{__rows * __columns}
		{
			if (__rows > 26 || __columns > 26)
				throw std::runtime_error{
					"Requires:\n"s
					+
					"	__rows <= 26\n"
					+
					"	__columns <= 26\n"
				};
		}
	public:
		void add_input(const pptg::input & input)
		{
			for (const auto & l: input.ref())
			{
				for (const auto & w: l)
				{
					if (! self_type::contains(__data, w))
					{
						__data.push_back(w);
					}
				}
			}
			if (__data.size() > __size)
			{
				throw std::runtime_error{
					"Input words are too large!\n"s
					+
					"Input: " + std::to_string(__data.size()) + "\n"
					"Limit: "
					+
					std::to_string(__size) + "\n"
					+
					"\n"
				};
			}
		}
	public:
		static bool contains(const auto & test, const std::string & word)
		{
			for (const auto & w: test)
			{
				if (w == word)
				{
					return true;
				}
			}
			return false;
		}
	public:
		void add_entropies(const std::string & filename)
		{
			{
				bool status = (__size > __data.size());
				contract_assert(status);
			}

			std::ifstream file{filename};
			if (! file)
				throw std::runtime_error{
					"Can not open <random words file>: "s
					+
					"\""
					+
					filename
					+
					"\""
				};
			self_type::format random_words;
			std::string get;

			// Read words from file
			while (std::getline(file, get))
			{
				if (pptg::input::is_latin_word(get))
				{
					if (! self_type::contains(random_words, get))
						random_words.push_back(get);
				}
			}

			// Test random_words count
			{
				if (random_words.size() < __size - __data.size())
				{
					throw std::runtime_error{
						"Can not extract enough unique Latin words from file \""s + filename + "\",\n"
						+
						"Filled: "s + std::to_string(__data.size()) + "\n"
						+
						"Total: " + std::to_string(__size) + "\n"
						+
						"Requires: "
						+
						std::to_string(__size)
						+
						" - "
						+
						std::to_string(__data.size())
						+ " = "
						+
						std::to_string(__size - __data.size())
						+
						"\n\n"
					};
				}
			}

			// Shuffle random_words
			{
				for (unsigned i=0; i<pptg::rnd()%12+5; ++i)
				{
					std::this_thread::sleep_for(std::chrono::microseconds(pptg::rnd()%5000+100));
					std::shuffle(random_words.begin(), random_words.end(), pptg::rnd);
				}
			}

			// Take words from rand_words to __data
			{
				for (const auto & word: random_words)
				{
					if (__data.size() >= __size)
						break;
					if (! self_type::contains(__data, word))
					{
						__data.push_back(word);
					}
				}
			}
		}
	public:
		void shuffle_data()
		{
			for (unsigned i=0; i<20u + pptg::rnd()%100; ++i)
			{
				std::this_thread::sleep_for(std::chrono::microseconds(pptg::rnd()%5000+100));
				std::shuffle(__data.begin(), __data.end(), pptg::rnd);
			}
		}
	public:
		void check_data() const
		{
			// Check size
			if (__data.size() != __size)
			{
				throw std::runtime_error{
					"__data count error:\n__data.size() != __size\n\n"
				};
			}

			// Check unique
			{
				for (auto itr=__data.begin(); itr!=__data.end(); ++itr)
				{
					const std::string tmp = *itr;
					if (self_type::contains(std::span{itr+1, __data.end()}, tmp))
					{
						throw std::runtime_error{
							"Error: duplicated word in __data: "s + tmp
						};
					}
				}
			}
		}
	public:
		const self_type::format & ref() const
		{
			return __data;
		}
	public:
		void make_lookup(const pptg::input & input)
		{
			for (const auto & l: input.ref())
			{
				__lookup.emplace_back();
				for (const auto & w: l)
				{
					unsigned pos = 0;
					for (;pos<__data.size(); ++pos)
					{
						if (w == __data[pos])
						{
							break;
						}
					}
					if (pos >= __data.size())
						throw std::runtime_error{
							w + " is not written to __data."
						};
					unsigned j_index = pos / __columns;
					unsigned i_index = pos % __columns;
					std::string lookup;
					lookup.push_back('A' + i_index);
					lookup.push_back('a' + j_index);
					__lookup.back().push_back(lookup);
				}
			}
		}
	public:
		void log_matrix() const
		{
			for (unsigned j=0; j<__rows; ++j)
			{
				for (unsigned i=0; i<__columns; ++i)
				{
					std::clog << __data[j*__columns+i] << " ";
				}
				std::clog << std::endl;
			}
		}
	public:
		void print_lookup() const
		{
			std::cout << "----------------------\n";
			std::cout << "----------------------\n";
			std::cout << "----------------------\n";
			std::cout << "Lookup:\n";
			for (const auto & l: __lookup)
			{
				for (const auto & w: l)
				{
					std::cout << w << " ";
				}
				std::cout << std::endl;
			}
		}
	public:
		unsigned rows() const
		{
			return __rows;
		}
		unsigned columns() const
		{
			return __columns;
		}
	};
}	// namespace pptg

namespace pptg
{
	class html
	{
	private:
		const pptg::matrix & __matrix;
		std::ostringstream __html;
	public:
		html(const pptg::matrix & matrix__):
			__matrix{matrix__}
		{
			this->start();
			this->make_table();
			this->end();
		}
	private:
		void start()
		{
			__html
				<< "<!DOCTYPE html>\n"
				<< "<head>\n"
				<< "<title>Table</title>\n"
			;

			// css
			__html
				<< "<style>\n"
				<< "table\n"
				<< "{\n"
				<< "	background:#DDDDDD;\n"
				<< "	border:3px solid #000000;\n"
				<< "}\n"
				<< "th, td\n"
				<< "{\n"
				<< "	background:#FFFFFF;\n"
				<< "	text-align:center;\n"
				<< "}\n"
				<< "td\n"
				<< "{\n"
				<< "	font-weight:bold;\n"
				<< "	color:#F32542;\n"
				<< "}\n"
				<< ".s_row\n"
				<< "{\n"
				<< "	background:#727325;\n"
				<< "	color:#39FF72;\n"
				<< "}\n"
				<< ".s_column\n"
				<< "{\n"
				<< "	background:#9395C2;\n"
				<< "}\n"
				<< "</style>\n"
			;

			__html
				<< "</head>\n"
				<< "<body>\n"
			;
		}
	private:
		void end()
		{
			__html
				<< "</body>\n"
				<< "</html>\n"
			;
		}
	private:
		void make_table()
		{
			__html << "<table border=0>\n";

		// Make first row
			__html << "<tr>\n";
			__html << "<td>Table</td>";
			for (char x='A'; x<static_cast<char>('A'+__matrix.columns()); ++x)
			{
				__html << "<th";
				if (int{x-'A'}%2==0)
					__html << " class=\"s_column\"";
				__html << ">" << x << "</th>";
			}
			__html << "</tr>\n";

		// Make other rows
			for (unsigned j=0; j<__matrix.rows(); ++j)
			{
				__html << "<tr>\n";
				__html << "<th";
				if (j%2 == 0)
					__html << " class=\"s_row\"";
				__html << ">" << static_cast<char>('a'+j) << "</th>";
				for (unsigned i=0; i<__matrix.columns(); ++i)
				{
					__html << "<td";
					if (j%2==0)
						__html << " class=\"s_row\"";
					else if (i%2 == 0)
						__html << " class=\"s_column\"";
					__html << ">" << __matrix.ref()[j*__matrix.columns()+i] << "</td>";
				}
				__html << "</tr>\n";
			}

			__html << "</table>\n";
		}
	public:
		void write(const std::string & filename) const
		{
			std::ofstream file{filename, std::ios::trunc};
			if (! file)
				throw std::runtime_error{
					"Can not open file to write html result: "s
					+
					"\""
					+
					filename
					+
					"\""
				};
			file << __html.str();
		}
	};	// class html
}	// namespace pptg

int main(int argc, char ** argv)
{
	try
	{
		if (argc < 2)
		{
			std::clog <<
				"Usage:\npptg <input words> <input words> ... <random words file> <output html file>\n"
			;
			return 1;
		}
		const pptg::input input{argc, argv};
		//input.log();

		// rows: 20, columns: 12
		pptg::matrix matrix{20, 12};
		matrix.add_input(input);
		matrix.add_entropies(argv[argc-2]);
		matrix.shuffle_data();
		matrix.check_data();
		//matrix.log_matrix();
		matrix.make_lookup(input);
		matrix.print_lookup();

		pptg::html html{matrix};
		html.write(argv[argc-1]);
	}
	catch (const std::exception & e)
	{
		std::cerr << "ERROR: ======>\n" << e.what() << std::endl;
		return 1;
	}
}

