//
// TODO rewrite and site example

// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, synchronous
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <opencv2/opencv.hpp>
using namespace cv;

#include "camera_pool.hpp"

// Global camera object
// VideoCapture camera(0);
CameraPool cameras = CameraPool();

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

//------------------------------------------------------------------------------

// // Return a reasonable mime type based on the extension of a file.
// boost::beast::string_view
// mime_type(boost::beast::string_view path)
// {
//  using boost::beast::iequals;
//  auto const ext = [&path]
//  {
//    auto const pos = path.rfind(".");
//    if (pos == boost::beast::string_view::npos)
//      return boost::beast::string_view{};
//    return path.substr(pos);
//  }();
//  if (iequals(ext, ".htm"))  return "text/html";
//  if (iequals(ext, ".html")) return "text/html";
//  if (iequals(ext, ".php"))  return "text/html";
//  if (iequals(ext, ".css"))  return "text/css";
//  if (iequals(ext, ".txt"))  return "text/plain";
//  if (iequals(ext, ".js"))   return "application/javascript";
//  if (iequals(ext, ".json")) return "application/json";
//  if (iequals(ext, ".xml"))  return "application/xml";
//  if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
//  if (iequals(ext, ".flv"))  return "video/x-flv";
//  if (iequals(ext, ".png"))  return "image/png";
//  if (iequals(ext, ".jpe"))  return "image/jpeg";
//  if (iequals(ext, ".jpeg")) return "image/jpeg";
//  if (iequals(ext, ".jpg"))  return "image/jpeg";
//  if (iequals(ext, ".gif"))  return "image/gif";
//  if (iequals(ext, ".bmp"))  return "image/bmp";
//  if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
//  if (iequals(ext, ".tiff")) return "image/tiff";
//  if (iequals(ext, ".tif"))  return "image/tiff";
//  if (iequals(ext, ".svg"))  return "image/svg+xml";
//  if (iequals(ext, ".svgz")) return "image/svg+xml";
//  return "application/text";
// }

// // Append an HTTP rel-path to a local filesystem path.
// // The returned path is normalized for the platform.
// std::string
// path_cat(
//  boost::beast::string_view base,
//  boost::beast::string_view path)
// {
//  if (base.empty())
//    return path.to_string();
//  std::string result = base.to_string();
// #if BOOST_MSVC
//  char constexpr path_separator = '\\';
//  if (result.back() == path_separator)
//    result.resize(result.size() - 1);
//  result.append(path.data(), path.size());
//  for (auto& c : result)
//    if (c == '/')
//      c = path_separator;
// #else
//  char constexpr path_separator = '/';
//  if (result.back() == path_separator)
//    result.resize(result.size() - 1);
//  result.append(path.data(), path.size());
// #endif
//  return result;
// }

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<class Body, class Allocator, class Send> void handle_request
  ( boost::beast::string_view doc_root
  , http::request<Body, http::basic_fields<Allocator>>&& req
  , Send&& send)
{


 // Returns a bad request response
 auto const bad_request =
   [&req](boost::beast::string_view why)
 {
   http::response<http::string_body> res{ http::status::bad_request, req.version() };
   res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
   res.set(http::field::content_type, "text/html");
   res.keep_alive(req.keep_alive());
   res.body() = why.to_string();
   res.prepare_payload();
   return res;
 };

 // Returns a not found response
 auto const not_found =
   [&req](boost::beast::string_view target)
 {
   http::response<http::string_body> res{ http::status::not_found, req.version() };
   res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
   res.set(http::field::content_type, "text/html");
   res.keep_alive(req.keep_alive());
   res.body() = "The resource '" + target.to_string() + "' was not found.";
   res.prepare_payload();
   return res;
 };

 // Returns a server error response
 auto const server_error =
   [&req](boost::beast::string_view what)
 {
   http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
   res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
   res.set(http::field::content_type, "text/html");
   res.keep_alive(req.keep_alive());
   res.body() = "An error occurred: '" + what.to_string() + "'";
   res.prepare_payload();
   return res;
 };

 // Make sure we can handle the method
 if (req.method() != http::verb::get &&
   req.method() != http::verb::head)
   return send(bad_request("Unknown HTTP-method"));


 // Request path must be absolute and not contain "..".
 if( req.target().empty() ||
     req.target()[0] != '/' ||
     req.target() == "/"    ||  //need to request a feed
     req.target().find("..") != boost::beast::string_view::npos
   )
   return send(bad_request("Illegal request-target"));


//count endpoint
 auto const show_count =
   [&req](boost::beast::string_view target)
 {
   http::response<http::string_body> res{ http::status::ok, req.version() };
   //res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
   //res.set(http::field::content_identifier, "--jpgboundary");
   res.set(http::field::content_type, "text/plain");

   //  res.set(http::field::content_type, "multipart/x-mixed-replace;boundary=jpgboundary\n");
    res.set(http::field::content_length, target.length());


   //res.keep_alive(req.keep_alive());
   res.body() = target.to_string();

   res.prepare_payload();

   return res;
 };

 if(req.target() == "/count")
 {
  send(show_count(std::to_string(cameras.count())));
 }

 char * num = new char[req.target().size()-1];
 for(int i = 0; i < req.target().size()-1; i++)
 {
	 *(num+i) = req.target().data()[i+1];
 }
// const char* n = req.target().data().c_str();
 // std::string n = req.target().to_string();
  int camNum = static_cast<int>(std::atoi(num));
//int camNum = 0;
  if(camNum >= cameras.count() || camNum < 0 || cameras.count() == 0)
  {
    return send(not_found(req.target()));
  }

  std::string data = cameras.getFrame(camNum);


 auto const show_frame =
   [&req](boost::beast::string_view target)
 {
   http::response<http::string_body> res{ http::status::ok, req.version() };
   res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
   //res.set(http::field::content_identifier, "--jpgboundary");
   res.set(http::field::content_type, "image/jpeg");
   res.set(http::field::accept, "image/jpeg");
  res.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
  res.set(http::field::access_control_allow_headers, "Content-Type, image/jpeg");
  res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");

    res.set(http::field::access_control_allow_origin, "*");
   res.set(http::field::content_length, target.length());


   //res.keep_alive(req.keep_alive());
   res.body() = target.to_string();

   res.prepare_payload();

   return res;
 };
 send(show_frame(data));
// other examples
//  // Build the path to the requested file
//  std::string path = path_cat(doc_root, req.target());
//  if (req.target().back() == '/')
//    path.append("index.html");

//  // Attempt to open the file
//  boost::beast::error_code ec;
//  http::file_body::value_type body;
//  body.open(path.c_str(), boost::beast::file_mode::scan, ec);

//  // Handle the case where the file doesn't exist
//  if (ec == boost::system::errc::no_such_file_or_directory)
//    return send(not_found(req.target()));

//  // Handle an unknown error
//  if (ec)
//    return send(server_error(ec.message()));

//  // Cache the size since we need it after the move
//  auto const size = body.size();

//  // Respond to HEAD request
//  if (req.method() == http::verb::head)
//  {
//    http::response<http::empty_body> res{ http::status::ok, req.version() };
//    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
//    res.set(http::field::content_type, mime_type(path));
//    res.content_length(size);
//    res.keep_alive(req.keep_alive());
//    return send(std::move(res));
//  }

//  // Respond to GET request
//  http::response<http::file_body> res{
//    std::piecewise_construct,
//    std::make_tuple(std::move(body)),
//    std::make_tuple(http::status::ok, req.version()) };
//  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
//  res.set(http::field::content_type, mime_type(path));
//  res.content_length(size);
//  res.keep_alive(req.keep_alive());
//  return send(std::move(res));
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(boost::system::error_code ec, char const* what)
{
 std::cerr << what << ": " << ec.message() << "\n";
}

// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template<class Stream>
struct send_lambda
{
 Stream& stream_;
 bool& close_;
 boost::system::error_code& ec_;

 explicit
   send_lambda(
     Stream& stream,
     bool& close,
     boost::system::error_code& ec)
   : stream_(stream)
   , close_(close)
   , ec_(ec)
 {
 }

 template<bool isRequest, class Body, class Fields>
 void
   operator()(http::message<isRequest, Body, Fields>&& msg) const
 {
   // Determine if we should close the connection after
   close_ = msg.need_eof();

   // We need the serializer here because the serializer requires
   // a non-const file_body, and the message oriented version of
   // http::write only works with const messages.
   http::serializer<isRequest, Body, Fields> sr{ msg };
   http::write(stream_, sr, ec_);
 }
};

// Handles an HTTP server connection
void
do_session(
 tcp::socket& socket,
 std::shared_ptr<std::string const> const& doc_root)
{
 bool close = false;
 boost::system::error_code ec;

 // This buffer is required to persist across reads
 boost::beast::flat_buffer buffer;

 // This lambda is used to send messages
 send_lambda<tcp::socket> lambda{ socket, close, ec };

 for (;;)
 {
   // Read a request
   http::request<http::string_body> req;
   http::read(socket, buffer, req, ec);
   if (ec == http::error::end_of_stream)
   {
    //  std::cout << "eos";
     break;
   }
   if (ec)
   {
     std::cout << "read";
     return fail(ec, "read");
   }

   // Send the response
   handle_request(*doc_root, std::move(req), lambda);
   if (ec)
   {
     std::cout << "write";
     return fail(ec, "write");
   }
   if (close)
   {
     std::cout << "close";
     // This means we should close the connection, usually because
     // the response indicated the "Connection: close" semantic.
     break;
   }
 }

 // Send a TCP shutdown
 socket.shutdown(tcp::socket::shutdown_send, ec);

 // At this point the connection is closed gracefully
}

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
 try
 {
   // Check command line arguments.
  //  if (4 != 4)
  //  {
  //    std::cerr <<
  //      "Usage: http-server-sync <address> <port> <doc_root>\n" <<
  //      "Example:\n" <<
  //      "    http-server-sync 0.0.0.0 80 .\n";
  //    return EXIT_FAILURE;
  //  }

   auto const address = boost::asio::ip::make_address("0.0.0.0");
   auto const port = static_cast<unsigned short>(std::atoi("80"));
   auto const doc_root = std::make_shared<std::string>("C:\\Users\\deskj\\programs\\html\\servertest");

   // The io_context is required for all I/O
   boost::asio::io_context ioc{ 1 };

   // The acceptor receives incoming connections
   tcp::acceptor acceptor{ ioc, {address, port} };
   for (;;)
   {
     // This will receive the new connection
     tcp::socket socket{ ioc };

     // Block until we get a connection
     acceptor.accept(socket);

     // Launch the session, transferring ownership of the socket
     std::thread{ std::bind(
       &do_session,
       std::move(socket),
       doc_root) }.detach();
   }
 }
 catch (const std::exception& e)
 {
   std::cerr << "Error: " << e.what() << std::endl;
   return EXIT_FAILURE;
 }

  return 0; // TODO success var
}
