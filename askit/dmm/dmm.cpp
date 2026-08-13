//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <filesystem>
#include <iostream>
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <regex>
#include <iomanip>
#include <sstream>
#include <vector>
#include <random>
#include <chrono>


using std::string_literals::operator""s;
namespace fs = std::filesystem;
namespace bfs = boost::filesystem;
namespace bpp = boost::process;
namespace asio = boost::asio;

namespace dmm
{
	constexpr std::float32_t min_hours = 0.5f32;	// 0:30:0
	constexpr std::float32_t max_hours = 12.0f32;	// 12:0:0
	constexpr int limit_low_minutes = 10;	// 0:10:0
	constexpr int limit_high_minutes = 720;	// 12:0:0

	const std::string prompt_help = R"(dmm [(--?(h|help))|help])";

	inline std::mt19937 rng{std::mt19937{std::random_device{}()}()};
}	// namespace dmm

namespace dmm
{
	// string MANager, path MANager.
	class man
	{
	public:
		static std::string to_string(const auto & value)
		{
			return (std::ostringstream{} << value).str();
		}
		static std::string quoted(const auto & value)
		{
			return (std::ostringstream{} << std::quoted(dmm::man::to_string(value))).str();
		}
	public:
		static fs::path normalize_path(const fs::path & path)
		{
			if (! fs::exists(path))
				throw std::runtime_error{
					"This program requires all paths exist! (error in dmm::man::normalize_path) .\n"s
					+
					"This path does not exist:\n"
					+
					dmm::man::to_string(path)
				};
			std::string tmp = path.lexically_normal().string();
			if (tmp != "/" && tmp.ends_with("/"))
			{
				tmp = tmp.substr(0, tmp.size()-1);
			}
			return fs::path{tmp};
		}
	};
}	// namespace dmm

namespace dmm
{
	class help
	{
	public:
		static std::string get_help_i()
		{
			return std::format(
				R"(
					++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					+ dmm
					+++++++++++++++++
					+ Help Menu
					++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

					command:
					dmm <source dir> <work dir> <upgrade dir>
						<time interval> <Max limit of random time interval>
						[ignore file list]
						<cmd>

					* The option order can not be changed;

					Get Help:
					{}

					++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

					<time interval> format:
							hours:minutes:seconds
							(only positive int allowed)
						Limit:
							Min: {} hours, Max: {} hours;

						For example:
							300:400:500    - 300 hours + 400 minutes + 500 seconds

					<Max limit of random time interval> format:
							minutes
							(only positive int allowed)
						Limit low: {} minutes ,
						Limit high: {} minutes .
				)",
				dmm::prompt_help,
				dmm::min_hours,
				dmm::max_hours,
				dmm::limit_low_minutes,
				dmm::limit_high_minutes
			);
		}
		static std::string shift(const std::string & input, int num)
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
		static std::string get_help()
		{
			return dmm::help::shift(dmm::help::get_help_i(), 5);
		}
	};	// class help
}	// namespace dmm

namespace dmm
{
	enum class exception_action
	{
		fatal_error,
		help,
		prompt_help,
		ignore,	// ignore this exception
		processed,	// exception processed (but not ignore)
		unknown
	};

	class dmm_error
	{
	private:
		const dmm::exception_action __action;
		const std::string __msg;
	public:
		dmm_error() = delete;
		dmm_error(
			const dmm::exception_action action__,
			const std::string & msg__
		) noexcept:
			__action{action__},
			__msg{msg__}
		{
		}
		virtual ~dmm_error() noexcept
		{
		}
	public:
		dmm::exception_action action() const noexcept
		{
			return __action;
		}
		std::string what() const
		{
			return __msg;
		}
	public:
		void print() const noexcept
		{
			switch (this->action())
			{
			case dmm::exception_action::fatal_error:
				this->print_what();
				return;
			case dmm::exception_action::help:
				std::cerr << dmm::help::get_help();
				return;
			case dmm::exception_action::prompt_help:
				this->print_what();
				std::cerr << "\nGet Help:\n" << dmm::prompt_help << std::endl;
				break;
			case dmm::exception_action::ignore:
			case dmm::exception_action::processed:
				return;
			case dmm::exception_action::unknown:
			default:
				this->print_what();
			}
		}
	private:
		void print_what() const noexcept
		{
			std::cerr << "==============================\n";
			std::cerr << "==============================\n";
			std::cerr << "ERROR:\n";
			std::cerr << this->what() << std::endl;
		}
	};

	class fatal_error:
		virtual public dmm::dmm_error
	{
	public:
		fatal_error(const std::string & msg__) noexcept:
			dmm::dmm_error{dmm::exception_action::fatal_error, msg__}
		{
		}
	};

	class help_exception:
		virtual public dmm::dmm_error
	{
	public:
		help_exception() noexcept:
			dmm::dmm_error{dmm::exception_action::help, ""}
		{
		}
	};

	class prompt_help_exception:
		virtual public dmm::dmm_error
	{
	public:
		prompt_help_exception(const std::string & tips__) noexcept:
			dmm::dmm_error{dmm::exception_action::prompt_help, tips__}
		{
		}
	};

	class processed_exception:
		virtual public dmm::dmm_error
	{
	public:
		processed_exception():
			dmm::dmm_error{dmm::exception_action::processed, ""}
		{
		}
	};
}	// namespace dmm

namespace dmm
{
	class args
	{
	private:
		const std::vector<std::string> __args;
	private:
		std::string __source_dir{};
		std::string __work_dir{};
		std::string __upgrade_dir{};

		std::string __interval{};
		int __hours{}, __minutes{}, __seconds{};

		std::string __max_interval_limit_random{};
		std::float32_t __random_minutes_limit{};

		std::string __cmd_string{};
		bfs::path __cmd{};

		std::vector<fs::path> __ignore_files{};
	public:
		args() = delete;
		args(int argc__, char ** argv__):
			__args{argv__, argv__ + argc__}
		{
			this->do_args();
		}
		virtual ~args()
		{
		}
	public:
		void print_log() const
		{
			std::clog << "args count: " << __args.size() << std::endl;
			std::clog << "__source_dir: " << __source_dir << std::endl;
			std::clog << "__work_dir: " << __work_dir << std::endl;
			std::clog << "__upgrade_dir: " << __upgrade_dir << std::endl;
			std::clog << "__interval: " << __interval << std::endl;
			std::clog << "\t__hours: " << __hours << std::endl;
			std::clog << "\t__minutes: " << __minutes << std::endl;
			std::clog << "\t__seconds : " << __seconds << std::endl;
			std::clog << "__max_interval_limit_random: " << __max_interval_limit_random << std::endl;
			std::clog << "\t__random_minutes_limit: " << __random_minutes_limit << std::endl;
			std::clog << "__ignore_files:\n";
			for (const auto & path: __ignore_files)
				std::clog << "\t" << path << std::endl;
			std::clog << "__cmd_string: " << __cmd_string << std::endl;
			std::clog << "\t__cmd: " << __cmd << std::endl;
		}
	public:
		std::string source_dir() const {return __source_dir;}
		std::string work_dir() const {return __work_dir;}
		std::string upgrade_dir() const {return __upgrade_dir;}
		int hours() const {return __hours;}
		int minutes() const {return __minutes;}
		int seconds() const {return __seconds;}
		std::float32_t random_minutes_limit() const {return __random_minutes_limit;}
		bfs::path cmd() const {return __cmd;}
		std::string cmd_string() const {return __cmd_string;}
		const std::vector<fs::path> & ignore_files() const {return __ignore_files;}
	private:
		void do_args()
		{
			this->check_help();
			this->set_parameters();
			this->check_parameters();
			this->check_and_make_times();
			this->check_and_make_cmd();
			this->check_ignore_files();
		}
	private:
		void check_help() const
		{
			if (__args.size() < 2u)
				throw dmm::help_exception{};
			if (__args.size() == 2u)
			{
				std::regex re{"((--?(h|help))|help)"};
				std::smatch sm;
				if (std::regex_match(__args[1], sm, re))
					throw dmm::help_exception{};
			}
		}
	private:
		void set_parameters()
		{
			this->set_value(__source_dir, 1);
			this->set_value(__work_dir, 2);
			this->set_value(__upgrade_dir, 3);
			this->set_value(__interval, 4);
			this->set_value(__max_interval_limit_random, 5);
			this->set_value(__cmd_string, __args.size()-1);
			this->set_ignore_files(6, __args.size() - 2);
		}
	private:
		void set_value(std::string & variable, int index)
		{
			if (index >= (int)__args.size())
				throw dmm::prompt_help_exception{
					"The " + dmm::man::to_string(index) + "th parameter is not specifiled,\n"
					+
					"All options are required."
				};
			variable = __args[index];
		}
	private:
		void check_parameters() const
		{
			this->check_dirs();
		}
	private:
		void check_dirs() const
		{
			this->dir_exists("__source_dir", __source_dir);
			this->dir_exists("__work_dir", __work_dir);
			this->dir_exists("__upgrade_dir", __upgrade_dir);

			{
				fs::path d1 = dmm::man::normalize_path(__source_dir);
				fs::path d2 = dmm::man::normalize_path(__work_dir);
				fs::path d3 = dmm::man::normalize_path(__upgrade_dir);
				if (d1 == d2 || d1 == d3 || d2 == d3)
				{
					throw dmm::prompt_help_exception{
						"Requires __source_dir, __work_dir, __upgrade_dir not the same dir!"
					};
				}
			}
		}
	private:
		void dir_exists(const std::string & var_name, const std::string & var_value) const
		{
			if (! fs::exists(var_value))
				throw dmm::prompt_help_exception{
					"The "s
					+
					var_name
					+
					" does not exist: "
					+
					dmm::man::quoted(var_value)
				};

			if (! fs::is_directory(var_value))
				throw dmm::prompt_help_exception{
					"The "s
					+
					var_name
					+
					" is not a directory: "
					+
					dmm::man::quoted(var_value)
				};
		}
	private:
		void check_and_make_times()
		{
			this->check_and_make_interval();
			this->check_and_make_max_random_minutes();
		}
	private:
		void check_and_make_interval()
		{
			std::regex re{"([0-9]+):([0-9]+):([0-9]+)"};
			std::smatch sm;
			if (! std::regex_match(__interval, sm, re))
				throw dmm::prompt_help_exception{
					"<time interval> format error!"
				};

			__hours = std::stoi(sm[1]);
			__minutes = std::stoi(sm[2]);
			__seconds = std::stoi(sm[3]);

			{
				auto total_hours =
					__hours +
					__minutes/60.0
					+
					__seconds/3600.0
				;
				if (total_hours < dmm::min_hours || total_hours > dmm::max_hours)
					throw dmm::prompt_help_exception{
						"Total hours of <time interval> is out of range!"
					};
			}
		}
	private:
		void check_and_make_max_random_minutes()
		{
			{
				std::regex re{"[0-9]+"};
				std::smatch sm;
				if (! std::regex_match(__max_interval_limit_random, sm, re))
					throw dmm::prompt_help_exception{
						"Requires <Max limit of random time interval> is a positive integral!"
					};
			}
			__random_minutes_limit = std::stoi(__max_interval_limit_random);
			if (
				__random_minutes_limit < dmm::limit_low_minutes
				||
				__random_minutes_limit > dmm::limit_high_minutes
			)
			{
				throw dmm::prompt_help_exception{
					"<Max limit of random time interval> is out of range!"
				};
			}
		}
	private:
		void check_and_make_cmd()
		{
			__cmd = bpp::environment::find_executable(__cmd_string);
			if (__cmd.empty())
				throw dmm::prompt_help_exception{
					"Command not found: "s
					+
					dmm::man::quoted(__cmd_string)
				};
		}
	private:
		void set_ignore_files(int from, int to)
		{
			for (int i=from; i<=to; ++i)
				__ignore_files.emplace_back(__args[i]);
		}
	private:
		void check_ignore_files() const
		{
			for (const fs::path & path: __ignore_files)
			{
				if (! fs::exists(path))
					throw dmm::prompt_help_exception{
						"This file in the list of __ignore_files does not exist:\n"s
						+
						dmm::man::to_string(path)
					};
			}
		}
	};
}	// namespace dmm

namespace dmm
{
	class dear_manager
	{
	private:
		const dmm::args & __args;
	public:
		dear_manager() = delete;
		dear_manager(const dmm::args & args__):
			__args{args__}
		{
		}
		virtual ~dear_manager()
		{
		}
	public:
		void start()
		{
			while (true)
			{
				this->move();
				this->restart();
				this->sleep();
			}
		}
	private:
		void move()
		{
			bool status = true;

			if (dmm::rng()%2 == 0)
			{
				if (! this->source_to_work())
					if (! this->work_to_upgrade())
						status = false;
			}
			else
			{
				if (! this->work_to_upgrade())
					if (! this->source_to_work())
						status = false;
			}

			if (! status)
				throw dmm::fatal_error{
					"No task, all tasks might be finished!"
				};
		}
	private:
		bool is_ignore_file(const fs::path & path) const
		{
			for (const fs::path & pp: __args.ignore_files())
			{
				if (dmm::man::normalize_path(path) == dmm::man::normalize_path(pp))
				{
					std::clog << "Get an ignore file: " << path
						<< "\n\tsearch another file ..." << std::endl;
					return true;
				}
			}
			return false;
		}
	private:
		auto retrieve_list(const fs::path & from_dir) const
		{
			std::vector<fs::path> tmp_list;
			for (
				const fs::directory_entry & entry:
					fs::directory_iterator{from_dir}
			)
			{
				if (! this->is_ignore_file(entry))
					tmp_list.emplace_back(entry);
			}
			return tmp_list;
		}
	private:
		bool do_move(const fs::path & from_dir, const fs::path & to_dir) const
		{
			auto tmp_list = this->retrieve_list(from_dir);	// create tmp_list
			std::clog << "tmp_list:\n";
			for (const auto & pp: tmp_list)
				std::clog << "\t" << pp << std::endl;
			std::clog << "---\n";

			if (tmp_list.empty())
				return false;

			const fs::path move_file = tmp_list[dmm::rng() % tmp_list.size()];
			//	Destroy tmp_list after getting the move_file
			tmp_list = {};

			std::clog << "Prepared move file " << move_file << " to dir "
				<< to_dir << std::endl;

			return this->rename(move_file, to_dir);
		}
	private:
		bool rename(const fs::path & from_name, const fs::path & to_dir) const
		{
			if (! fs::exists(from_name))
				throw dmm::fatal_error{
					"Bad: this should not happen:\n"s
					+
					"the path to be moved does not exist:\n"
					+
					dmm::man::to_string(from_name)
				};
			if (! fs::exists(to_dir) || ! fs::is_directory(to_dir))
				throw dmm::fatal_error{
					"Bad: this should not happen:\n"s
					+
					"The destination dir to be moved to is invalid:\n"s
					+
					dmm::man::to_string(to_dir)
				};
			return this->do_rename(from_name, to_dir/from_name.filename());
		}
		bool do_rename(const fs::path & from_name, const fs::path & to_name) const
		{
			std::error_code ec;
			fs::rename(from_name, to_name, ec);
			std::clog << "**********************************************************************\n";
			if (! ec)
			{
				std::clog
					<< "** Moved OK: "
					<< from_name << "\t=>\t" << to_name
					<<std::endl;
			}
			else
			{
				std::clog
					<< "** Moving Failed: "
					<< from_name << "\t=>\t" << to_name
					<<std::endl;
				;
			}
			std::clog << "**\n** " << std::chrono::system_clock::now() << std::endl;
			std::clog << "**********************************************************************\n";
			return ! ec;
		}
	private:
		bool work_to_upgrade()
		{
			return this->do_move(__args.work_dir(), __args.upgrade_dir());
		}
	private:
		bool source_to_work()
		{
			return this->do_move(__args.source_dir(), __args.work_dir());
		}
	private:
		void restart()
		{
			{
				for (int i=0; i<20; ++i)
				{
					asio::thread_pool pool;
					boost::system::error_code ec;
					auto killall = bpp::environment::find_executable("killall");
					if (killall.empty())
						throw dmm::fatal_error{
							"Command not found: \"killall\""
						};
					bpp::process proc{
						pool.get_executor(),
						killall,
						{
							__args.cmd().string()
						},
						ec
					};
					proc.wait();
					pool.join();
					std::clog << "killall " << __args.cmd() << " ..." << std::endl;
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
			}

			{
				int i;
				for (i=1; i<6; ++i)
				{
					boost::system::error_code ec;
					asio::thread_pool pool;
					bpp::process proc{
						pool.get_executor(),
						__args.cmd(),
						{
						},
						ec
					};
					int status = proc.wait();
					pool.join();
					std::clog << "Trying to restart " << __args.cmd() << "... ";
					std::clog << i << " times ... ";
					if (status == 0 && (! ec || ec == asio::error::eof))
					{
						std::clog << "Successful!";
						break;
					}
				}
				if (i > 5)
					std::clog << "Failed!";
				std::clog << std::endl;
				std::clog << __args.cmd() << " is restarted at "
					<< std::chrono::system_clock::now() << std::endl;
			}
		}
	private:
		void sleep()
		{
			{
				const unsigned long total_seconds =
					__args.hours() * 3600
					+
					__args.minutes() * 60
					+
					__args.seconds()
					+
					dmm::rng() % static_cast<unsigned long>(__args.random_minutes_limit() * 60)
				;

				const unsigned long hours = total_seconds / 3600;

				const unsigned long tmp = total_seconds - hours*3600;

				const unsigned long minutes = tmp / 60;
				const unsigned long seconds = (tmp - minutes * 60);

				std::clog << "Please wait for " << hours << ":" << minutes << ":" << seconds
					<< std::endl;

				std::clog << "(Waiting is started at " << std::chrono::system_clock::now()
					<< ")"
					<< std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(total_seconds));
			}
		}
	};	// class dear_manager
}	// namespace dmm

int main(int argc, char ** argv)
{
	try
	{
		try
		{
			try
			{
				dmm::args args{argc, argv};
				args.print_log();
				std::clog << "---------------------------------------------------------------\n";
				dmm::dear_manager dm{args};
				dm.start();
			}
			catch (const dmm::dmm_error & e)
			{
				throw e;
			}
			catch (const std::system_error & e)
			{
				throw dmm::fatal_error{
					"Error Code: "s + dmm::man::to_string(e.code()) + "\n"
					+
					"Error: "s + e.what() + "\n"
				};
			}
			catch (const std::exception & e)
			{
				throw dmm::fatal_error{
					"Error: "s + e.what() + "\n"
				};
			}
			catch (...)
			{
				throw dmm::fatal_error{
					"Error: Fatal Error.\n"
				};
			}
		}
		catch (const dmm::dmm_error & e)
		{
			e.print();
			std::cerr << std::endl;
			throw dmm::processed_exception{};
		}
	}
	catch (const dmm::processed_exception & e)
	{
		return 1;
	}
	catch (...)
	{
		std::cerr << "==========\n";
		std::cerr << "==========\n";
		std::cerr << "Error: Bad Error!" << std::endl << std::endl;
		return 2;
	}
}

