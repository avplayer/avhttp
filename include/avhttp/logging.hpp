//
// logging.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef __LOGGING_H__
#define __LOGGING_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <iostream>
#include <string>
#include <fstream>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>

namespace avhttp {

namespace fs = boost::filesystem;

///Simple logging class for internal use.
// Instructions:
//	At entrance method(eg. main) call INIT_LOGGER, two params, first is the path for
//	log file, the second is the filename, see INIT_LOGGER for details.
//	Then can use LOG_DEBUG/LOG_INFO/LOG_WARNING/LOG_ERROR for logging.
// @begin example
//  #include "avhttp.hpp" // avhttp.hpp includes logging.hpp, can include logging.hpp alone too, for sure.
//  int main()
//  {
//     INIT_LOGGER(".", "example.log");	// Create example.log in current working dir.
//     // Can be INIT_LOGGER("", "") too; For no outputing to files but stdout.
//     LOG_DEBUG("Initialized.");
//     std::string result = do_something();
//     LOG_DEBUG("do_something return : " << result);	// Output do_something's result.
//     ...
//     UNINIT_LOGGER();	// Uninstall logging module.
//  }
// @end example

class logger : public boost::noncopyable
{
public:
	logger(fs::path const& logpath, fs::path const& filename, bool append = true, bool inited = false)
	{
		try
		{
			m_inited = inited;
			if (!inited)
				return;
			if (filename.empty())
				return;
			if (!fs::exists(logpath)) fs::create_directories(logpath);
			m_file.open((logpath / filename).string().c_str(),
				std::ios_base::out | (append ? std::ios_base::app : std::ios_base::out));
			*this << "\n\n\n*** starting log ***\n\n\n";
		}
		catch (std::exception& e)
		{
			std::cerr << "failed to create log '" << filename.string() << "': " << e.what() << std::endl;
		}
	}
	~logger() {}

public:

	template <class T>
	logger& operator<<(T const& v)
	{
		if (!m_inited)
			return *this;

		std::cout << v;
		std::cout.flush();

		if (m_file.is_open())
		{
			m_file << v;
			m_file.flush();
		}

#if defined(WIN32) && defined(LOGGER_DBG_VIEW)
		std::ostringstream oss;
		oss << v;
		m_dbg_view += oss.str();
#endif
		return *this;
	}

	const std::string& dbg_view() const
	{
		return m_dbg_view;
	}

	void clear_dbg_view()
	{
		m_dbg_view = "";
	}

	bool inited() const
	{
		return m_inited;
	}

private:
	std::ofstream m_file;
	std::string m_dbg_view;
	bool m_inited;
};

inline char const* time_now_string()
{
	time_t t = std::time(0);
	tm* timeinfo = std::localtime(&t);
	static char str[200];
	std::strftime(str, 200, " %b %d %X ", timeinfo);
	return str;
}

namespace aux {

template<class Lock>
Lock& lock_single()
{
	static Lock lock_instance;
	return lock_instance;
}

template<class Log, class Logger_ptr>
Logger_ptr& logger_single(std::string path = ".",
	std::string filename = "avhttp.log", bool append = true, bool binit = false)
{
	static Logger_ptr logger_instance(new Log(path, filename, append, binit));
	if (logger_instance && !binit)
		return logger_instance;
	if (logger_instance)
	{
		if (logger_instance->inited())
			return logger_instance;
	}
	logger_instance.reset(new Log(path, filename, append, binit));
	return logger_instance;
}

}

//////////////////////////////////////////////////////////////////////////
// Internal interface about logging.

#define _LOGS_ (*(avhttp::aux::logger_single<avhttp::logger, boost::shared_ptr<avhttp::logger> >()))

#ifdef LOGGER_THREAD_SAFE
#define _LOCKS_() boost::mutex::scoped_lock lock(avhttp::aux::lock_single<boost::mutex>())
#else
#define _LOCKS_() ((void)0)
#endif // LOGGER_THREAD_SAFE

#if defined(WIN32) && defined(LOGGER_DBG_VIEW)
#define _DBG_VIEW_() do { OutputDebugStringA(_LOGS_.dbg_view().c_str()); _LOGS_.clear_dbg_view(); } while (0)
#else
#define _DBG_VIEW_() ((void)0)
#endif // WIN32 && LOGGER_DEBUG_VIEW

//////////////////////////////////////////////////////////////////////////
// Public interface about logging.

///Init logging module.
// @param path path to the log file.
// @param file the log file name.
// @remark: if file is "", then there will be no outputing to 
//      files but stdout.
#define INIT_LOGGER(path, file) do {\
	_LOCKS_();\
	avhttp::aux::logger_single<avhttp::logger, boost::shared_ptr<avhttp::logger> >(path, file, true, true);\
} while (0)

///Uninstall logging module.
#define UNINIT_LOGGER() do {\
	_LOCKS_();\
	avhttp::aux::logger_single<avhttp::logger, boost::shared_ptr<avhttp::logger> >().reset();\
} while (0)

#if defined(DEBUG) || defined(_DEBUG)
#define LOG_DEBUG(message) do { \
	_LOCKS_(); \
	_LOGS_ << avhttp::time_now_string() << "[DEBUG]: " << message << "\n"; \
	_DBG_VIEW_(); \
} while (0)

#define LOG_INFO(message) do { \
	_LOCKS_(); \
	_LOGS_ << avhttp::time_now_string() << "[INFO]: " << message << "\n"; \
	_DBG_VIEW_(); \
} while (0)

#define LOG_WARNING(message) do { \
	_LOCKS_(); \
	_LOGS_ << avhttp::time_now_string() << "[WARNING]: " << message << "\n"; \
	_DBG_VIEW_(); \
} while (0)

#define LOG_ERROR(message) do { \
	_LOCKS_(); \
	_LOGS_ << avhttp::time_now_string() << "[ERROR]: " << message << "\n"; \
	_DBG_VIEW_(); \
} while (0)

#else
#define LOG_DEBUG(message) ((void)0)
#define LOG_INFO(message) ((void)0)
#define LOG_WARNING(message) ((void)0)
#define LOG_ERROR(message) ((void)0)
#endif

}

#endif // __LOGGING_H__
