//
// file.hpp
// ~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// path LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef __FILE_HPP__
#define __FILE_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/noncopyable.hpp>
#include <boost/filesystem/fstream.hpp>

#include "avhttp/storage_interface.hpp"

namespace avhttp {

using std::ios;

class file
	: public storage_interface
	, public boost::noncopyable
{
public:
	file() {}
	virtual ~file() { close(); }

public:

	// Saving module init.
	// @param file_path path of the file.
	// @param ec saves the complete info when met error.
	virtual void open(const fs::path &file_path, boost::system::error_code &ec)
	{
		ec = boost::system::error_code();
		m_fstream.open(file_path, ios::binary|ios::in|ios::out);
		if (!m_fstream.is_open())
		{
			m_fstream.clear();
			m_fstream.open(file_path, ios::trunc|ios::binary|ios::out|ios::in);
		}
		if (!m_fstream.is_open())
		{
			ec = boost::system::errc::make_error_code(boost::system::errc::bad_file_descriptor);
		}
	}

	///Opened or not.
	inline bool is_open() const
	{
		return m_fstream.is_open();
	}

	// Close the saving module.
	virtual void close()
	{
		m_fstream.close();
	}

// Is the crap below really necessary? I debut it.

	// Write the data.
	// @param buf The buffer to be written.
	// @param offset Where to start writing.
	// @param size The size of the buffer.
	// @return The bytes exactly written.
    //         -1 when met error.
	virtual int write(const char *buf, boost::uint64_t offset, int size)
	{
		m_fstream.seekp(offset, ios::beg);
		m_fstream.write(buf, size);
		m_fstream.flush();
		return size;
	}

	// Read data.
	// @param buf The buffer to be read.
	// @param offset Where to start reading.
	// @param size Size of the buffer.
	// @return The bytes exactly read.
    //         -1 when met error.
	virtual int read(char *buf, boost::uint64_t offset, int size)
	{
		m_fstream.seekg(offset, ios::beg);
		m_fstream.read(buf, size);
		return size;
	}


protected:
	boost::filesystem::fstream m_fstream;
};

// Default saving object.
static storage_interface* default_storage_constructor()
{
	return new file();
}

}

#endif // __FILE_HPP__
