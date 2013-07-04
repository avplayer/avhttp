//
// async_read_body.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
// Copyright (C) 2012 - 2013  microcat <microcai@fedoraproject.org>
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// path LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef __AVHTTP_MISC_HTTP_READBODY_HPP__
#define __AVHTTP_MISC_HTTP_READBODY_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

BOOST_STATIC_ASSERT_MSG(BOOST_VERSION >= 105400, "You must use boost-1.54 or later!!!");

#include "avhttp/http_stream.hpp"

namespace avhttp {
namespace detail {

// match condition!
struct read_all_t
{
	read_all_t(boost::int64_t content_length)
		: m_content_length(content_length)
	{
	}

	template <typename Error>
	std::size_t operator()(const Error& err, std::size_t bytes_transferred)
	{
		if(err)
			return 0;

		if(m_content_length > 0 )
		{
			// Yet has read content_length, yes.
			return m_content_length - bytes_transferred;
		}
		else
		{
			return 4096;
		}
	}

	boost::int64_t m_content_length;
};

inline read_all_t read_all(boost::int64_t content_length)
{
	return read_all_t(content_length);
}

template <typename AsyncReadStream, typename MutableBufferSequence, typename Handler>
class read_body_op : boost::asio::coroutine
{
public:
	read_body_op(AsyncReadStream &stream, const avhttp::url &url,
		MutableBufferSequence &buffers, Handler handler)
		: m_stream(stream)
		, m_buffers(buffers)
		, m_handler(BOOST_ASIO_MOVE_CAST(Handler)(handler))
	{
		m_stream.async_open(url, *this);
	}

#if defined(BOOST_ASIO_HAS_MOVE)
    read_body_op(const read_body_op &other)
      : m_stream(other.m_stream)
	  , m_buffers(other.m_buffers)
	  , m_handler(other.m_handler)
    {
    }

    read_body_op(read_body_op &&other)
      : m_stream(other.m_stream)
	  , m_buffers(other.m_buffers)
	  , m_handler(BOOST_ASIO_MOVE_CAST(Handler)(other.m_handler))
    {
    }
#endif // defined(BOOST_ASIO_HAS_MOVE)

	void operator()(const boost::system::error_code &ec, std::size_t bytes_transferred = 0)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{
			if(!ec)
			{
				BOOST_ASIO_CORO_YIELD boost::asio::async_read(
					m_stream, m_buffers, read_all(m_stream.content_length()), *this);
			}
			else
			{
				m_handler(ec, bytes_transferred);
				return;
			}

			if(ec == boost::asio::error::eof && m_stream.content_length() == 0)
			{
				m_handler(boost::system::error_code(), bytes_transferred);
			}
			else
			{
				m_handler(ec, bytes_transferred);
			}
		}
	}

// private:
	AsyncReadStream &m_stream;
	MutableBufferSequence &m_buffers;
	Handler m_handler;
};

template <typename AsyncReadStream, typename MutableBufferSequence, typename Handler>
read_body_op<AsyncReadStream, MutableBufferSequence, Handler>
	make_read_body_op(AsyncReadStream &stream,
	const avhttp::url &url, MutableBufferSequence &buffers, Handler handler)
{
	return read_body_op<AsyncReadStream, MutableBufferSequence, Handler>(
		stream, url, buffers, handler);
}

} // namespace detail


///For http_stream to ask url asynchronously.
// This method is for http_stream to ask url asynchronously, and 
// call the user via handler callback, data will be saved in the
// buffers user given first.
// @notice:
//  1. The method will call back till body or eof or any other
//     errors, back via error_code.
//  2. During the whole procedure, should keep stream and buffers'
//     lifetime.
// @param stream a http_stream object.
// @param url given url.
// @param buffers one or some buffers
// This class MUST meet the MutableBufferSequence, 
// MutableBufferSequence's defines.
// See boost.asio for details.
// @param handler will be call back at the end(ok or error), it
// meets:
// @begin code
//  void handler(
//    const boost::system::error_code &ec,	// For returning the state.
//    std::size_t bytes_transferred			// Return the length of 
//                                          // data read in bytes.
//  );
// @end code
// @begin example
//  void handler(const boost::system::error_code &ec, std::size_t bytes_transferred)
//  {
//      // Process async call-back.
//  }
//  ...
//  avhttp::http_stream h(io);
//  async_read_body(h, "http://www.boost.org/LICENSE_1_0.txt",
//      boost::asio::buffer(data, 1024), handler);
//  io.run();
//  ...
// @end example
template<typename AsyncReadStream, typename MutableBufferSequence, typename Handler>
AVHTTP_DECL void async_read_body(AsyncReadStream &stream,
	const avhttp::url &url, MutableBufferSequence &buffers, Handler handler)
{
	detail::make_read_body_op(stream, url, buffers, handler);
}

} // namespace avhttp

#endif // __AVHTTP_MISC_HTTP_READBODY_HPP__
