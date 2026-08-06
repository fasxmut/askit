//
// Copyright (c) 2026 fasxmut (fasxmut at protonmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <filesystem>
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <sstream>
#include <regex>
#include <vector>
#include <meta>
#include <iomanip>
#include <set>

using std::string_literals::operator""s;
namespace fs = std::filesystem;
namespace bpp = boost::process;
namespace asio = boost::asio;

//////////////////////////////////////////////////////////////////////

namespace dperm
{
	class str_man final
	{
	public:
		static std::string to_string(const auto & value)
		{
			return (std::ostringstream{} << value).str();
		}
	private:
		static std::tuple<std::string, bool> bare_i2(const std::string & value)
		{
			auto is_blanks =
			[]
			(const std::string & str)
			{
				std::regex re{"[ \n\r\v\t]*"};
				std::smatch sm;
				return std::regex_match(str, sm, re);
			};

			if (is_blanks(value))
				return {"", false};

			std::string str = value;
			bool bared = false;
			{
				const std::regex re{"[ \n\r\v\t]*([^ \n\r\v\t].*)"};
				std::smatch sm{};
				if (std::regex_match(str, sm, re))
				{
					std::string tmp = sm[1];
					if (str != tmp)
					{
						str = tmp;
						bared = true;
					}
				}

				if (is_blanks(str))
					return {"", false};
			}
			{
				const std::regex re{"(.*[^ \n\r\v\t])[ \n\r\v\t]*"};
				std::smatch sm{};
				if (std::regex_match(str, sm, re))
				{
					std::string tmp = sm[1];
					if (str != tmp)
					{
						str = tmp;
						bared = true;
					}
				}

				if (is_blanks(str))
					return {"", false};
			}
			return {str, bared};
		}
		static std::tuple<std::string, bool> bare_i3(const std::string & value)
		{
			std::string str = value;
			bool bared = false;
			const std::vector<std::string> quoted{
				R"(")",
				R"(')",
				R"(\")",
				R"(\')"
			};
			for (const std::string & s: quoted)
			{
				if (str.starts_with(s) && str.ends_with(s))
				{
					str = str.substr(s.size(), str.size() - s.size() * 2);
					bared = true;
				}
				// else, continue
			}
			return {str, bared};
		}
	public:
		static std::string bare(const auto & value)
		{
			std::string str = dperm::str_man::to_string(value);
			bool bared1 = true, bared2 = true;
			while (bared1 || bared2)
			{
				{
					auto result = dperm::str_man::bare_i2(str);
					str = std::get<0>(result);
					bared1 = std::get<1>(result);
				}
				{
					auto result = dperm::str_man::bare_i3(str);
					str = std::get<0>(result);
					bared2 = std::get<1>(result);
				}
			}
			return str;
		}
	public:
		static std::string quoted(const auto & value, bool bare_first)
		{
			if (bare_first)
				return dperm::str_man::to_string(std::quoted(dperm::str_man::bare(value)));
			else
				return dperm::str_man::to_string(std::quoted(dperm::str_man::to_string(value)));
		}
	public:
		static std::string normalize_path(const std::string & str)
		{
			std::string tmp = str;

			{
				if (tmp.empty())
					return "";
				const std::regex re{"\\.\\/+([^/]*)"};
				while (true)
				{
					std::smatch sm;
					if (std::regex_match(tmp, sm, re))
						tmp = sm[1];
					else
						break;
				}
				if (tmp.empty())
					return ".";
			}

			{
				const std::regex re{"([^/]*)\\/+\\."};
				while (true)
				{
					std::smatch sm;
					if (std::regex_match(tmp, sm, re))
						tmp = sm[1];
					else
						break;
				}
				if (tmp.empty())
					return "/";
			}

			{
				const std::regex re{"([^/]*)\\/+"};
				while (true)
				{
					std::smatch sm;
					if (std::regex_match(tmp, sm, re))
						tmp = sm[1];
					else
						break;
				}
				if (tmp.empty())
					return "/";
			}

			return tmp;
		}
	};	// class str_man
}	// namespace dperm

namespace dperm
{
	// Forward-declarations
	class help
	{
	private:
		std::string help_info() const;
	private:
		std::string indent(int count) const;
	public:
		std::string get_help() const;
	public:
		std::string get_prompt_help() const;
	};	// class help

	enum class exception_action
	{
		fatal,
		help,
		error_help,
		error_prompt_help,
		ignore
	};

	enum class mode
	{
		def,	// default
		def_r,	// default, then remove exec for files
		inh,	// inherit-deduction
		inh_r,	// inherit-deduction, then remove exec for files
		remove,	// remove exec for files only
		none
	};
}	// namespace dperm

//////////////////////////////////////////////////////////////////////

namespace dperm
{
	class exception
	{
	private:
		const dperm::exception_action __action;
		const std::string __msg;
		const dperm::help __help;
	public:
		exception() noexcept = delete;
		exception(
			const dperm::exception_action action__,
			const std::string & msg__
		) noexcept:
			__action{action__},
			__msg{msg__},
			__help{}
		{
		}
		virtual ~exception() noexcept
		{
		}
	public:
		dperm::exception_action action() const noexcept
		{
			return __action;
		}
		std::string what() const noexcept
		{
			return __msg;
		}
	public:
		bool print(std::ostream & out) const	// has exception
		{
			switch (this->action())
			{
			case dperm::exception_action::fatal:
				out
					<< "==========\n"
					<< "==========\n"
					<< "==========\n"
					<< "Fatal Error:\n"
					<< this->what() << "\n\n"
				;
				return true;
			case dperm::exception_action::help:
				out << __help.get_help() << "\n\n";
				return true;
			case dperm::exception_action::error_help:
				out
					<< "==========\n"
					<< "==========\n"
					<< "==========\n"
					<< "Error:\n"
					<< this->what() << "\n\n"
					<< __help.get_help() << "\n\n"
				;
				return true;
			case dperm::exception_action::error_prompt_help:
				out
					<< "==========\n"
					<< "==========\n"
					<< "==========\n"
					<< "Error:\n"
					<< this->what() << "\n\n"
					<< __help.get_prompt_help() << "\n\n"
				;
				return true;
			case dperm::exception_action::ignore:
				out
					<< "==========\n"
					<< "==========\n"
					<< "==========\n"
					<< "Warning:\n"
					<< this->what() << "\n\n"
				;
				return false;
			}
			out << "Bad Error.\n\n";
			return true;
		}
	};

	class fatal_error:
		virtual public dperm::exception
	{
	public:
		fatal_error(const std::string & msg__) noexcept:
			dperm::exception{
				dperm::exception_action::fatal,
				msg__
			}
		{
		}
	};

	class help_exception:
		virtual public dperm::exception
	{
	public:
		help_exception() noexcept:
			dperm::exception{
				dperm::exception_action::help,
				""
			}
		{
		}
	};

	class error_help_exception:
		virtual public dperm::exception
	{
	public:
		error_help_exception(const std::string & msg__) noexcept:
			dperm::exception{
				dperm::exception_action::error_help,
				msg__
			}
		{
		}
	};

	class error_prompt_help_exception:
		virtual public dperm::exception
	{
	public:
		error_prompt_help_exception(const std::string & msg__) noexcept:
			dperm::exception{
				dperm::exception_action::error_prompt_help,
				msg__
			}
		{
		}
	};

	class ignore_exception:
		virtual public dperm::exception
	{
	public:
		ignore_exception(const std::string & msg__) noexcept:
			dperm::exception{
				dperm::exception_action::ignore,
				msg__
			}
		{
		}
	};
}	// namespace dperm

//////////////////////////////////////////////////////////////////////

namespace dperm
{
	class args:
		virtual public std::enable_shared_from_this<dperm::args>
	{
	private:
		// input
		const std::vector<std::string> __args;
	private:
		// output
		bool __d_opt;
		bool __i_opt;
		bool __r_opt;
		bool __ro_opt;
		bool __verbose;
		bool __ls;
		bool __ln;
		dperm::mode __mode;
		std::shared_ptr<std::set<fs::path>> __path_list;
	public:
		virtual ~args()
		{
		}
	public:
		args() = delete;
		args(int argc__, char ** argv__):
			__args{argv__, argv__ + argc__},
			__d_opt{false},
			__i_opt{false},
			__r_opt{false},
			__verbose{false},
			__ls{false},
			__ln{false},
			__mode{dperm::mode::none},
			__path_list{
				std::make_shared<
					std::set<
						fs::path
					>
				>()
			}
		{
			this->check_help();
			this->check_and_make_args();
			this->check_and_make_mode();
			this->check_path_list();
			if (__verbose)
				this->print_summary();
		}
	public:
		const dperm::mode mode() const
		{
			return __mode;
		}
	public:
		const bool verbose() const
		{
			return __verbose;
		}
	public:
		const bool ls() const
		{
			return __ls;
		}
	public:
		const bool ln() const
		{
			return __ln;
		}
	public:
		const std::shared_ptr<std::set<fs::path>> path_list() const
		{
			return __path_list;
		}
	private:
		void check_and_make_args()
		{
			for (auto i=1u; i<__args.size(); ++i)
			{
				const std::string & t = __args[i];
				bool c1 = this->set_mode(t, "-d", __d_opt);
				bool c2 = this->set_mode(t, "-i", __i_opt);
				bool c3 = this->set_mode(t, "-r", __r_opt);
				bool c4 = this->set_mode(t, "-ro", __ro_opt);
				bool c5 = this->set_mode(t, "-v", __verbose);
				bool c6 = this->set_mode(t, "-ls", __ls);
				bool c7 = this->set_mode(t, "-ln", __ln);
				bool consumed = c1 || c2 || c3 || c4 || c5 || c6 || c7;
				if (! consumed)
				{
					const std::string pp = dperm::str_man::normalize_path(t);
					if (pp == "/")
						throw dperm::fatal_error{
							"The path "s + dperm::str_man::quoted("/", false)
							+
							" is not allowed."
						};
					__path_list->emplace(pp);
				}
			}
		}
	private:
		// Return if arg is consumed
		bool set_mode(
			const std::string & arg,
			const std::string & opt,
			bool & set
		)
		{
			if (arg == opt)
			{
				if (set)
				{
					throw dperm::error_prompt_help_exception{
						opt + " is duplictated."
					};
				}
				// else
				set = true;
				return true;
			}
			return false;
		}
	private:
		void check_help() const
		{
			bool is_help = false;
			if (__args.size() < 2u)
			{
				is_help = true;
			}
			else if (__args.size() == 2u)
			{
				std::regex re{"((--?(h|help))|help)"};
				std::smatch sm;
				if (std::regex_match(__args[1], sm, re))
					is_help = true;
			}
			if (is_help)
				throw dperm::help_exception{};
		}
	private:
		void check_and_make_mode()
		{
		// (1)
			if (__d_opt && __i_opt)
				throw dperm::error_prompt_help_exception{
					"-d and -i are mutex with each other."
				};

			if (__ro_opt)
			{
				if (__d_opt || __i_opt || __r_opt)
					throw dperm::error_prompt_help_exception{
						"-ro is mutex with -d, -i, -r"
					};
			}

		// (2)
			if (__ro_opt)
			{
				__mode = dperm::mode::remove;
				return;
			}

			// else

		// (3)
			if (__i_opt)
			{
				__mode = dperm::mode::inh;
			}
			else
			{
				__mode = dperm::mode::def;
			}

		// (4)
			if (__r_opt)
			{
				switch (__mode)
				{
				case dperm::mode::def:
					__mode = dperm::mode::def_r;
					break;
				case dperm::mode::inh:
					__mode = dperm::mode::inh_r;
					break;
				default:
					__mode = dperm::mode::none;
				}
			}

		// (5)
			if (__mode == dperm::mode::none)
			{
				throw dperm::fatal_error{"ERROR (22112391). "};
			}
		}
	private:
		void check_path_list() const
		{
			for (const fs::path & path: * __path_list)
				if (! fs::exists(path))
					throw dperm::error_prompt_help_exception{
						"This path does not exist: "s + dperm::str_man::quoted(path, true) + "\n"
					};
		}
	private:
		void print_summary() const
		{
			std::clog << "--------------\n";
			std::clog << "__d_opt: " << __d_opt << std::endl;
			std::clog << "__i_opt: " << __i_opt << std::endl;
			std::clog << "__r_opt: " << __r_opt << std::endl;

			std::clog << "Get mode: ";

			{ // print enumerator
				template for (
					constexpr const auto & eid:
						std::define_static_array(
							std::meta::enumerators_of(^^dperm::mode)
						)
				)
				{
					if ([:eid:] == this->mode())
					{
						std::clog << std::meta::identifier_of(eid);
					}
				}
			}
			std::clog << std::endl;

			std::clog << "Get args path list: ";
			for (const fs::path & path: * this->path_list())
				std::clog << dperm::str_man::quoted(path, true) << ", ";
			std::clog << std::endl;
		}
	};
}	// namespace dperm

namespace dperm
{
	class permissions:
		virtual public std::enable_shared_from_this<dperm::permissions>
	{
	private:
		// input
		std::shared_ptr<dperm::args> __args;
	private:
		std::set<fs::path> __path_list;
		std::set<fs::path> __symlink_list;
	private:
		std::string __whoami;
	public:
		permissions(
			std::shared_ptr<dperm::args> args__
		):
			__args{args__}
		{
		}
	public:
		virtual ~permissions()
		{
		}
	public:
		void work()
		{
			if (__args->verbose())
				std::clog << "------------------------------\n";

			this->list_all_paths();

			bool ls_mod = false;
			bool  ln_mod = false;
			// this->if_ls() is optional, if "list symlink" feature is not enabled.
			ls_mod = this->if_ls();

			__whoami = this->get_whoami();

			// this->check_path_owner() is always required,
			//	whenever "list non-owned" feature is enabled.
			ln_mod = this->check_path_owner();

			if (ls_mod || ln_mod)
				return;

			this->change_perms();
		}
	private:
		void list_all_paths()
		{
			for (const fs::path & path: * (__args->path_list()))
			{
				this->list_i(path);
			}
		}
	private:
		void list_i(const fs::path & path)
		{
			if (! fs::exists(path))
				return;

			if (fs::is_symlink(path))
			{
				// symlink will be not processed or followed
				__symlink_list.emplace(dperm::str_man::normalize_path(path.string()));
				return;
			}

			// else

			__path_list.emplace(dperm::str_man::normalize_path(path.string()));

			if (fs::is_directory(path))
			{
				for (const fs::directory_entry & entry: fs::directory_iterator{path})
				{
					this->list_i(entry);
				}
			}

			// else
		}

	private:
		std::string get_whoami() const
		{
			asio::thread_pool pool;
			auto cmd = bpp::environment::find_executable("whoami");
			if (cmd.empty())
				throw dperm::fatal_error{
					"Command not found: "s + dperm::str_man::quoted("whoami", false)
				};
			boost::system::error_code ec;
			asio::readable_pipe rp{pool.get_executor()};
			bpp::process proc{
				pool.get_executor(),
				cmd,
				{
				},
				bpp::process_stdio{
					.out = rp
				},
				ec
			};
			proc.wait();
			if (ec)
				throw std::system_error{ec, "Execute \"whoami\" error!"};
			std::string result;
			asio::read(
				rp,
				asio::dynamic_buffer(result),
				ec
			);
			if (ec && ec != asio::error::eof)
				throw std::system_error{ec, "Can not get \"whoami\" result!"};
			pool.join();
			return dperm::str_man::bare(result);
		}
	private:
		std::string get_path_owner(const fs::path & path) const
		{
			asio::thread_pool pool;
			auto cmd = bpp::environment::find_executable("stat");
			if (cmd.empty())
				throw dperm::fatal_error{
					"Command not found: "s + dperm::str_man::quoted("stat", false)
				};
			boost::system::error_code ec;
			asio::readable_pipe rp{pool.get_executor()};
			bpp::process proc{
				pool.get_executor(),
				cmd,
				{
					"-c",
					"'%U'",
					path.string()
				},
				bpp::process_stdio{
					.out = rp
				},
				ec
			};
			proc.wait();
			if (ec)
				throw std::system_error{ec, "Run command \"stat\" error!"};
			std::string result;
			asio::read(
				rp,
				asio::dynamic_buffer(result),
				ec
			);
			if (ec && ec != asio::error::eof)
				throw std::system_error{ec, "Can not get result from \"stat\""};
			pool.join();
			return dperm::str_man::bare(result);
		}
	private:
		// Return true: ln mode
		bool check_path_owner() const
		{
			if (__args->verbose())
				std::clog << "__whoami: " << dperm::str_man::quoted(__whoami, true)
					<< std::endl;
			int ln_counter = 0;
			for (const fs::path & path: __path_list)
			{
				const std::string owner = this->get_path_owner(path);
				if (owner != __whoami)
				{
					if (__args->ln())
					{
						std::cout << dperm::str_man::quoted(path, true) << std::endl;
						++ln_counter;
					}
					else
					{
						throw dperm::fatal_error{
							"Path list contains a path which is not owned by you: "s
							+
							dperm::str_man::quoted(path, true) + "\n"
							+
							"Path Owner: " + dperm::str_man::quoted(owner, false) + "\n"
							+
							"But you are: " + dperm::str_man::quoted(__whoami, false)
						};
					}
				}
			}
			if (__args->ln())
			{
				if (ln_counter < 1)
					std::cout << "No Non-owned" << std::endl;
				return true;
			}
			return false;
		}	// Method: .check_path_owner

	private:
		bool if_ls() const
		{
			if (__args->ls())
			{
				if (__symlink_list.size() < 1u)
					std::cout << "No Symlink" << std::endl;
				for (const auto & sl: __symlink_list)
				{
					std::cout << dperm::str_man::quoted(sl, true) << std::endl;
				}
				return true;
			}
			return false;
		}

	private:
		void change_perms() const
		{
			for (const fs::path & path: __path_list)
			{
				if (fs::is_symlink(path))
					throw dperm::fatal_error{"ERROR (113321)."};
				switch (__args->mode())
				{
				case dperm::mode::def:
					this->do_def(path);
					break;
				case dperm::mode::def_r:
					this->do_def(path);
					this->do_remove(path);
					break;
				case dperm::mode::inh:
					this->do_inh(path);
					break;
				case dperm::mode::inh_r:
					this->do_inh(path);
					this->do_remove(path);
					break;
				case dperm::mode::remove:
					this->do_remove(path);
					break;
				default:
					throw dperm::fatal_error{
						"ERROR (898231)."
					};
				}
			}
		}
	private:
		void do_def(const fs::path & path) const
		{
			if (fs::is_directory(path))
			{
				std::error_code ec;
				fs::permissions(
					path,
					fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write
					|
					fs::perms::group_exec | fs::perms::group_read
					|
					fs::perms::others_exec | fs::perms::others_read
					,
					fs::perm_options::replace,
					ec
				);
				if (ec)
					throw std::system_error{
						ec,
						"Failed:\n"s
						+
						"Change directory permissions: "s
						+
						dperm::str_man::quoted(path, true)
						+
						"  to 755"
					};
				if (__args->verbose())
					std::clog << "Set dir 755: " << path << std::endl;
				return;
			}
			else
			{
				bool rp = (fs::status(path).permissions() & fs::perms::owner_read) != fs::perms::none;
				bool wp = (fs::status(path).permissions() & fs::perms::owner_write) != fs::perms::none;
				bool xp = (fs::status(path).permissions() & fs::perms::owner_exec) != fs::perms::none;
				fs::perms perms = fs::perms::none;
				if (rp)
				{
					perms |=
						fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read
					;
				}
				if (wp)
				{
					perms |=
						fs::perms::owner_write
					;
				}
				if (xp)
				{
					perms |=
						fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec
					;
				}

				const int value = this->get_perms_value(perms);

				{
					std::error_code ec;
					fs::permissions(
						path,
						perms,
						fs::perm_options::replace,
						ec
					);
					if (ec)
						throw dperm::fatal_error{
							"Failed:\n"
							+
							"change file permssions: "s
							+
							dperm::str_man::quoted(path, true)
							+
							"  to "s
							+
							dperm::str_man::to_string(value)
							+ "\n"
						};
					if (__args->verbose())
						std::clog << "Set file " << value << ": "
							<< dperm::str_man::quoted(path, true)
							<< std::endl;
					return;
				}
			}
		}	// Method .do_def
	private:
		void do_inh(const fs::path & path) const
		{
			const fs::perms oldp = fs::status(path).permissions();
			fs::perms newp =
				(oldp & fs::perms::owner_read)
				|
				(oldp & fs::perms::owner_write)
				|
				(oldp & fs::perms::owner_exec)

				|

				(oldp & fs::perms::others_read)
				|
				(oldp & fs::perms::others_write)
				|
				(oldp & fs::perms::others_exec)
			;

			if ((oldp & fs::perms::others_read) != fs::perms::none)
				newp |= fs::perms::group_read;
			if ((oldp & fs::perms::others_write) != fs::perms::none)
				newp |= fs::perms::group_write;
			if ((oldp & fs::perms::others_exec) != fs::perms::none)
				newp |= fs::perms::group_exec;

			{
				std::error_code ec;
				fs::permissions(
					path,
					newp,
					fs::perm_options::replace,
					ec
				);
				if (ec)
					throw std::system_error{
						ec,
						"Failed:\n"s
						+
						"Change path permssions: "
						+
						dperm::str_man::quoted(path, true)
					};
				if (__args->verbose())
				{
					std::clog
						<< "Set "
						<< (fs::is_directory(path)?"directory ":"file ")
						<< this->get_perms_value(newp)
						<< ": "
						<< dperm::str_man::quoted(path, true) << std::endl;
				}
			}
		}	// Method .do_inh
	private:
		void do_remove(const fs::path & path) const
		{
			if (fs::is_directory(path))
				return;
			const fs::perms perms =
				fs::perms::owner_exec
				|
				fs::perms::group_exec
				|
				fs::perms::others_exec
			;

			{
				std::error_code ec;
				fs::permissions(
					path,
					perms,
					fs::perm_options::remove,
					ec
				);
				if (ec)
					throw std::system_error{
						ec,
						"Failed:\n"s
						+
						"Remove exec permssion from a file: "
						+
						dperm::str_man::quoted(path, true)
					};
				if (__args->verbose())
					std::clog << "Remove exec permission from file: "
						<< dperm::str_man::quoted(path, true)
						<< std::endl;
			}
		}	// Method .do_remove
	private:
		int get_perms_value(const fs::perms perms) const
		{
			int value = 0;
			auto check =
				[&]
				(const fs::perms npp, int add)
				{
					if ((perms & npp) != fs::perms::none)
						value += add;
				}
			;

			check(fs::perms::owner_read, 400);
			check(fs::perms::owner_write, 200);
			check(fs::perms::owner_exec, 100);
			check(fs::perms::group_read, 40);
			check(fs::perms::group_write, 20);
			check(fs::perms::group_exec, 10);
			check(fs::perms::others_read, 4);
			check(fs::perms::others_write, 2);
			check(fs::perms::others_exec, 1);

			return value;
		}
	};	// class permissions
}	// namespace dperm

//////////////////////////////////////////////////////////////////////
// post-definitions

std::string dperm::help::help_info() const
{
	return R"(
		""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
		" dperm -- change to default permssions for lots of paths.
		""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

		Command:

		dperm [-v] [-ls] [-ln] [-d|-i] [-r] [-ro] <path list>
		dperm [-h|--h|-help|--help|help]

		options:
			-v	--- Verbose information;

			-d	--- default mode:
				directory: 755
				file:
					owner's permissions keep unchanged;
					group and others' read/exec permissions inherit owner's;
					group and others' write permissions are removed;

			-i	--- inherit-deduction mode:
				Both directory and file:
					owner's permissions keep unchanged;
					others' permissions keep unchanged;
					group's permissions copy from others';

			-r	--- Remove Exec permission for a file:
				Do -r or -i first, then do -r for a file; directory is not a file;

			-ro	--- Remove exec permissoin only, other options -d, -i, -r are false;

			-ls	--- list symlinks and quit;

			-ln	--- list not owned paths and quit;

			Default: -d;

			-d and -i are mutex with each other;
			-ro is mutex with -d -i -r;

			symlink is always ignored;
	)";
}

std::string dperm::help::indent(int count) const
{
	std::stringstream io;
	io << this->help_info();
	std::string result;
	std::string line;
	while (std::getline(io, line))
	{
		// Process empty line.
		{
			std::regex re{"[ \t]*"};
			std::smatch sm;
			if (std::regex_match(line, sm, re))
			{
				result += "\n";
				continue;
			}
		}

		// Non-empty line.
		{
		// Remove count \t
			std::string re_str;
			for (int i=0; i<count; ++i)
			{
				re_str += "\\t";
			}
			re_str += "(.*)";
			std::regex re{re_str};
			std::smatch sm;
			if (! std::regex_match(line, sm, re))
				throw dperm::fatal_error{"Help text cracked."};

		// test only.
			line = sm[1];
			re = ".*[^ \t]+.*";
			sm = {};
			if (! std::regex_match(line, sm, re))
				throw dperm::fatal_error{"Help text cracked. (2)"};

			result += line + "\n";
		}
	}
	return result;
}	// Method .indent

std::string dperm::help::get_help() const
{
	return this->indent(2);
}

std::string dperm::help::get_prompt_help() const
{
	return
		"Get Help:\n"s
		"dperm [-h|--h|-help|--help|help]"
	;
}

// post-definitions end
//////////////////////////////////////////////////////////////////////

int main(int argc, char ** argv)
{
	try
	{
		try
		{
			try
			{
				std::cout << std::boolalpha;
				std::clog << std::boolalpha;
				std::cerr << std::boolalpha;
				auto args = std::make_shared<dperm::args>(argc, argv);
				auto perms = std::make_shared<dperm::permissions>(args);
				perms->work();
			}
			catch (const dperm::exception & e)
			{
				throw e;
			}
			catch(const std::system_error & e)
			{
				throw dperm::error_prompt_help_exception{
					"Error Code: "s + dperm::str_man::to_string(e.code()) + "\n"
					+
					"Error: " + e.what()
				};
			}
			catch (const std::exception & e)
			{
				throw dperm::error_prompt_help_exception{
					"Error: "s + e.what()
				};
			}
			catch (...)
			{
				throw dperm::error_prompt_help_exception{
					"c++ exception: unknown."
				};
			}
		}
		catch (const dperm::exception & e)
		{
			bool status = e.print(std::clog);
			if (status)
				return 1;
		}
	}
	catch (const dperm::fatal_error & e)
	{
		try
		{
			e.print(std::clog);
		}
		catch (...)
		{
			std::clog << "ERROR (77215). " << std::endl << std::endl;
		}
		return 1;
	}
}

