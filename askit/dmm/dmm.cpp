//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <filesystem>
#include <boost/process.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <regex>
#include <iomanip>
#include <random>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <stdfloat>

using std::string_literals::operator""s;
namespace fs = std::filesystem;
namespace proc = boost::process;
namespace asio = boost::asio;

namespace dmm
{
	const std::float32_t min_hours = 2;
	const std::float32_t max_hours = 4;
	const int limit_high_secs = 1200;
	const int limit_low_secs = 600;

	std::mt19937 rng{std::random_device{}()};

	class quoted
	{
	public:
		std::string operator()(const std::string & input) const
		{
			std::stringstream io;
			io << std::quoted(input);
			return io.str();
		}
	};

	enum class exc_action
	{
		error,
		show_help
	};

	class dmm_error
	{
	private:
		dmm::exc_action __ea;
		std::string __error;
	public:
		dmm_error(const dmm::exc_action ea__, const std::string & err__ = ""):
			__ea{ea__},
			__error{err__}
		{
		}
		dmm::exc_action action() const
		{
			return __ea;
		}
		std::string what() const
		{
			return __error;
		}
	};

	class now
	{
	public:
		std::string operator()() const
		{
			std::string nn = (std::ostringstream{} << std::chrono::system_clock::now()).str();
			while (nn.starts_with("\n"))
				nn = nn.substr(1);
			while (nn.ends_with("\n"))
				nn = nn.substr(0, nn.size()-1);
			return nn;
		}
	};
}	// namespace dmm

namespace dmm
{
	class dear_manager
	{
	private:
		const std::vector<std::string> __args;
	private:
		std::vector<fs::path> __ignore_files{};
		bool __rename_just_called_and_ok{false};
	public:
		dear_manager():
			__args{}
		{
		}
		dear_manager(int argc__, char ** argv__):
			__args{argv__, argv__+argc__}
		{
		}
	public:
		void start()
		{
			this->test_args();
			this->test_dirs();
			this->record_and_test_ignore_files();
			this->test_cmds();
			this->run();
		}
	private:
		void test_args() const
		{
			// test args count
			{
				if (__args.size() < 2)
				{
					throw dmm::dmm_error{
						dmm::exc_action::show_help
					};
				}

				if (__args.size() < 7u)
				{
					throw dmm::dmm_error{
						dmm::exc_action::error,
						"Requires at least 6 args!"
					};
				}
			}

			// test directories
			{
				for (int i=1; i<3; ++i)
				{
					if (! fs::is_directory(__args[i]))
					{
						throw dmm::dmm_error{
							dmm::exc_action::error,
							"Not an existed directory: "s
								+
							dmm::quoted{}(__args[i])
						};
					}
				}
			}
		}
	private:
		void test_dirs() const
		{
			fs::directory_entry source{__args[1]};
			fs::directory_entry work{__args[2]};
			fs::directory_entry upgrade{__args[3]};
			if (source == work || source == upgrade || work == upgrade)
			{
				throw dmm::dmm_error{
					dmm::exc_action::error,
					"Requires source-dir, work-dir, and upgrade-dir not the same dir!"
				};
			}
		}
	private:
		void record_and_test_ignore_files()
		{
			for (auto i=6u; i<__args.size()-1u; ++i)
			{
				__ignore_files.emplace_back(__args[i]);
			}
			std::clog << "Ignore Files Count: " << __ignore_files.size() << std::endl;
			for (const auto & igfn: __ignore_files)
			{
				if ( ! fs::exists(igfn))
					throw dmm::dmm_error{
						dmm::exc_action::error,
						"This file is set to be ignored, but it does not exist: \""s
						+
						igfn.string()
						+ "\""
					};
				std::clog << "This file will be ignored: " << igfn << std::endl;
			}
			std::clog << std::endl;
		}
	private:
		void test_cmds() const
		{
			auto cmd = __args[__args.size()-1];
			auto cmd_exec = proc::environment::find_executable(cmd);
			if (cmd_exec.empty())
			{
				throw dmm::dmm_error{
					dmm::exc_action::error,
					"Command not found: "s + dmm::quoted{}(cmd)
				};
			}

			auto killall = proc::environment::find_executable("killall");
			if (killall.empty())
			{
				throw dmm::dmm_error{
					dmm::exc_action::error,
					"Command not found: "s + dmm::quoted{}("killall")
				};
			}
		}
	private:
		int stoi(const std::string & str) const
		{
			std::string str2 = str;
			while (str2.starts_with("0"))
			{
				str2 = str2.substr(1);
			}

			if (str2.empty())
				return 0;
			try
			{
				int r = std::stoi(str2);
				if (r < 0)
					throw "(1)"s;
				if (str2 != std::to_string(r))
					throw "(2)"s;
				return r;
			}
			catch (const std::string & e)
			{
				throw dmm::dmm_error{
					dmm::exc_action::error,
					"Time format error: conversion error: "s + dmm::quoted{}(str) + "\t" + e
				};
			}
		}
	private:
		// hours, minutes, seconds
		std::tuple<int, int, int> make_interval() const
		{
			std::regex re{"([0-9]*):?([0-9]*):?([0-9]*)"};
			std::smatch sm;
			bool status = std::regex_match(__args[4], sm, re);
			if (! status)
				throw dmm::dmm_error{
					dmm::exc_action::error,
					"<time interval> format Error!"
				};
			int hours{0}, minutes{0}, seconds{0};
			if (sm[2].str().empty())
			{
				seconds = this->stoi(sm[1]);
			}
			else if (sm[3].str().empty())
			{
				minutes = this->stoi(sm[1]);
				seconds = this->stoi(sm[2]);
			}
			else
			{
				hours = this->stoi(sm[1]);
				minutes = this->stoi(sm[2]);
				seconds = this->stoi(sm[3]);
			}

			{
				std::float32_t real_hours = hours + minutes / 60.0f32 + seconds / 3600.0f32;
				if (real_hours < dmm::min_hours || real_hours > dmm::max_hours)
				{
					throw dmm::dmm_error{
						dmm::exc_action::error,
						"Time limit:\n"s
							+
						"Min hours: "s + std::to_string(dmm::min_hours) + "\n"
							+
						"Max hours: "s + std::to_string(dmm::max_hours)
					};
				}
			}

			return {
				hours,
				minutes,
				seconds
			};
		}
	private:
		int get_random_seconds() const
		{
			{
				int max_random_seconds = this->stoi(__args[5]);
				if (
					max_random_seconds < dmm::limit_low_secs
					||
					max_random_seconds > dmm::limit_high_secs
				)
				{
					throw dmm::dmm_error{
						dmm::exc_action::error,
						"ERROR: Requires "s
							+
						std::to_string(dmm::limit_high_secs)
							+
						" <=  <Max limit of random time interval>  <= "s
							+
						std::to_string(dmm::limit_high_secs) + " (seconds)"
					};
				}
				return dmm::rng() % max_random_seconds;
			}

		}
	private:
		void run() const
		{
			auto [source, work, upgrade] = std::tuple{__args[1], __args[2], __args[3]};

			{
				for (std::string & str:
					{
						std::ref(source),
						std::ref(work),
						std::ref(upgrade)
					}
				)
				{
					if (! str.ends_with("/"))
						str += "/";
				}
			}

			std::clog << "source-dir, work-dir, upgrade-dir:\n"
				<< source << "\n" << work << "\n" << upgrade << "\n"
				<< std::endl;

			auto [hours, minutes, seconds] = this->make_interval();
			std::clog << "hours, minutes, seconds:\n";
			std::clog << hours << ", " << minutes << ", " << seconds << std::endl << std::endl;

			int random_seconds = this->get_random_seconds();
			std::clog << "First random_seconds:\n" << random_seconds << "\n\n";

			while (true)
			{
				if (this->is_task_finished())
				{
					std::clog << "Task is already finished!" << std::endl;
					return;
				}
#if true
				{
					if (! this->move(work, upgrade))
						this->move(source, work);
				}
#else
				if (dmm::rng()%3 == 0)
				{
					if (! this->move(source, work))
						this->move(work, upgrade);
				}
				else
				{
					if (! this->move(work, upgrade))
						this->move(source, work);
				}
#endif
				this->restart();

				if (__rename_just_called_and_ok)
				{
					const_cast<bool &>(__rename_just_called_and_ok) = false;

					random_seconds = this->get_random_seconds();

					std::clog
						<< "\n\n---\n---\n"
						<< "Next rolling after:\n\t\t"
						<< "hours:minutes:seconds = "
						<< hours << ":" << minutes << ":" << seconds+random_seconds
						<< " ..."
						<< std::endl
					;

					std::this_thread::sleep_for(std::chrono::hours(hours));
					std::this_thread::sleep_for(std::chrono::minutes(minutes));
					std::this_thread::sleep_for(std::chrono::seconds(seconds));
					std::this_thread::sleep_for(std::chrono::seconds(random_seconds));
				}
				else	// else skip to next rolling immediately: no sleep
				{
					std::clog << "\n\nSkipped\n\n";
				}
			}
		}
	private:
		std::vector<fs::directory_entry> get_entries(const std::string & dir) const
		{
			std::vector<fs::directory_entry> entries;
			for (const auto & entry: fs::directory_iterator{dir})
			{
				entries.push_back(entry);
			}
			return entries;
		}
	private:
		// Wrap fs::rename
		void rename (const fs::path from, const fs::path to, std::error_code & ec) const
		{
			contract_assert(! __rename_just_called_and_ok);
			{
				// If the path "from" is found in the ignore file list,
				//		fs::rename will be not executed.
				for (const fs::path & path: __ignore_files)
				{
					if (path.lexically_normal() == from.lexically_normal())
					{
						std::clog
							<< "\n\nOh, Note: This path is in the ignore list:\n"
							<< from
							<< "\n\n"
						;
						ec = std::error_code{};
						return;
					}
				}
			}

			fs::rename(from, to, ec);
			if (! ec)
				const_cast<bool &>(__rename_just_called_and_ok) = true;
		}
	private:
		bool move(const std::string & from, const std::string & to) const
		{
			auto entries = this->get_entries(from);
			if (entries.size() < 1)
			{
				return false;
			}
			auto id = dmm::rng() % entries.size();
			const auto & entry = entries[id];
			std::string msg = "Move from "s + dmm::quoted{}(entry.path().string())
				+ " to " + dmm::quoted{}(to) + " : ";
			std::error_code ec;
			this->rename(entry, to / entry.path().filename(), ec);
			if (ec)
			{
				try
				{
					throw std::system_error{ec};
				}
				catch (const std::exception & e)
				{
					msg += " failed: "s + e.what();
					msg += "  At "s + dmm::now{}();
				}
				throw dmm::dmm_error{
					dmm::exc_action::error,
					msg
				};
			}
			// else
			if (__rename_just_called_and_ok)
			{
				msg += "successful.";
				msg += "  At "s + dmm::now{}();
			}
			else
			{
				msg += "ignored, rolling will be skipped to next immediately.";
				msg += "  At "s + dmm::now{}();
			}
			std::clog << msg << std::endl;
			return true;
		}
		void restart() const
		{
			auto cmd = __args[__args.size()-1];
			auto cmd_exec = proc::environment::find_executable(cmd);
			auto killall = proc::environment::find_executable("killall");

			// stop cmd
			{
				std::clog << "Re-killing " << cmd << ", wait ...\n";
				for (int i=0; i<20; ++i)
				{
					asio::thread_pool pool;
					proc::process proc{
						pool.get_executor(),
						killall,
						{
							cmd_exec.string()
						}
					};
					proc.wait();
					pool.join();
					std::clog
						<< dmm::now{}()
						<< "  "
						<< std::flush;
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				std::clog << "\n\nNote: Killing finished!  " << dmm::now{}()
					<< std::endl
					<< std::endl;
			}

			// start cmd
			{
				asio::thread_pool pool;
				proc::process proc{
					pool.get_executor(),
					cmd_exec,
					{
					}
				};
				std::clog << "Starting ...\n";
				proc.wait();
				std::clog << "Starting ... (2)\n";
				pool.join();
				std::clog << "Started!  At "
					<< dmm::now{}()
					<< std::endl;
			}
		}
		bool is_task_finished() const
		{
			if (
				this->get_entries(__args[1]).size() < 1u
					&&
				this->get_entries(__args[2]).size() < 1u
			)
			{
				return true;
			}
			return false;
		}
	private:
		std::string get_help_i() const
		{
			return std::format(
				R"(
					+++++++++++++++++
					+ dmm
					+++++++++++++++++
					+ Help Menu
					+++++++++++++++++

					command:
					dmm <source dir> <work dir> <upgrade dir>
						<time interval> <Max limit of random time interval>
						[ignore file list]
						<cmd>

					* The option order can not be changed;

					<time interval> format:
							seconds
							minutes:seconds
							hours:minutes:seconds
							(only unsigned int allowed)
						Limit:
							Min: {} hours, Max: {} hours;

						For example:
							324            - 324 seconds
							300:400        - 300 minutes + 400 seconds
							300:400:500    - 300 hours + 400 minutes + 500 seconds

					<Max limit of random time interval> format:
							seconds
							(only unsigned int allowed)
						Limit low: {} secs  ,
						Limit high: {} secs .
				)",
				dmm::min_hours,
				dmm::max_hours,
				dmm::limit_low_secs,
				dmm::limit_high_secs
			);
		}
		std::string shift(const std::string & input, int num) const
		{
			std::stringstream io;
			io << input << std::flush;
			std::string line;
			std::string result;
			std::string start;
			for (int i=0; i<num; ++i)
				start += "\t";
			int ln = 0;
			while (std::getline(io, line))
			{
				++ln;

				if (line.empty())
				{
					result += "\n";
					continue;
				}

				// else

				{
					std::regex re{"[ \t\n\v\r]+"};
					std::smatch sm;
					if (std::regex_match(line, sm, re))
					{
						result += "\n";
						continue;
					}
				}

				// else

				if (! line.starts_with(start))
				{
					throw std::runtime_error{"Format Error, (help) input line: "s + std::to_string(ln)};
				}

				// else

				result += line.substr(num) + "\n";
			}
			return result;
		}
	public:
		std::string get_help() const
		{
			return this->shift(this->get_help_i(), 5);
		}
	};	// class dear_manager
}	// namespace dmm

int main(int argc, char ** argv)
{
	try
	{
		try
		{
			dmm::dear_manager dm{argc, argv};
			dm.start();
			std::clog << "WARNING: Program stopped." << std::endl;
		}
		catch (const dmm::dmm_error & e)
		{
			switch (e.action())
			{
			case dmm::exc_action::error:
				throw std::runtime_error{
					"=====\n"s
					"=====\n"s
					"=====\n"s
					"=====\n"s
						+
					"ERROR:\n"
						+
					e.what() + "\n\n"
				};
			case dmm::exc_action::show_help:
				throw std::runtime_error{
					dmm::dear_manager{}.get_help()
				};
			}
		}
		catch (const std::exception & e)
		{
			throw std::runtime_error{""s + e.what()};
		}
		catch (...)
		{
			throw std::runtime_error{dmm::dear_manager{}.get_help()};
		}
	}
	catch (const std::exception & help)
	{
		std::clog << help.what() << std::endl;
	}
}

