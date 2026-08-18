//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <filesystem>
#include <iostream>
#include <sstream>
#include <regex>
#include <vector>
#include <stdfloat>
#include <iomanip>
#include <format>
#include <thread>
#include <chrono>
#include <mutex>
#include <memory>
#include <random>
#include <future>
#include <functional>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

namespace fs = std::filesystem;
namespace bfs = boost::filesystem;
namespace bpp = boost::process;
namespace asio = boost::asio;

using std::string_literals::operator""s;

// global shared variables.
namespace dmm
{
	constexpr inline int limit_hours = 12;
	std::mutex log_mutex;
	std::mutex transfer_mutex;
	std::mutex restart_mutex;
	inline std::mt19937 rng{std::mt19937{std::random_device{}()}};

	using second_type = unsigned long long;
}

namespace dmm
{
	enum class exception_action
	{
		show_help,
		prompt_help,
		fatal_error,
		ignore,
		none
	};

	class help
	{
	private:
		std::string get_help_i() const;
	private:
		std::string indent(const int left_tab_count) const;
	public:
		std::string get_help() const;
		std::string prompt_help() const;
	};
}	// namespace dmm

namespace dmm
{
	class exception
	{
	private:
		std::string __msg;
		dmm::exception_action __action;
	public:
		exception() = delete;
	public:
		virtual ~exception() noexcept = default;
		exception(
			const std::string & msg__,
			const dmm::exception_action action__
		) noexcept:
			__msg{msg__},
			__action{action__}
		{
		}
	public:
		virtual std::string what() const noexcept
		{
			return __msg;
		}
	public:
		virtual dmm::exception_action action() const noexcept
		{
			return __action;
		}
	private:
		void print_what() const noexcept
		{
			std::clog << "----------------------------------------------------------------------\n";
			std::clog << "----------------------------------------------------------------------\n";
			std::clog << "ERROR:\n";
			std::clog << this->what() << std::endl << std::endl;
		}
	private:
		// Has exception
		void print_i() const
		{
			switch (this->action())
			{
			case dmm::exception_action::show_help:
				std::clog << dmm::help{}.get_help() << std::endl;
				std::clog << std::endl;
				return;
			case dmm::exception_action::prompt_help:
				this->print_what();
				std::clog << dmm::help{}.prompt_help() << std::endl;
				std::clog << std::endl;
				return;
			case dmm::exception_action::fatal_error:
				this->print_what();
				std::clog << "Fatal Error!" << std::endl;
				std::clog << std::endl;
				return;
			case dmm::exception_action::ignore:
			case dmm::exception_action::none:
			default:
				return;
			}
		}
	public:
		//	Return true:	normal message printed.
		//	Return false:	abnormal message printed.
		bool print() const noexcept;
	};

	class help_exception:
		virtual public dmm::exception
	{
	public:
		help_exception(const std::string & msg__) noexcept:
			dmm::exception{msg__, dmm::exception_action::show_help}
		{
		}
	};

	class prompt_help_exception:
		virtual public dmm::exception
	{
	public:
		prompt_help_exception(const std::string & msg__) noexcept:
			dmm::exception{msg__, dmm::exception_action::prompt_help}
		{
		}
	};

	class fatal_error:
		virtual public dmm::exception
	{
	public:
		fatal_error(const std::string & msg__) noexcept:
			dmm::exception{msg__, dmm::exception_action::fatal_error}
		{
		}
	};

	class ignore_error:
		virtual public dmm::exception
	{
	public:
		ignore_error(const std::string & msg__) noexcept:
			dmm::exception{msg__, dmm::exception_action::ignore}
		{
		}
	};
}	// namespace dmm

namespace dmm
{
	class args
	{
	private:
		std::vector<std::string> __args;
	private:
		fs::path __source_dir;
		fs::path __work_dir;
		fs::path __upgrade_dir;
		dmm::second_type __min_seconds{0};
		dmm::second_type __max_seconds{0};
		std::vector<fs::path> __ignore_files;
		bfs::path __cmd;

	public:
		args() = delete;
		args(int argc__, char ** argv__):
			__args{argv__, argv__ + argc__}
		{
			this->start();
		}
		virtual ~args() = default;

//////////////////////////////////////////////////////////////////////
//	begin: result api

	public:
		fs::path source_dir() const {return __source_dir;}
		fs::path work_dir() const {return __work_dir;}
		fs::path upgrade_dir() const {return __upgrade_dir;}
		dmm::second_type min_seconds() const {return __min_seconds;}
		dmm::second_type max_seconds() const {return __max_seconds;}
		std::vector<fs::path> ignore_files() const {return __ignore_files;}
		bfs::path cmd() const {return __cmd;}

//	end: result api
//////////////////////////////////////////////////////////////////////

	private:
		void start()
		{
			this->check_help();
			this->check_and_set_dirs();
			this->check_and_set_times();
			this->check_and_set_cmd();
			// Pre check: un-existed ignore files from args will be skipped without exception.
			this->make_ignore_files();
		}

	public:
		void log() const
		{
			std::unique_lock<std::mutex> lock{dmm::log_mutex};

			std::clog << "----------------------------------------------------------------------\n";
			std::clog << "---- dmm::args, log ----\n";
			{
				std::clog << "<source dir>: " << __source_dir << std::endl;
				std::clog << "<work dir>: " << __work_dir << std::endl;
				std::clog << "<upgrade dir>: " << __upgrade_dir << std::endl;
			}

			{
				std::clog << "Min Time: " << __min_seconds << " seconds"
					<< " (it is " << __min_seconds/60.0 << " minutes)"
					<< std::endl;
				std::clog << "Max Time: " << __max_seconds << " seconds"
					<< " (it is " << __max_seconds/60.0 << " minutes)"
					<< std::endl;
			}

			{
				std::clog << "cmd: " << __cmd << std::endl;
			}

			{
				std::clog << "Ignore Files:\n";
				for (const auto & p: __ignore_files)
					std::clog << "\t" << p << std::endl;
				if (__ignore_files.size() < 1u)
					std::clog << "\t" << "No Ignore Files." << std::endl;
			}
			std::clog << "----------------------------------------------------------------------\n";
		}

	private:
		void check_help() const
		{
			bool is_help = false;

			if (__args.size() < 2u)
				is_help = true;
			else if (__args.size() == 2u)
			{
				std::regex re{"(--?(h|help))|(help)"};
				std::smatch sm;
				if (std::regex_match(__args[1], sm, re))
					is_help = true;
			}

			if (is_help)
				throw dmm::help_exception{""};
		}

//////////////////////////////////////////////////////////////////////
	private:
		void check_and_set_dirs()
		{
			if (__args.size() < 4u)
				throw dmm::prompt_help_exception{
					"Requires <source dir> <work dir> <upgrade dir>"
				};

			this->check_and_set_dir(__source_dir, "<source dir>", __args[1]);
			this->check_and_set_dir(__work_dir, "<work dir>", __args[2]);
			this->check_and_set_dir(__upgrade_dir, "<upgrade dir>", __args[3]);

			this->check_dirs_same();
		}

	private:
		void check_and_set_dir(
			fs::path & dir,
			const std::string & mark,
			const fs::path & path
		)
		{
			if (path.empty())
				throw dmm::prompt_help_exception{
					"Requires "s + mark + " is not empty!"
				};

			if (! fs::exists(path))
				throw dmm::fatal_error{
					mark + " does not exist: " + dmm::args::quoted(path.string())
				};

			if (! fs::is_directory(path))
				throw dmm::fatal_error{
					mark + " is not a directory: " + dmm::args::quoted(path.string())
				};

			dir = dmm::args::normalize_path(path);
		}

	private:
		void check_dirs_same() const
		{
			if (
				__source_dir == __work_dir
				||
				__source_dir == __upgrade_dir
				||
				__work_dir == __upgrade_dir
			)
			{
				throw dmm::fatal_error{
					"Requires <source dir> <work dir> <upgrade dir> are not the same dir."
				};
			}
		}
//////////////////////////////////////////////////////////////////////

	private:
		void check_and_set_times()
		{
			if (__args.size() < 6u)
				throw dmm::prompt_help_exception{
					"Requires <Min Wait Time (minutes)> <Max Wait Time (minutes)>"
				};

			this->check_and_set_time(__min_seconds, "<Min Wait Time (minutes)>", __args[4]);
			this->check_and_set_time(__max_seconds, "<Max Wait Time (minutes)>", __args[5]);

			if (__min_seconds >= __max_seconds)
				throw dmm::fatal_error{
					"Requires \"<Min Wait Time> not equal to or greater than <Max Wait Time>.\""
				};
		}

	private:
		void check_and_set_time(
			dmm::second_type & seconds,
			const std::string & mark,
			const std::string & value
		)
		{
			try
			{
				std::regex re{"([0-9]+)\\.?[0-9]*"};
				std::smatch sm;
				if (! std::regex_match(value, sm, re))
					throw 0;

				const std::string f_msg = 
					"Time too large: "s + dmm::args::quoted(value) + " (minutes)\n"
					+
					"Time Limit: "s + std::to_string(dmm::limit_hours*60) + " (minutes).\n"
				;
				if (std::string{sm[1]}.size() > std::to_string(dmm::limit_hours*60).size())
					throw dmm::fatal_error{f_msg + "(std::regex checked)\n"};

				const std::float32_t v = std::stof(value);
				if (v > dmm::limit_hours * 60)
					throw dmm::fatal_error{f_msg + "(value compare checked)\n"};

				seconds = static_cast<dmm::second_type>(v*60);
			}
			catch (const dmm::exception & e)
			{
				throw e;
			}
			catch (...)
			{
				throw dmm::fatal_error{
					"Invalid time: "s + dmm::args::quoted(value)
					+
					mark + " :\n"
					+
					"\t" + "Requires a non-negative number!"
					+
					"\t" + "Extra chars are not allowed!"
				};
			}
		}

	private:
		void check_and_set_cmd()
		{
			if (__args.size() < 7u)
				throw dmm::prompt_help_exception{
					"Requires a cmd at last option args."
				};
			const std::string __cmd_str = __args[__args.size()-1];
			if (__cmd_str.empty())
				throw dmm::prompt_help_exception{"cmd at last option of args should be not empty!"};
			__cmd = bpp::environment::find_executable(__cmd_str);
			if (__cmd.empty())
				throw dmm::fatal_error{
					"Command not found: "s + dmm::args::quoted(__cmd_str)
					+ "\nLast option of args will be used as command."
				};
		}

	private:
		void make_ignore_files()
		{
			for (auto i=6u; i<__args.size()-1u; ++i)
			{
				this->check_add_ignore_file(__args[i]);
			}
		}

	private:
		const void check_add_ignore_file(const std::string & filename)
		{
			fs::path p{filename};
			if (fs::exists(p))
				__ignore_files.push_back(p);
		}

//////////////////////////////////////////////////////////////////////
	public:
		static fs::path normalize_path(const fs::path & path)
		{
			fs::path p2 = path.lexically_normal();

			// Just return: un-existed pach check is not done here.
			if (! fs::exists(p2))
				return p2;

			std::string str = p2.string();
			if (str != "/" && str.ends_with("/"))
				str = str.substr(0, str.size()-1);

			return fs::path{str};
		}

	public:
		static std::string quoted(const auto & any)
		{
			return (std::ostringstream{} << std::quoted((std::ostringstream{} << any).str())).str();
		}
	};	// class args
}	// namespace dmm

namespace dmm
{
	enum class transfer_id
	{
		src_to_wk,
		wk_to_up
	};
}	// namespace dmm

namespace dmm
{
	class transfer_manager:
		virtual public std::enable_shared_from_this<dmm::transfer_manager>
	{
	private:
		const dmm::transfer_id __id;
		const std::string __id_string;
		const std::shared_ptr<dmm::args> __args;
	private:
		const fs::path __retrieve_dir{};
		const fs::path __dest_dir{};
	private:
		std::promise<void> __promise;
		bool __delay_thread;
	public:
		transfer_manager() = delete;
		transfer_manager(
			const dmm::transfer_id id__,
			std::shared_ptr<dmm::args> args__,
			std::promise<void> && promise__,
			const bool delay_thread__
		):
			__id{id__},
			__args{args__},
			__promise{std::move(promise__)},
			__delay_thread{delay_thread__}
		{
			switch (__id)
			{
			case dmm::transfer_id::src_to_wk:
				const_cast<fs::path &>(__retrieve_dir) = __args->source_dir();
				const_cast<fs::path &>(__dest_dir) = __args->work_dir();
				const_cast<std::string &>(__id_string) = "[source dir to work dir]";
				break;
			case dmm::transfer_id::wk_to_up:
				const_cast<fs::path &>(__retrieve_dir) = __args->work_dir();
				const_cast<fs::path &>(__dest_dir) = __args->upgrade_dir();
				const_cast<std::string &>(__id_string) = "[work dir to upgrade dir]";
				break;
			default:
				throw dmm::fatal_error{
					"Might be corrupted code: no such id of dmm::transfer_id:\n"s
					+
					std::to_string(static_cast<int>(__id))
					+
					"\n"
				};
			}
		}
		virtual ~transfer_manager()
		{
		}
	public:
		void start()
		{
			{
				if (__delay_thread)
				{
					dmm::second_type time = 27u*60u;
					{
						{
							std::unique_lock<std::mutex> lock{dmm::log_mutex};
							std::clog
								<< "\n"
								<< __id_string << " will be delayed for "
								<< time / 60.0 << " minutes ..."
								<< std::endl
							;
						}
						std::this_thread::sleep_for(std::chrono::seconds(time));

						this->wait();
					}
				}
			}

			try
			{
				this->run();
				__promise.set_value();
			}
			catch (...)
			{
				__promise.set_exception(std::current_exception());
			}
		}
	private:
		void run()
		{
			while (true)
			{
				if (this->transfer())
				{
					this->restart();
				}
				else
				{
					std::unique_lock<std::mutex> lock{dmm::log_mutex};
					std::clog
						<< __id_string << ": all tasks are finished! Next cycling ..."
						<< std::endl;
					lock.unlock();
				}
				this->wait();
			}
		}
	private:
		void wait() const
		{
			dmm::second_type seconds =
				dmm::rng() % (__args->max_seconds() - __args->min_seconds() + 1)
				+
				__args->min_seconds()
			;

			std::unique_lock<std::mutex> lock{dmm::log_mutex};
			std::clog << __id_string << ": Next restart: please Wait for " << seconds/60.0 << " minutes ..." << std::endl << std::endl;
			lock.unlock();

			const dmm::second_type hint_interval = 300;	// 5 minutes

			while (seconds > hint_interval)
			{
				lock.lock();
				std::clog << __id_string << " left " << seconds/60.0 << " minutes ...\n";
				lock.unlock();
				seconds -= hint_interval;
				std::this_thread::sleep_for(std::chrono::seconds(hint_interval));
			}

			if (seconds > 0)
			{
				lock.lock();
				std::clog << __id_string << " left " << seconds/60.0 << " minutes ...\n";
				lock.unlock();
				std::this_thread::sleep_for(std::chrono::seconds(seconds));
			}
		}
	private:
		// Return false: should stop this thread
		bool transfer()
		{
			fs::path path;

			{
				const auto retrieve_list = this->retrieve_dir(__retrieve_dir);
				if (retrieve_list.size() < 1u)
					return false;

				bool found = false;

				// First, randomly try 20 times
				for (int i=0; i<20; ++i)
				{
					int index = dmm::rng() % retrieve_list.size();
					path = retrieve_list[index];
					if (fs::exists(path))
					{
						found = true;
						break;
					}
				}

				if (! found)
				{
					// Randomly tried failed, try one by one
					for (const auto & pp: retrieve_list)
					{
						if (fs::exists(pp))
						{
							found = true;
							path = pp;
							break;
						}
					}
				}

				if (! found)
					return false;
			}

			return this->move_file(path);
		}

	private:
		// Return false: should stop
		bool move_file(const fs::path & path)
		{
			if (! fs::exists(__dest_dir) || ! fs::is_directory(__dest_dir))
			{
				std::unique_lock<std::mutex> lock{dmm::log_mutex};
				std::clog << __id_string << ": __dest_dir is not a valid dir!" << std::endl;
				lock.unlock();
				return false;
			}

			const fs::path dest_file{__dest_dir / path.filename()};

			if (fs::exists(dest_file))
			{
				throw dmm::fatal_error{
					"Fatal Error, file duplicated, I cannot override it, requires fixing by a human:\n"s
					+
					"source: " + dmm::args::quoted(path.string()) + "\n"
					+
					"destination: " + dmm::args::quoted(dest_file.string()) + "\n"
				};
			}

			{
				std::error_code ec;

				{
					std::unique_lock<std::mutex> lock{dmm::transfer_mutex};
					fs::rename(path, dest_file, ec);
				}

				if (ec)
					return false;
				else
				{
					std::unique_lock<std::mutex> lock{dmm::log_mutex};
					std::clog << "\n\n==================================================\n";
					std::clog << "|| " << __id_string << ", file moved successfully:\n||\t"
						<< path << "\t=>\t" << dest_file << std::endl;
					std::clog << "==================================================\n";

					return true;
				}
			}

			return true;
		}

	private:
		void restart()
		{
			{ // Check cmd again.
				if (__args->cmd().empty())
				{
					throw dmm::fatal_error{
						"Command not found (2): "s + dmm::args::quoted(__args->cmd().string())
					};
				}
			}

			{ // Stop
				auto killall = bpp::environment::find_executable("killall");
				if (killall.empty())
					throw dmm::fatal_error{
						"Command not found: "s + dmm::args::quoted("killall")
					};

				{
					std::vector<int> status_list;

					std::string out_string;
					std::string err_string;

					{
						std::unique_lock<std::mutex> lock{dmm::restart_mutex};
						for (int i=0; i<25; ++i)
						{
							boost::system::error_code ec;
							asio::thread_pool pool;
							asio::readable_pipe rp_out{pool.get_executor()};
							asio::readable_pipe rp_err{pool.get_executor()};
							bpp::process proc{
								pool.get_executor(),
								killall,
								{
									__args->cmd().string()
								},
								bpp::process_stdio
								{
									.out = rp_out,
									.err = rp_err
								},
								ec
							};
							std::this_thread::sleep_for(std::chrono::milliseconds(150));
							int status = proc.wait();

							asio::read(
								rp_out,
								asio::dynamic_buffer(out_string),
								ec
							);
							asio::read(
								rp_err,
								asio::dynamic_buffer(err_string),
								ec
							);

							pool.join();

							status_list.push_back(status);
						}
					}

					{
						std::unique_lock<std::mutex> lock{dmm::restart_mutex};
						std::clog << __id_string << "=>\nTrying to stop "
							<< __args->cmd() << " ... ";
						bool status = false;
						{
							bool b1 = false, b2 = false;
							for (int x: status_list)
							{
								std::clog << x << " ";
								if (x == 0)
									b1 = true;
								if (x == 1)
									b2 = true;
							}
							status = b1 && b2;
						}
						std::clog << std::endl;
						std::clog << "Killed: " << (status?"good":"weak");
						if (! status)
						{
							std::clog
								<< "; last out log: "
								<< (out_string.empty()?"empty"s:out_string)
							;
							std::clog
								<< "; last err log: "
								<< (err_string.empty()?"empty"s:err_string)
							;
						}
						std::clog << std::endl;
					}
				}
			}	// end: stop

			{ // Start
				int status;
				int i;

				{
					std::unique_lock<std::mutex> lock{dmm::restart_mutex};

					for (i=1; i<50; ++i)
					{
						boost::system::error_code ec;
						asio::thread_pool pool;
						bpp::process proc{
							pool.get_executor(),
							__args->cmd(),
							{
							},
							ec
						};
						std::this_thread::sleep_for(std::chrono::milliseconds(150));
						status = proc.wait();
						pool.join();
						if (status == 0)
							break;
					}
				}

				{
					std::unique_lock<std::mutex> lock{dmm::log_mutex};

					std::clog << "##################################################\n";
					std::clog << "# " << __id_string << "=>\n# Restart " << __args->cmd()
						<< ((status==0)?" OK:":" failed:")
						<< "\tTried " << i << " times!\n";
					std::clog << "##################################################\n";
					std::clog << "\n\n"
						<< __id_string << " Restarting task at: " << std::chrono::system_clock::now()
						<< std::endl;
				}
			}
		}

	private:
		bool is_ignore_file(const fs::path & path) const
		{
			if (! fs::exists(path))
				return true;
			for (const fs::path & pp: __args->ignore_files())
			{
				if (dmm::args::normalize_path(pp) == dmm::args::normalize_path(path))
					return true;
			}
			return false;
		}
	private:
	// Real-time retrieve
		std::vector<fs::path> retrieve_dir(const fs::path & dir) const
		{
			std::vector<fs::path> paths;
			for (const fs::directory_entry & entry: fs::directory_iterator{dir})
			{
				fs::path pp{entry};
				if (! this->is_ignore_file(pp))
					paths.push_back(pp);
			}
			return paths;
		}
	};	// class transfer_manager
}	// namespace dmm

namespace dmm
{
	class transfer_man:
		virtual public std::enable_shared_from_this<dmm::transfer_man>
	{
	private:
		const dmm::transfer_id __id;
		const std::shared_ptr<dmm::args> __args;
		std::thread __start;
	private:
		std::future<void> __future;
		bool __delay_thread;
	public:
		transfer_man(
			const dmm::transfer_id & id__,
			const std::shared_ptr<dmm::args> args__,
			const bool delay_thread__
		):
			__id{id__},
			__args{args__},
			__delay_thread{delay_thread__}
		{
		}
		transfer_man() = delete;
		virtual ~transfer_man()
		{
		}
	public:
		void start()
		{
			std::promise<void> promise;
			__future = promise.get_future();
			__start = std::thread{
				&dmm::transfer_manager::start,
				std::make_shared<dmm::transfer_manager>(
					__id,
					__args,
					std::move(promise),
					__delay_thread
				)
			};
		}
	public:
		void join()
		{
			if (__start.joinable())
				__start.join();
		}
	public:
		void future_get()
		{
			__future.get();
		}
	};
}	// namespace dmm

namespace dmm
{
	class dear_manager
	{
	private:
		const std::shared_ptr<dmm::args> __args;
	private:
		std::shared_ptr<dmm::transfer_man> __to_work;
		std::shared_ptr<dmm::transfer_man> __to_upgrade;
	public:
		dear_manager(
			std::shared_ptr<dmm::args> args__
		):
			__args{args__},
			__to_work{
				std::make_shared<dmm::transfer_man>(
					dmm::transfer_id::src_to_wk,
					__args,
					false
				)
			},
			__to_upgrade{
				std::make_shared<dmm::transfer_man>(
					dmm::transfer_id::wk_to_up,
					__args,
					true
				)
			}
		{
		}
		virtual ~dear_manager() = default;
		dear_manager() = delete;
	public:
		void start()
		{
			__to_work->start();
			__to_upgrade->start();

			// (1)
			__to_work->join();
			__to_upgrade->join();

			// (2)
			// Be careful: these two lines must be placed after (1),
			//	otherwise exception thrown from one thread will corrupt another thread.
			__to_work->future_get();
			__to_upgrade->future_get();
		}
	};	// class dear_manager
}	// namespace dmm

namespace dmm
{
	std::string dmm::help::get_help_i() const
	{
		return std::format(
			R"(
				----------------------------------------------------------------------
				| dmm - Dear Manager of Process.
				|
				| Help Menu
				----------------------------------------------------------------------

				Commands:

				dmm <source dir> <work dir> <upgrade dir>
						<Min Wait Time (minutes)> <Max Wait Time (minutes)>
						[Path / To / Ignore Files]
						<cmd>

				dmm [(--?(h|help))|(help)]

				Note:
				* <source dir> <work dir> <upgrade dir> should not be the same dir.
				* <Min Wait Time> must be not equal to or greater than <Max Wait Time>,
				* <Min Wait Time>
					and
					<Max Wait Time>
					:
						should be valid non-negative numbers,
						floating is accepted.
						do not add extra chars.
				* <Max Wait Time> limit:
					{} minutes
					(it is {} hours or {} seconds)
			)",
			dmm::limit_hours * 60,
			dmm::limit_hours,
			dmm::limit_hours * 60 * 60
		);
	}

	std::string dmm::help::prompt_help() const
	{
		return "Get Help:\ndmm [(--?(h|help))|help]\n";
		
		return
			"Get Help: "s
			+
			"dmm [(--?(h|help))|(help)]"
		;
	}

	std::string dmm::help::indent(const int left_tab_count) const
	{
		std::string result;
		std::string line;
		std::stringstream io;
		io << this->get_help_i();
		while (std::getline(io, line))
		{
			{
				std::regex re{"[ \t\n\r\v]*"};
				std::smatch sm;
				if (std::regex_match(line, sm, re))
				{
					result += "\n";
					continue;
				}
			}

			{
				std::string ire;
				for (int i=0; i<left_tab_count; ++i)
					ire += "\t";
				if (! line.starts_with(ire))
					throw dmm::fatal_error{"Help Text Cracked."};

				result += line.substr(left_tab_count) + "\n";
			}
		}

		return result;
	}

	std::string dmm::help::get_help() const
	{
		return this->indent(4);
	}

	//	Return true:	normal message printed.
	//	Return false:	abnormal message printed.
	bool dmm::exception::print() const noexcept
	{
		try
		{
			try
			{
				std::unique_lock<std::mutex> lock{dmm::log_mutex};
				this->print_i();
			}
			catch (...)
			{
				throw dmm::fatal_error{"ERROR: Print exception message failed."};
			}
		}
		catch (const dmm::exception & e)
		{
			try
			{
				std::unique_lock<std::mutex> lock{dmm::log_mutex};
				e.print_i();
			}
			catch (...)
			{
			}
			return false;
		}
		return true;
	}
}	// namespace dmm

int main(int argc, char ** argv)
{
	std::cout << std::boolalpha;
	std::cerr << std::boolalpha;
	std::clog << std::boolalpha;
	try
	{
		try
		{
			auto args = std::make_shared<dmm::args>(argc, argv);

			args->log();

			dmm::dear_manager dp{args};
			dp.start();
		}
		catch (const dmm::exception & e)
		{
			throw e;
		}
		catch (const std::system_error & e)
		{
			throw dmm::fatal_error{
				"std::system_error,\n"s
				+
				"code: " + (std::ostringstream{} <<e.code()).str() + "\n"
				+
				"what: " + e.what() + "\n"
			};
		}
		catch (const std::exception & e)
		{
			throw dmm::fatal_error{
				"std::exception,\n"s
				+
				"what: " + e.what()
			};
		}
		catch (...)
		{
			throw dmm::fatal_error{"unknown exception."};
		}
	}
	catch (const dmm::exception & e)
	{
		[[maybe_unused]] bool status = e.print();
	}
}

