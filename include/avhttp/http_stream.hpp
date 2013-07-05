//
// http_stream.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef __HTTP_STREAM_HPP__
#define __HTTP_STREAM_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <vector>
#include <cstring>	// for std::strcmp/std::strlen

#include <boost/shared_array.hpp>
#include <boost/asio/detail/config.hpp>

#include "avhttp/url.hpp"
#include "avhttp/settings.hpp"
#include "avhttp/detail/io.hpp"
#include "avhttp/detail/parsers.hpp"
#include "avhttp/detail/error_codec.hpp"
#ifdef AVHTTP_ENABLE_OPENSSL
#include "avhttp/detail/ssl_stream.hpp"
#endif
#ifdef AVHTTP_ENABLE_ZLIB
extern "C"
{
#include "zlib.h"
#ifndef z_const
# define z_const
#endif
}
#endif

#include "avhttp/detail/socket_type.hpp"
#include "avhttp/detail/utf8.hpp"


namespace avhttp {

// A http stream, for ask the data from an url, asynchronously or 
// synchronously. Now supports http and https protocol.
// @notice: Class http_stream is NOT thread-safe!!
// An example for synchronous calls.
// @begin example
//  try
//  {
//  	boost::asio::io_service io_service;
//  	avhttp::http_stream h(io_service);
//  	avhttp::request_opts opt;
//
//  	// Set the request options.
//  	opt.insert("Connection", "close");
//  	h.request_options(opt);
//  	h.open("http://www.boost.org/LICENSE_1_0.txt");
//
//  	boost::system::error_code ec;
//  	while (!ec)
//  	{
//  		char data[1024];
//  		std::size_t bytes_transferred = h.read_some(boost::asio::buffer(data, 1024), ec);
//			// For fixed-size data, you can use boost::asio::read, like:
//			// std::size_t bytes_transferred = boost::asio::read(h, boost::asio::buffer(buf), ec);
//  		std::cout.write(data, bytes_transferred);
//  	}
//  }
//  catch (std::exception &e)
//  {
//  	std::cerr << "Exception: " << e.what() << std::endl;
//  }
// @end example
//
// Below is for asynchronous ones.
// @begin example
//  class downloader
//  {
//  public:
//  	downloader(boost::asio::io_service &io)
//  		: m_io_service(io)
//  		, m_stream(io)
//  	{
//  		// Set the request options.
//  		avhttp::request_opts opt;
//  		opt.insert("Connection", "close");
//  		m_stream.request_options(opt);
//			// Starts the asynchronous request.
//  		m_stream.async_open("http://www.boost.org/LICENSE_1_0.txt",
//  			boost::bind(&downloader::handle_open, this, boost::asio::placeholders::error));
//  	}
//  	~downloader()
//  	{}
//  
//  public:
//  	void handle_open(const boost::system::error_code &ec)
//  	{
//  		if (!ec)
//  		{
//  			m_stream.async_read_some(boost::asio::buffer(m_buffer),
//  				boost::bind(&downloader::handle_read, this,
//  				boost::asio::placeholders::bytes_transferred,
//  				boost::asio::placeholders::error));
//				// Supports boost::asio::async_read too, use boost.asio, like:
//				boost::asio::async_read(m_stream, boost::asio::buffer(m_buffer),
// 					boost::bind(&downloader::handle_read, this,
// 					boost::asio::placeholders::bytes_transferred,
// 					boost::asio::placeholders::error));
//  		}
//  	}
//  
//  	void handle_read(int bytes_transferred, const boost::system::error_code &ec)
//  	{
//  		if (!ec)
//  		{
//  			std::cout.write(m_buffer.data(), bytes_transferred);
//  			m_stream.async_read_some(boost::asio::buffer(m_buffer),
//  				boost::bind(&downloader::handle_read, this,
//  				boost::asio::placeholders::bytes_transferred,
//  				boost::asio::placeholders::error));
//  		}
//  	}
//  
//  private:
//  	boost::asio::io_service &m_io_service;
//  	avhttp::http_stream m_stream;
//  	boost::array<char, 1024> m_buffer;
//  };
//
//  int main(int argc, char* argv[])
//  {
//		boost::asio::io_service io;
//		downloader d(io);
//		io.run();
//		return 0;
//  }
// @end example


using boost::asio::ip::tcp;

class http_stream : public boost::noncopyable
{
public:

	/// Constructor.
	AVHTTP_DECL explicit http_stream(boost::asio::io_service &io);

	/// Destructor.
	AVHTTP_DECL virtual ~http_stream();

	///Request the given url.
	// Throw boost::system::system_error on failure.
	// @param u URL to be opened.
	// @begin example
	//   avhttp::http_stream h(io_service);
	//   try
	//   {
	//     h.open("http://www.boost.org");
	//   }
	//   catch (boost::system::system_error &e)
	//   {
	//     std::cerr << e.waht() << std::endl;
	//   }
	// @end example
	AVHTTP_DECL void open(const url &u);

	///Request the given url.
	// @param u URL to be opened.
	// Get state via ec.
	// @begin example
	//   avhttp::http_stream h(io_service);
	//   boost::system::error_code ec;
	//   h.open("http://www.boost.org", ec);
	//   if (ec)
	//   {
	//     std::cerr << e.waht() << std::endl;
	//   }
	// @end example
	AVHTTP_DECL void open(const url &u, boost::system::error_code &ec);

	///Request URL asynchronously.
	// @param u URL to be opened.
	// @param handler be called on okay. Must meet:
	// @begin code
	//  void handler(
	//    const boost::system::error_code &ec // To return the state.
	//  );
	// @end code
	// @begin example
	//  void open_handler(const boost::system::error_code &ec)
	//  {
	//    if (!ec)
	//    {
	//      // Okay!
	//    }
	//  }
	//  ...
	//  avhttp::http_stream h(io_service);
	//  h.async_open("http://www.boost.org", open_handler);
	// @end example
	// @remark: handler can also bind a regular method via boost.bind as
	// the param handler of async_open.
	template <typename Handler>
	void async_open(const url &u, BOOST_ASIO_MOVE_ARG(Handler) handler);

	///Read from http_stream.
	// @param buffers one or more buffers, must meets MutableBufferSequence,
	// MutableBufferSequenceis defined in the documention of boost.asio.
	// @return: The length of data read. In bytes.
	//          Throw boost::asio::system_error on failure.
	// @remark: Will block until there is data or meets error.
	//          read_some cannot read data in given size.
	// @begin example
	//  try
	//  {
	//    std::size bytes_transferred = s.read_some(boost::asio::buffer(data, size));
	//  } catch (boost::asio::system_error &e)
	//  {
	//    std::cerr << e.what() << std::endl;
	//  }
	//  ...
	// @end example
	template <typename MutableBufferSequence>
	std::size_t read_some(const MutableBufferSequence &buffers);

	///Read from http_stream.
	// @param buffers one or more buffers, must meet
	// MutableBufferSequence, MutableBufferSequence is defined in
    // the document of boost.asio.
	// @param ec Will return the infomation .
	// @return the length of the data read.
	// @notice: Will block until reads or meets error.
	// read_some can NOT read fixed-size data.
	// @begin example
	//  boost::system::error_code ec;
	//  std::size bytes_transferred = s.read_some(boost::asio::buffer(data, size), ec);
	//  ...
	// @end example
	// See boost document for the usage of boost::asio::buffer. 
	// boost.array or std.vector can be used for collecting data.
	template <typename MutableBufferSequence>
	std::size_t read_some(const MutableBufferSequence &buffers,
		boost::system::error_code &ec);

	///Read asynchronously from http_stream.
	// @param buffers one or more buffers, must meet 
    // MutableBufferSequence, which defined in the documention of
    // boost.asio : 
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/MutableBufferSequence.html
	// @param handler will be call back in the end (ok or error), meets:
	// @begin code
	//  void handler(
	//    const boost::system::error_code &ec,	// For return state.
	//    std::size_t bytes_transferred			// Num of bytes be read.
	//  );
	// @end code
	// @begin example
	//   void handler(const boost::system::error_code &ec, std::size_t bytes_transferred)
	//   {
	//		// Process asynchronous call-back.
	//   }
	//   http_stream h(io_service);
	//   ...
	//   boost::array<char, 1024> buffer;
	//   boost::asio::async_read(h, boost::asio::buffer(buffer), handler);
	//   ...
	// @end example
	// See boost document for the usage of boost::asio::buffer. 
	// boost.array or std.vector can be used for collecting data.
	template <typename MutableBufferSequence, typename Handler>
	void async_read_some(const MutableBufferSequence &buffers, BOOST_ASIO_MOVE_ARG(Handler) handler);

	///Send some data to http_stream.
	// @param buffers One or more buffers to be sent. 
    // Must meet ConstBufferSequence, See:
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/ConstBufferSequence.html
	// @return Num of bytes sent.
	// @remark: Will block until finishes(ok or error).
	// write_some is NOT supposed to transfer all the data, the bytes 
    // transferred shouda be known via the params returned.
	// @begin example
	//  try
	//  {
	//    std::size bytes_transferred = s.write_some(boost::asio::buffer(data, size));
	//  }
	//  catch (boost::asio::system_error &e)
	//  {
	//    std::cerr << e.what() << std::endl;
	//  }
	//  ...
	// @end example
	// See boost document for the usage of boost::asio::buffer. 
	// boost.array or std.vector can be used for collecting data.
	template <typename ConstBufferSequence>
	std::size_t write_some(const ConstBufferSequence &buffers);

	///Send some data to http_stream.
	// @param buffers One or more buffers to be sent. 
    // Must meet ConstBufferSequence, See:
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/ConstBufferSequence.html
	// @return Num of bytes sent.
	// @remark: Will block until finishes(ok or error).
	// write_some is NOT supposed to transfer all the data, the bytes 
    // transferred shouda be known via the params returned.
	// @begin example
	//  boost::system::error_code ec;
	//  std::size bytes_transferred = s.write_some(boost::asio::buffer(data, size), ec);
	//  ...
	// @end example
	// See boost document for the usage of boost::asio::buffer. 
	// boost.array or std.vector can be used for collecting data.
	template <typename ConstBufferSequence>
	std::size_t write_some(const ConstBufferSequence &buffers,
		boost::system::error_code &ec);

	///Send some data asynchronously to the http_stream.
	// @param buffers One or more buffers to be sent. 
    // Must meet ConstBufferSequence, See:
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/ConstBufferSequence.html
	// @param handler will be call back in the end (ok or error), meets:
	// @begin code
	//  void handler(
	//    std::size_t bytes_transferred			// Num of bytes be read.
	//    const boost::system::error_code &ec,	// For return state.
	//  );
	// @end code
	// @begin example
	//   void handler(int bytes_transferred, const boost::system::error_code &ec)
	//   {
	//		// Process asynchronous call-back.
	//   }
	//   http_stream h(io_service);
	//   ...
	//   h.async_write_some(boost::asio::buffer(data, size), handler);
	//   ...
	// @end example
	// See boost document for the usage of boost::asio::buffer. 
	// boost.array or std.vector can be used for collecting data.
	template <typename ConstBufferSequence, typename Handler>
	void async_write_some(const ConstBufferSequence &buffers, BOOST_ASIO_MOVE_ARG(Handler) handler);

	///Send a request to an http server.
	// @Send a request to an http server, throw on failures.
	// @param opt is the options for the request.
	// @begin example
	//  avhttp::http_stream h(io_service);
	//  ...
	//  request_opts opt;
	//  opt.insert("cookie", "name=admin;passwd=#@aN@2*242;");
	//  ...
	//  h.request(opt);
	// @end example
	AVHTTP_DECL void request(request_opts &opt);

	///Send a request to an http server.
	// @param opt is the options for the request.
	// @param ec will return the information when meets error.
	// @begin example
	//  avhttp::http_stream h(io_service);
	//  ...
	//  request_opts opt;
	//  boost::system::error_code ec;
	//  opt.insert("cookie", "name=admin;passwd=#@aN@2*242;");
	//  ...
	//  h.request(opt, ec);
	//  ...
	// @end example
	AVHTTP_DECL void request(request_opts &opt, boost::system::error_code &ec);

	///Send a asynchronous request to an http server.
	// @param opt is the options for the request.
	// @param handler will be call back in the end (ok or error), meets:
	// @begin code
	//  void handler(
	//    const boost::system::error_code &ec,	// For return state.
	//  );
	// @end code
	// @begin example
	//  void request_handler(const boost::system::error_code &ec)
	//  {
	//    if (!ec)
	//    {
	//      // Ok!
	//    }
	//  }
	//  ...
	//  avhttp::http_stream h(io_service);
	//  ...
	//  request_opts opt;
	//  opt.insert("cookie", "name=admin;passwd=#@aN@2*242;");
	//  h.async_request(opt, boost::bind(&request_handler, boost::asio::placeholders::error));
	// @end example
	template <typename Handler>
	void async_request(const request_opts &opt, BOOST_ASIO_MOVE_ARG(Handler) handler);

	///Clear IO buffer.
	// @remark: NOT thread-safe! Don't call it while reading/writing!
	AVHTTP_DECL void clear();

	///Close http_stream.
	// @throw asio::system_error on failure.
	// @remark: Stop all the IO operation, asynchronous ones will
    //          be call back.
	// Error boost::asio::error::operation_aborted.
	AVHTTP_DECL void close();

	///Close http_stream.
	// @param ec for saving error informations.
	// @remark: Stop all the IO operation, asynchronous ones will
    //          be call back.
	// Error boost::asio::error::operation_aborted.
	AVHTTP_DECL void close(boost::system::error_code &ec);

	///See it is opened or not.
	// @return opened or not.
	AVHTTP_DECL bool is_open() const;

	///Return current http_stream's io_service quote.
	AVHTTP_DECL boost::asio::io_service& get_io_service();

	///Set the max redirect times.
	// @param n the max redirect times, set to 0 to 
    //          disable redirection.
	AVHTTP_DECL void max_redirects(int n);

	///Set up proxy, and visit http server via it.
	// @param s options for proxy.
	// @begin example
	//  avhttp::http_stream h(io_service);
	//  proxy_settings s;
	//  s.type = socks5;
	//  s.hostname = "example.proxy.com";
	//  s.port = 8080;
	//  h.proxy(s);
	//  ...
	// @end example
	AVHTTP_DECL void proxy(const proxy_settings &s);

	///Set the http option while connecting.
	// @param options http option. Now supports:
	//  _request_method, can be "GET", "POST", or"HEAD", default "GET".
	//  Host, can be httpserver , default is http server.
	//  Accept, can be anything, default "*/*".
	// @begin example
	//  avhttp::http_stream h(io_service);
	//  request_opts options;
	//  options.insert("_request_method", "POST"); // default by GET.
	//  h.request_options(options);
	//  ...
	// @end example
	AVHTTP_DECL void request_options(const request_opts &options);

	///Return the http option while connecting.
	// @begin example
	//  avhttp::http_stream h(io_service);
	//  request_opts options;
	//  options = h.request_options();
	//  ...
	// @end example
	AVHTTP_DECL request_opts request_options(void) const;

	///Http server response options.
	// @return: All http server response options, in the form 
    //          of key/value.
	AVHTTP_DECL response_opts response_options(void) const;

	///Return location.
	// @return: location, "" for none.
	AVHTTP_DECL const std::string& location() const;

	///Return url finally requested.
	AVHTTP_DECL const std::string final_url() const;

	///Return content_length.
	// @return: content_length, -1 for none.
	AVHTTP_DECL boost::int64_t content_length();

	///Set whether check server cert.
	// @param is_check true for check, false for not to check.
	// Default is true.
	AVHTTP_DECL void check_certificate(bool is_check);

	///Add cert path.
	// @param path cert path.
	AVHTTP_DECL void add_verify_path(const std::string &path);

	///Load cert file.
	// @param filename the name of given cert file.
	AVHTTP_DECL void load_verify_file(const std::string &filename);


protected:

	// Private implement, NOT public interface.

	template <typename MutableBufferSequence>
	std::size_t read_some_impl(const MutableBufferSequence &buffers,
		boost::system::error_code &ec);

	// Asynchronous processing module members implement.

	template <typename Handler>
	void handle_resolve(const boost::system::error_code &err,
		tcp::resolver::iterator endpoint_iterator, Handler handler);

	template <typename Handler>
	void handle_connect(Handler handler,
		tcp::resolver::iterator endpoint_iterator, const boost::system::error_code &err);

	template <typename Handler>
	void handle_request(Handler handler, const boost::system::error_code &err);

	template <typename Handler>
	void handle_status(Handler handler, const boost::system::error_code &err);

	template <typename Handler>
	void handle_header(Handler handler, int bytes_transferred, const boost::system::error_code &err);

	template <typename MutableBufferSequence, typename Handler>
	void handle_read(const MutableBufferSequence &buffers,
		Handler handler, const boost::system::error_code &ec, std::size_t bytes_transferred);

	template <typename MutableBufferSequence, typename Handler>
	void handle_skip_crlf(const MutableBufferSequence &buffers,
		Handler handler, boost::shared_array<char> crlf,
		const boost::system::error_code &ec, std::size_t bytes_transferred);

	template <typename MutableBufferSequence, typename Handler>
	void handle_async_read(const MutableBufferSequence &buffers,
		Handler handler, const boost::system::error_code &ec, std::size_t bytes_transferred);

	template <typename MutableBufferSequence, typename Handler>
	void handle_chunked_size(const MutableBufferSequence &buffers,
		Handler handler, const boost::system::error_code &ec, std::size_t bytes_transferred);

	// Connect to socks proxy, do information exchange here, error info is in ec.
	template <typename Stream>
	void socks_proxy_connect(Stream &sock, boost::system::error_code &ec);

	template <typename Stream>
	void socks_proxy_handshake(Stream &sock, boost::system::error_code &ec);

	// Socks proxy do asynchronous connect.
	template <typename Stream, typename Handler>
	void async_socks_proxy_connect(Stream &sock, Handler handler);

	// Asynchronous request proxy, and call back.
	template <typename Stream, typename Handler>
	void async_socks_proxy_resolve(const boost::system::error_code &err,
		tcp::resolver::iterator endpoint_iterator, Stream &sock, Handler handler);

	template <typename Stream, typename Handler>
	void handle_connect_socks(Stream &sock, Handler handler,
		tcp::resolver::iterator endpoint_iterator, const boost::system::error_code &err);

	template <typename Stream, typename Handler>
	void handle_socks_process(Stream &sock, Handler handler,
		int bytes_transferred, const boost::system::error_code &err);

#ifdef AVHTTP_ENABLE_OPENSSL
	// Do CONNECT order, for requesting https servers.
	template <typename Stream, typename Handler>
	void async_https_proxy_connect(Stream &sock, Handler handler);

	template <typename Stream, typename Handler>
	void async_https_proxy_resolve(const boost::system::error_code &err,
		tcp::resolver::iterator endpoint_iterator, Stream &sock, Handler handler);

	template <typename Stream, typename Handler>
	void handle_connect_https_proxy(Stream &sock, Handler handler,
		tcp::resolver::iterator endpoint_iterator, const boost::system::error_code &err);

	template <typename Stream, typename Handler>
	void handle_https_proxy_request(Stream &sock, Handler handler,
		const boost::system::error_code &err);

	template <typename Stream, typename Handler>
	void handle_https_proxy_status(Stream &sock, Handler handler,
		const boost::system::error_code &err);

	template <typename Stream, typename Handler>
	void handle_https_proxy_header(Stream &sock, Handler handler,
		int bytes_transferred, const boost::system::error_code &err);

	template <typename Stream, typename Handler>
	void handle_https_proxy_handshake(Stream &sock, Handler handler,
		const boost::system::error_code &err);

	// Do CONNECT order, for requesting https servers.
	template <typename Stream>
	void https_proxy_connect(Stream &sock, boost::system::error_code &ec);
#endif

	template <typename Stream>
	void request_impl(Stream &sock, request_opts &opt, boost::system::error_code &ec);


protected:

	// Define socket_type, socket_type is a redefine of variant_stream, 
	// which can BOTH act ssl_socket or nossl_socket, so, when 
    // requesting socket, no need to write different code.
#ifdef AVHTTP_ENABLE_OPENSSL
	typedef avhttp::detail::ssl_stream<tcp::socket&> ssl_socket;
#endif
	typedef tcp::socket nossl_socket;
	typedef avhttp::detail::variant_stream<
		nossl_socket
#ifdef AVHTTP_ENABLE_OPENSSL
		, ssl_socket
#endif
	> socket_type;

	// socks procdure status.
	enum socks_status
	{
		socks_proxy_resolve,	// Request proxy server's IP.
		socks_connect_proxy,	// connect proxy server.
		socks_send_version,		// Send socks version num.
		socks4_resolve_host,	// For socks4 to request IP info.
		socks4_response,		// socks4 server response request.
		socks5_response_version,// socks5 return version info.
		socks5_send_userinfo,	// send user password info.
		socks5_connect_request,	// send connection request.
		socks5_connect_response,// server response connect request.
		socks5_auth_status,		// auth status.
		socks5_result,			// The end.
		socks5_read_domainname,	// Read domain info.
#ifdef AVHTTP_ENABLE_OPENSSL
		ssl_handshake,			// ssl do async hand shaking.
#endif
	};

private:

	boost::asio::io_service &m_io_service;			// quote of io_service.
	tcp::resolver m_resolver;						// reslove HOST.
	socket_type m_sock;								// socket.
	nossl_socket m_nossl_socket;					// non-ssl socket, for the proxy of https.
	bool m_check_certificate;						// check the cert of server?
	std::string m_ca_directory;						// path to the cert.
	std::string m_ca_cert;							// CA cert file name.
	request_opts m_request_opts;					// request http header info.
	request_opts m_request_opts_priv;				// request http header info.
	response_opts m_response_opts;					// server-replied http header info.
	proxy_settings m_proxy;							// proxy settings.
	int m_proxy_status;								// proxy status in async.
	tcp::endpoint m_remote_endp;					// for socks4 proxy.
	std::string m_protocol;							// protocol(http/https).
	url m_url;										// save the current requesting url.
	bool m_keep_alive;								// get the option connection, influnceed by m_response_opts.
	int m_status_code;								// http return status.
	std::size_t m_redirects;						// num of redirections.
	std::size_t m_max_redirects;					// the max of them.
	std::string m_content_type;						// content type.
	boost::int64_t m_content_length;				// content length.
	std::size_t m_body_size;						// body size.
	std::string m_location;							// redirect URL.
	boost::asio::streambuf m_request;				// request buffer.
	boost::asio::streambuf m_response;				// reply buffer.
#ifdef AVHTTP_ENABLE_ZLIB
	z_stream m_stream;								// zlib support.
	char m_zlib_buffer[1024];						// unzip buffer.
	std::size_t m_zlib_buffer_size;					// bytes input.
	bool m_is_gzip;									// gunzipped?
#endif
	bool m_is_chunked;								// using chunked?
	bool m_skip_crlf;								// skip crlf?
	bool m_is_chunked_end;							// skip chunked footer?
	std::size_t m_chunked_size;						// chunked size.
};

}

#include "avhttp/impl/http_stream.ipp"

#endif // __HTTP_STREAM_HPP__
