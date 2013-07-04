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

#include <boost/lexical_cast.hpp>
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

	///从这个http_stream读取一些数据.
	// @param buffers一个或多个用于读取数据的缓冲区, 这个类型必须满足
	// MutableBufferSequence, MutableBufferSequence的定义在boost.asio
	// 文档中.
	// @param ec在发生错误时, 将传回错误信息.
	// @函数返回读取到的数据大小.
	// @备注: 该函数将会阻塞到一直等待有数据或发生错误时才返回.
	// read_some不能读取指定大小的数据.
	// @begin example
	//  boost::system::error_code ec;
	//  std::size bytes_transferred = s.read_some(boost::asio::buffer(data, size), ec);
	//  ...
	// @end example
	// 关于示例中的boost::asio::buffer用法可以参考boost中的文档. 它可以接受一个
	// boost.array或std.vector作为数据容器.
	template <typename MutableBufferSequence>
	std::size_t read_some(const MutableBufferSequence &buffers,
		boost::system::error_code &ec);

	///从这个http_stream异步读取一些数据.
	// @param buffers一个或多个用于读取数据的缓冲区, 这个类型必须满足MutableBufferSequence,
	//  MutableBufferSequence的定义在boost.asio文档中.
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/MutableBufferSequence.html
	// @param handler在读取操作完成或出现错误时, 将被回调, 它满足以下条件:
	// @begin code
	//  void handler(
	//    const boost::system::error_code &ec,	// 用于返回操作状态.
	//    std::size_t bytes_transferred			// 返回读取的数据字节数.
	//  );
	// @end code
	// @begin example
	//   void handler(const boost::system::error_code &ec, std::size_t bytes_transferred)
	//   {
	//		// 处理异步回调.
	//   }
	//   http_stream h(io_service);
	//   ...
	//   boost::array<char, 1024> buffer;
	//   boost::asio::async_read(h, boost::asio::buffer(buffer), handler);
	//   ...
	// @end example
	// 关于示例中的boost::asio::buffer用法可以参考boost中的文档. 它可以接受一个
	// boost.array或std.vector作为数据容器.
	template <typename MutableBufferSequence, typename Handler>
	void async_read_some(const MutableBufferSequence &buffers, BOOST_ASIO_MOVE_ARG(Handler) handler);

	///向这个http_stream中发送一些数据.
	// @param buffers是一个或多个用于发送数据缓冲. 这个类型必须满足ConstBufferSequence, 参考文档:
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/ConstBufferSequence.html
	// @返回实现发送的数据大小.
	// @备注: 该函数将会阻塞到一直等待数据被发送或发生错误时才返回.
	// write_some不保证发送完所有数据, 用户需要根据返回值来确定已经发送的数据大小.
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
	// 关于示例中的boost::asio::buffer用法可以参考boost中的文档. 它可以接受一个
	// boost.array或std.vector作为数据容器.
	template <typename ConstBufferSequence>
	std::size_t write_some(const ConstBufferSequence &buffers);

	///向这个http_stream中发送一些数据.
	// @param buffers是一个或多个用于发送数据缓冲. 这个类型必须满足ConstBufferSequence, 参考文档:
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/ConstBufferSequence.html
	// @返回实现发送的数据大小.
	// @备注: 该函数将会阻塞到一直等待数据被发送或发生错误时才返回.
	// write_some不保证发送完所有数据, 用户需要根据返回值来确定已经发送的数据大小.
	// @begin example
	//  boost::system::error_code ec;
	//  std::size bytes_transferred = s.write_some(boost::asio::buffer(data, size), ec);
	//  ...
	// @end example
	// 关于示例中的boost::asio::buffer用法可以参考boost中的文档. 它可以接受一个
	// boost.array或std.vector作为数据容器.
	template <typename ConstBufferSequence>
	std::size_t write_some(const ConstBufferSequence &buffers,
		boost::system::error_code &ec);

	///从这个http_stream异步发送一些数据.
	// @param buffers一个或多个用于读取数据的缓冲区, 这个类型必须满足ConstBufferSequence,
	//  ConstBufferSequence的定义在boost.asio文档中.
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/ConstBufferSequence.html
	// @param handler在发送操作完成或出现错误时, 将被回调, 它满足以下条件:
	// @begin code
	//  void handler(
	//    int bytes_transferred,				// 返回发送的数据字节数.
	//    const boost::system::error_code &ec	// 用于返回操作状态.
	//  );
	// @end code
	// @begin example
	//   void handler(int bytes_transferred, const boost::system::error_code &ec)
	//   {
	//		// 处理异步回调.
	//   }
	//   http_stream h(io_service);
	//   ...
	//   h.async_write_some(boost::asio::buffer(data, size), handler);
	//   ...
	// @end example
	// 关于示例中的boost::asio::buffer用法可以参考boost中的文档. 它可以接受一个
	// boost.array或std.vector作为数据容器.
	template <typename ConstBufferSequence, typename Handler>
	void async_write_some(const ConstBufferSequence &buffers, BOOST_ASIO_MOVE_ARG(Handler) handler);

	///向http服务器发起一个请求.
	// @向http服务器发起一个请求, 如果失败抛出异常.
	// @param opt是向服务器发起请求的选项信息.
	// @begin example
	//  avhttp::http_stream h(io_service);
	//  ...
	//  request_opts opt;
	//  opt.insert("cookie", "name=admin;passwd=#@aN@2*242;");
	//  ...
	//  h.request(opt);
	// @end example
	AVHTTP_DECL void request(request_opts &opt);

	///向http服务器发起一个请求.
	// @param opt是向服务器发起请求的选项信息.
	// @param ec在发生错误时, 将传回错误信息.
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

	///向http服务器发起一个异步请求.
	// @param opt指定的http请求选项.
	// @param handler 将被调用在打开完成时. 它必须满足以下条件:
	// @begin code
	//  void handler(
	//    const boost::system::error_code &ec	// 用于返回操作状态.
	//  );
	// @end code
	// @begin example
	//  void request_handler(const boost::system::error_code &ec)
	//  {
	//    if (!ec)
	//    {
	//      // 请求成功!
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

	///清除读写缓冲区数据.
	// @备注: 非线程安全! 不应在正在进行读写操作时进行该操作!
	AVHTTP_DECL void clear();

	///关闭http_stream.
	// @失败抛出asio::system_error异常.
	// @备注: 停止所有正在进行的读写操作, 正在进行的异步调用将回调
	// boost::asio::error::operation_aborted错误.
	AVHTTP_DECL void close();

	///关闭http_stream.
	// @param ec保存失败信息.
	// @备注: 停止所有正在进行的读写操作, 正在进行的异步调用将回调
	// boost::asio::error::operation_aborted错误.
	AVHTTP_DECL void close(boost::system::error_code &ec);

	///判断是否打开.
	// @返回是否打开.
	AVHTTP_DECL bool is_open() const;

	///反回当前http_stream所使用的io_service的引用.
	AVHTTP_DECL boost::asio::io_service& get_io_service();

	///设置最大重定向次数.
	// @param n 指定最大重定向次数, 为0表示禁用重定向.
	AVHTTP_DECL void max_redirects(int n);

	///设置代理, 通过设置代理访问http服务器.
	// @param s 指定了代理参数.
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

	///设置请求时的http选项.
	// @param options 为http的选项. 目前有以下几项特定选项:
	//  _request_method, 取值 "GET/POST/HEAD", 默认为"GET".
	//  Host, 取值为http服务器, 默认为http服务器.
	//  Accept, 取值任意, 默认为"*/*".
	// @begin example
	//  avhttp::http_stream h(io_service);
	//  request_opts options;
	//  options.insert("_request_method", "POST"); // 默认为GET方式.
	//  h.request_options(options);
	//  ...
	// @end example
	AVHTTP_DECL void request_options(const request_opts &options);

	///返回请求时的http选项.
	// @begin example
	//  avhttp::http_stream h(io_service);
	//  request_opts options;
	//  options = h.request_options();
	//  ...
	// @end example
	AVHTTP_DECL request_opts request_options(void) const;

	///http服务器回复选项.
	// @返回服务器回复的所有选项信息, key/value形式.
	AVHTTP_DECL response_opts response_options(void) const;

	///返回location.
	// @返回location信息, 如果没有则返回空串.
	AVHTTP_DECL const std::string& location() const;

	///返回最终请求的url信息.
	AVHTTP_DECL const std::string final_url() const;

	///返回content_length.
	// @content_length信息, 如果没有则为-1.
	AVHTTP_DECL boost::int64_t content_length();

	///设置是否认证服务器证书.
	// @param is_check 如果为true表示认证服务器证书, 如果为false表示不认证服务器证书.
	// 默认为认证服务器证书.
	AVHTTP_DECL void check_certificate(bool is_check);

	///添加证书路径.
	// @param path证书路径.
	AVHTTP_DECL void add_verify_path(const std::string &path);

	///加载证书文件.
	// @param filename指定的证书文件名.
	AVHTTP_DECL void load_verify_file(const std::string &filename);


protected:

	// 内部相关实现, 非外部接口.

	template <typename MutableBufferSequence>
	std::size_t read_some_impl(const MutableBufferSequence &buffers,
		boost::system::error_code &ec);

	// 异步处理模板成员的相关实现.

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

	// 连接到socks代理, 在这一步中完成和socks的信息交换过程, 出错信息在ec中.
	template <typename Stream>
	void socks_proxy_connect(Stream &sock, boost::system::error_code &ec);

	template <typename Stream>
	void socks_proxy_handshake(Stream &sock, boost::system::error_code &ec);

	// socks代理进行异步连接.
	template <typename Stream, typename Handler>
	void async_socks_proxy_connect(Stream &sock, Handler handler);

	// 异步代理查询回调.
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
	// 实现CONNECT指令, 用于请求目标为https主机时使用.
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

	// 实现CONNECT指令, 用于请求目标为https主机时使用.
	template <typename Stream>
	void https_proxy_connect(Stream &sock, boost::system::error_code &ec);
#endif

	template <typename Stream>
	void request_impl(Stream &sock, request_opts &opt, boost::system::error_code &ec);


protected:

	// 定义socket_type类型, socket_type是variant_stream的重定义, 它的作用
	// 可以为ssl_socket或nossl_socket, 这样, 在访问socket的时候, 就不需要
	// 区别编写不同的代码.
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

	// socks处理流程状态.
	enum socks_status
	{
		socks_proxy_resolve,	// 查询proxy主机的IP.
		socks_connect_proxy,	// 连接proxy主机.
		socks_send_version,		// 发送socks版本号.
		socks4_resolve_host,	// 用于socks4查询连接的主机IP端口信息.
		socks4_response,		// socks4服务器返回请求.
		socks5_response_version,// socks5返回版本信息.
		socks5_send_userinfo,	// 发送用户密码信息.
		socks5_connect_request,	// 发送连接请求.
		socks5_connect_response,// 服务器返回连接请求.
		socks5_auth_status,		// 认证状态.
		socks5_result,			// 最终结局.
		socks5_read_domainname,	// 读取域名信息.
#ifdef AVHTTP_ENABLE_OPENSSL
		ssl_handshake,			// ssl进行异步握手.
#endif
	};

private:

	boost::asio::io_service &m_io_service;			// io_service引用.
	tcp::resolver m_resolver;						// 解析HOST.
	socket_type m_sock;								// socket.
	nossl_socket m_nossl_socket;					// 非ssl socket, 只用于https的proxy实现.
	bool m_check_certificate;						// 是否认证服务端证书.
	std::string m_ca_directory;						// 证书路径.
	std::string m_ca_cert;							// CA证书文件.
	request_opts m_request_opts;					// 向http服务器请求的头信息.
	request_opts m_request_opts_priv;				// 向http服务器请求的头信息.
	response_opts m_response_opts;					// http服务器返回的http头信息.
	proxy_settings m_proxy;							// 代理设置.
	int m_proxy_status;								// 异步中代理状态.
	tcp::endpoint m_remote_endp;					// 用于socks4代理中.
	std::string m_protocol;							// 协议类型(http/https).
	url m_url;										// 保存当前请求的url.
	bool m_keep_alive;								// 获得connection选项, 同时受m_response_opts影响.
	int m_status_code;								// http返回状态码.
	std::size_t m_redirects;						// 重定向次数计数.
	std::size_t m_max_redirects;					// 重定向次数计数.
	std::string m_content_type;						// 数据类型.
	boost::int64_t m_content_length;				// 数据内容长度.
	std::size_t m_body_size;						// body大小.
	std::string m_location;							// 重定向的地址.
	boost::asio::streambuf m_request;				// 请求缓冲.
	boost::asio::streambuf m_response;				// 回复缓冲.
#ifdef AVHTTP_ENABLE_ZLIB
	z_stream m_stream;								// zlib支持.
	char m_zlib_buffer[1024];						// 解压缓冲.
	std::size_t m_zlib_buffer_size;					// 输入的字节数.
	bool m_is_gzip;									// 是否使用gz.
#endif
	bool m_is_chunked;								// 是否使用chunked编码.
	bool m_skip_crlf;								// 跳过crlf.
	bool m_is_chunked_end;							// 跳过chunked footer.
	std::size_t m_chunked_size;						// chunked大小.
};

}

#include "avhttp/impl/http_stream.ipp"

#endif // __HTTP_STREAM_HPP__
