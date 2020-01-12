#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/opencv.hpp>
using namespace cv;

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>



// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<class Body, class Allocator,class Send> void handle_request
  ( VideoCapture&& camera
  , http::request<Body, http::basic_fields<Allocator>>&& req
  , Send&& send
  )
{

  // for now only one open route
  //read and encode frame
  Mat frame;
  camera.read(frame);
  std::vector<unsigned char> buffer;
  cv::imencode(".jpg", frame, buffer, std::vector<int>());
  std::string data = std::string(buffer.begin(), buffer.end());

  // Respond with image frame
  auto const hijack =
    [&req](boost::beast::string_view target)
    {
      http::response<http::string_body> res{ http::status::ok, req.version() };
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.set(http::field::content_type, "image/jpeg");
      res.keep_alive(req.keep_alive());
      res.body() = target.to_string();
      res.prepare_payload();
      return res;
    };

return send(hijack(data));

// // Returns a bad request response
// auto const bad_request =
// [&req](boost::beast::string_view why)
// {
// http::response<http::string_body> res{ http::status::bad_request, req.version() };
// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
// res.set(http::field::content_type, "text/html");
// res.keep_alive(req.keep_alive());
// res.body() = why.to_string();
// res.prepare_payload();
// return res;
// };

// // Returns a not found response
// auto const not_found =
// [&req](boost::beast::string_view target)
// {
// http::response<http::string_body> res{ http::status::not_found, req.version() };
// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
// res.set(http::field::content_type, "text/html");
// res.keep_alive(req.keep_alive());
// res.body() = "The resource '" + target.to_string() + "' was not found.";
// res.prepare_payload();
// return res;
// };

// // Returns a server error response
// auto const server_error =
// [&req](boost::beast::string_view what)
// {
// http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
// res.set(http::field::content_type, "text/html");
// res.keep_alive(req.keep_alive());
// res.body() = "An error occurred: '" + what.to_string() + "'";
// res.prepare_payload();
// return res;
// };

// // Make sure we can handle the method
// if (req.method() != http::verb::get &&
// req.method() != http::verb::head)
// return send(bad_request("Unknown HTTP-method"));

// // Request path must be absolute and not contain "..".
// if (req.target().empty() ||
// req.target()[0] != '/' ||
// req.target().find("..") != boost::beast::string_view::npos)
// return send(bad_request("Illegal request-target"));

// // Build the path to the requested file
// std::string path = path_cat(doc_root, req.target());
// if (req.target().back() == '/')
// path.append("index.html");

// // Attempt to open the file
// boost::beast::error_code ec;
// http::file_body::value_type body;
// body.open(path.c_str(), boost::beast::file_mode::scan, ec);

// // Handle the case where the file doesn't exist
// if (ec == boost::system::errc::no_such_file_or_directory)
// return send(not_found(req.target()));

// // Handle an unknown error
// if (ec)
// return send(server_error(ec.message()));

// // Cache the size since we need it after the move
// auto const size = body.size();

// // Respond to HEAD request
// if (req.method() == http::verb::head)
// {
// http::response<http::empty_body> res{ http::status::ok, req.version() };
// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
// res.set(http::field::content_type, mime_type(path));
// res.content_length(size);
// res.keep_alive(req.keep_alive());
// return send(std::move(res));
// }

// // Respond to GET request
// http::response<http::file_body> res{
// std::piecewise_construct,
// std::make_tuple(std::move(body)),
// std::make_tuple(http::status::ok, req.version()) };
// res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
// res.set(http::field::content_type, mime_type(path));
// res.content_length(size);
// res.keep_alive(req.keep_alive());
// return send(std::move(res));
}

//------------------------------------------------------------------------------

// Report a failure
void fail(boost::system::error_code ec, char const* what)
{
  std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
  // This is the C++11 equivalent of a generic lambda.
  // The function object is used to send an HTTP message.
  struct send_lambda
  {
    session& self_;

    explicit
    send_lambda(session& self) : self_(self) {}

    template<bool isRequest, class Body, class Fields>
    void operator()(http::message<isRequest, Body, Fields>&& msg) const
    {
      // The lifetime of the message has to extend
      // for the duration of the async operation so
      // we use a shared_ptr to manage it.
      auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(std::move(msg));

      // Store a type-erased version of the shared
      // pointer in the class to keep it alive.
      self_.res_ = sp;

      // Write the response
      http::async_write
        ( self_.socket_
        , *sp
        , boost::asio::bind_executor
            ( self_.strand_
            , std::bind
              ( &session::on_write
              , self_.shared_from_this()
              , std::placeholders::_1
              , std::placeholders::_2
              , sp->need_eof()
              )
            )
        );
    }
  };

  tcp::socket socket_;
  std::shared_ptr<VideoCapture const> camera_;
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  boost::beast::flat_buffer buffer_;
  http::request<http::string_body> req_;
  std::shared_ptr<void> res_;
  send_lambda lambda_;

public:
  // Take ownership of the socket
  explicit
  session(tcp::socket socket, std::shared_ptr<VideoCapture const> const& camera)
    : socket_(std::move(socket))
    , camera_(std::move(camera))
    , strand_(socket_.get_executor())
    , lambda_(*this)
  {}

  // Start the asynchronous operation
  void run()
  {
    do_read();
  }

  void
  do_read()
  {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};

    // Read a request
    http::async_read
      ( socket_
      , buffer_
      , req_
      , boost::asio::bind_executor
        ( strand_
        , std::bind
          ( &session::on_read
          , shared_from_this()
          , std::placeholders::_1
          , std::placeholders::_2
          )
        )
      );
  }

  void on_read(boost::system::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if (ec == http::error::end_of_stream)
      return do_close();

    if (ec)
      return fail(ec, "read");

    // Send the response
    handle_request(std::move(camera_), std::move(req_), lambda_);
  }

  void on_write( boost::system::error_code ec, std::size_t bytes_transferred, bool close)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec)
      return fail(ec, "write");

    if (close)
    {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      return do_close();
    }

    // We're done with the response so delete it
    res_ = nullptr;

    // Read another request
    do_read();
  }

  void
  do_close()
  {
    // Send a TCP shutdown
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
  }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  std::shared_ptr<VideoCapture const> camera_;

  public:
  listener( boost::asio::io_context& ioc, std::shared_ptr<VideoCapture const> const& camera, tcp::endpoint endpoint) : acceptor_(ioc), socket_(ioc), camera_(camera)
  {
    boost::system::error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if (ec)
    {
      fail(ec, "open");
      return;
    }

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
      fail(ec, "set_option");
      return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if (ec)
    {
      fail(ec, "bind");
      return;
    }

    // Start listening for connections
    acceptor_.listen(
    boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
      fail(ec, "listen");
      return;
    }
  }

  // Start accepting incoming connections
  void run()
  {
    if (!acceptor_.is_open())
      return;
    do_accept();
  }

  void do_accept()
  {
    acceptor_.async_accept
      ( socket_
      , std::bind(&listener::on_accept, shared_from_this(), std::placeholders::_1)
      );
  }

  void
  on_accept(boost::system::error_code ec)
  {
    if (ec)
    {
      fail(ec, "accept");
    }
    else
    {
      // Create the session and run it
      std::make_shared<session>
        ( std::move(socket_)
        , std::move(camera_)
        )->run();
    }
    // Accept another connection
    do_accept();
  }
};

  //------------------------------------------------------------------------------

int main(int argc, char* argv[])
{

  auto const address = boost::asio::ip::make_address("0.0.0.0");
  auto const port = static_cast<unsigned short>(std::atoi("80"));
  auto const threads = std::max<int>(1, std::atoi("4"));
  auto const camera = std::shared_ptr<VideoCapture>(0);

  // The io_context is required for all I/O
  boost::asio::io_context ioc{ threads };

  // Create and launch a listening port
  std::make_shared<listener>
    ( ioc
    , camera
    , tcp::endpoint{ address, port }
    )->run();

  // Run the I/O service on the requested number of threads
  std::vector<std::thread> v;
  v.reserve(threads - 1);
  for (auto i = threads - 1; i > 0; --i)
    v.emplace_back
      ( [&ioc]
          {
            ioc.run();
          }
      );
  ioc.run();

  camera->release();
  return EXIT_SUCCESS;
}
