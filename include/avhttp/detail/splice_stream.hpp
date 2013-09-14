

#ifndef AVHTTP_DETAIL_SPICE_STREAM_HPP
#define AVHTTP_DETAIL_SPICE_STREAM_HPP

#include <boost/asio.hpp>
#include <boost/make_shared.hpp>

namespace avhttp {
namespace detail {

template<class Handler,class StreamRead,class StreamWrite>
struct splice_stream_op
{
	splice_stream_op(StreamRead &s1, StreamWrite &s2, Handler handler)
	  : m_read_stream(s1)
	  , m_write_stream(s2)
	  , m_handler(handler)
	  , m_state(0)
	  , bytes_spliced(0)
	  , m_streambuf(boost::make_shared<boost::asio::streambuf>())
	{
		m_read_stream.async_read_some(m_streambuf->prepare(4096), *this);
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
		switch (m_state)
		{
			case 0:
			{
				if (!ec) {
					m_state = 1;
					m_streambuf->commit(bytes_transfered);
					// 写入.
					return m_write_stream.async_write_some(m_streambuf->data(), *this);
				}
				if ( ec == boost::asio::error::eof )
				{
					m_handler(boost::system::error_code(), bytes_spliced);
				}else
				{
					m_handler(ec, bytes_spliced);
				}
			}
			break;
			case 1:
			{
				m_state = 0;
				bytes_spliced += bytes_transfered;
				if (!ec) {
					m_streambuf->consume(bytes_transfered);
					return m_read_stream.async_read_some(m_streambuf->prepare(4096), *this);
				}
				m_handler(ec, bytes_spliced);
			}
		}
	}

private:
	StreamRead& m_read_stream;
	StreamWrite& m_write_stream;
	boost::shared_ptr<boost::asio::streambuf> m_streambuf;
	Handler m_handler;

	std::size_t bytes_spliced;

	int m_state;
};

template<class Handler,class StreamRead,class StreamWrite>
splice_stream_op<Handler, StreamRead, StreamWrite>
make_splice_stream_op(StreamRead &s1, StreamWrite &s2, Handler handler)
{
	return splice_stream_op<Handler, StreamRead, StreamWrite>(s1, s2, handler);
}

} // namespace detail

//  helper functiom
template<class Handler,class StreamRead,class StreamWrite>
void async_splice_stream(StreamRead &s1, StreamWrite &s2, Handler handler)
{
	detail::make_splice_stream_op(s1, s2, handler);
}

template<class StreamRead,class StreamWrite>
std::size_t splice_stream(StreamRead &s1, StreamWrite &s2, boost::system::error_code &ec)
{
	boost::asio::streambuf buf;

	std::size_t readed = 0;
	std::size_t writed = 0;
	std::size_t spliced = 0;

	while ( (readed = s1.read_some(buf.prepare(4096), ec)) > 0);
	{
		buf.commit(readed);

		writed = s2.write_some(buf.data(), ec);
		buf.consume(writed);

		if (!ec)
		{
			spliced+= writed;
		} else {
			return spliced;
		}
	}

	return spliced;
}


} // namespace avhttp


#endif // AVHTTP_DETAIL_SPICE_STREAM_HPP
