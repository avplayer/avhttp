

#ifndef AVHTTP_INTER_THREAD_STREAM_HPP
#define AVHTTP_INTER_THREAD_STREAM_HPP

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

namespace avhttp {

/*
 * 用法同 管道, 但是优点是不经过内核. 支持同步和异步读取.
 *
 * 注意, 该对象是线程安全的, 但是可以禁用线程安全特性, 如果你的代码是单线程的话
 *
 * 多次调用 async_read/some 是未定义行为.
 *
 * TODO: 暂时先实现 异步, 同步的稍后实现.
 */

class interthread_stream : boost::noncopyable
{
public:
	explicit interthread_stream(boost::asio::io_service & io_service)
		: m_io_service(io_service)
	{

	}

	template <typename ConstBufferSequence, typename Handler>
	void async_write_some(const ConstBufferSequence& buffers, BOOST_ASIO_MOVE_ARG(Handler) handler)
	{
 		boost::mutex::scoped_lock l(m_mutex);

	 	if (m_state_read_closed)
		{
			return m_io_service.post(
				boost::asio::detail::bind_handler(
					handler,
					boost::asio::error::make_error_code(boost::asio::error::broken_pipe),
					0
				)
			);
		}

		std::size_t bytes_writed;

		bytes_writed = boost::asio::buffer_copy(m_buffer.prepare(boost::asio::buffer_size(buffers)),
			boost::asio::buffer(buffers));

		m_buffer.commit(bytes_writed);

		if (m_current_read_handler)
		{
			std::size_t bytes_readed = boost::asio::buffer_copy(
				boost::asio::buffer(m_read_buffer), m_buffer.data());
			m_buffer.consume(bytes_readed);

			// 唤醒协程.

			m_io_service.post(
				boost::asio::detail::bind_handler(
					m_current_read_handler,
					boost::system::error_code(),
					bytes_readed
				)
			);

			m_current_read_handler = NULL;
		}

		// 决定是否睡眠.

		if ( m_buffer.size() >= 512)
		{
			m_current_write_handler = m_io_service.wrap(boost::bind<void>(handler, _1, bytes_writed));
		}else
		{
			m_io_service.post(
				boost::asio::detail::bind_handler(
					handler,
					boost::system::error_code(),
					bytes_writed
				)
			);
		}
	}

	template <typename MutableBufferSequence, typename Handler>
	void async_read_some(const MutableBufferSequence& buffers, BOOST_ASIO_MOVE_ARG(Handler) handler)
	{
		boost::mutex::scoped_lock l(m_mutex);

		if (m_buffer.size() == 0)
		{
			if (m_state_write_closed)
			{
				return m_io_service.post(
					boost::asio::detail::bind_handler(
						handler,
						boost::asio::error::make_error_code(boost::asio::error::eof),
						0
					)
				);
			}

			m_read_buffer = buffers;
			m_current_read_handler = m_io_service.wrap(handler);
			return;
		}

		std::size_t bytes_readed = boost::asio::buffer_copy(boost::asio::buffer(buffers), m_buffer.data());
		m_buffer.consume(bytes_readed);

		m_io_service.post(
			boost::asio::detail::bind_handler(
				handler,
				boost::system::error_code(),
				bytes_readed
			)
		);

		// 接着唤醒 write
		if (m_current_write_handler)
		{
			if (m_buffer.size() <= 512)
			{
				// 唤醒协程
				m_current_write_handler(boost::system::error_code(), 0);
				m_current_write_handler = NULL;
			}
		}

	}

	// 关闭, 这样 read 才会读到 eof!
	void shutdown(boost::asio::socket_base::shutdown_type type)
	{
		using namespace boost::asio;

		boost::mutex::scoped_lock l(m_mutex);

		if (type == socket_base::shutdown_send
			|| type == socket_base::shutdown_both)
		{
			// 关闭写
			m_state_write_closed = true;
		}

		if (type == socket_base::shutdown_receive
			|| type == socket_base::shutdown_both)
		{
			// 关闭读
			m_state_read_closed = true;
		}

		if (m_state_write_closed)
		{
			// 检查是否有 read,  是就返回
			if (m_current_read_handler)
			{
				m_current_read_handler(error::make_error_code(error::eof),0);
				m_current_read_handler = NULL;
			}
		}
		return cond_wake_writer();
	}

	// 写入数据
	template <typename ConstBufferSequence>
	std::size_t write_some(const ConstBufferSequence& buffers, boost::system::error_code & ec)
	{
		// 唤醒正在睡眠的协程.
		boost::mutex::scoped_lock l(m_mutex);

		if (m_state_read_closed)
		{
			ec = boost::asio::error::make_error_code(boost::asio::error::broken_pipe);
			return 0;
		}

		std::size_t bytes_writed;

		bytes_writed = boost::asio::buffer_copy(m_buffer.prepare(boost::asio::buffer_size(buffers)),
			boost::asio::buffer(buffers));

		m_buffer.commit(bytes_writed);

		// 检查是否有读取的，有则唤醒.

		if (m_current_read_handler)
		{
			std::size_t bytes_readed = boost::asio::buffer_copy(
				boost::asio::buffer( m_read_buffer ), m_buffer.data());

			m_buffer.consume( bytes_readed );

 			// 唤醒 reader
 			m_current_read_handler(ec, bytes_readed);
		}

		if (m_buffer.size() >= 512)
		{
			// 进入睡眠状态.
			m_current_write_handler = boost::bind(&boost::condition_variable::notify_all, &m_write_cond);
			m_write_cond.wait(l);
			// 当 m_current_write_handler 被调用的时候， write_cond 就被 notify 了， 也就是执行到这里了
		}
		return bytes_writed;
	}

	// 读取数据
	template <typename MutableBufferSequence>
	std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code & ec)
	{
		std::size_t bytes_readed;
		boost::mutex::scoped_lock l(m_mutex);
		// 唤醒正在睡眠的协程.
		if (m_buffer.size() == 0)
		{
			if (m_state_write_closed)
			{
				ec = boost::asio::error::make_error_code(boost::asio::error::eof);
				return 0;
			}

			m_read_buffer = buffers;
			volatile std::size_t _bytes_readed;

			// 进入睡眠状态.
			m_current_read_handler = boost::bind(&interthread_stream::m_read_cond_wait_read_handler, this,
				_1, _2, &ec, &_bytes_readed);
			m_read_cond.wait(l);
			bytes_readed = _bytes_readed;
			// 当 m_current_read_handler 被调用的时候， m_read_cond 就被 notify 了， 也就是执行到这里了
			return bytes_readed;
		}

		// 有剩余的，马上读取
		bytes_readed = boost::asio::buffer_copy(boost::asio::buffer(buffers), m_buffer.data());
		m_buffer.consume(bytes_readed);

		// 有没有 write 在睡眠？有的话唤醒
		cond_wake_writer();

		return bytes_readed;
	}

	boost::asio::io_service & get_io_service()
	{
		return m_io_service;
	}

private:
	void cond_wake_writer()
	{
		if (m_current_write_handler)
		{
			if (m_buffer.size() <= 512)
			{
				// 唤醒协程
				m_current_write_handler(boost::system::error_code(), 0);
				m_current_write_handler = NULL;
			}
		}
	}

private:

	void m_read_cond_wait_read_handler(const boost::system::error_code & ec, std::size_t bytes_transfered,
		boost::system::error_code *ec_out, volatile std::size_t *bytes_transfered_out)
	{
		* ec_out = ec;
		* bytes_transfered_out =bytes_transfered;
		m_read_cond.notify_all();
	}

private:
	boost::asio::io_service & m_io_service;

	boost::asio::streambuf m_buffer;

	boost::asio::mutable_buffer m_read_buffer;

	typedef boost::function<
		void (boost::system::error_code, std::size_t)
	> async_handler_type;

	// 注意,  这里意味着同时发起多次 async_read/write 操作, 是不支持的.
	async_handler_type m_current_read_handler;
	async_handler_type m_current_write_handler;

	mutable boost::mutex m_mutex;
	mutable boost::condition_variable m_write_cond;
	mutable boost::condition_variable m_read_cond;

	bool m_state_write_closed;
	bool m_state_read_closed;
};

} // namespace avhttp

#endif // AVHTTP_INTER_THREAD_STREAM_HPP
