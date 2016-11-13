// --------------------------------------------------------------------------
//
// File
//		Name:    testhttpserver.cpp
//		Purpose: Test code for HTTP server class
//		Created: 26/3/04
//
// --------------------------------------------------------------------------

#include "Box.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef HAVE_SIGNAL_H
	#include <signal.h>
#endif

#include "autogen_HTTPException.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HTTPServer.h"
#include "IOStreamGetLine.h"
#include "S3Client.h"
#include "S3Simulator.h"
#include "ServerControl.h"
#include "Test.h"
#include "decode.h"
#include "encode.h"

#include "MemLeakFindOn.h"

#define SHORT_TIMEOUT 5000
#define LONG_TIMEOUT 300000

class TestWebServer : public HTTPServer
{
public:
	TestWebServer();
	~TestWebServer();

	virtual void Handle(HTTPRequest &rRequest, HTTPResponse &rResponse);

};

// Build a nice HTML response, so this can also be tested neatly in a browser
void TestWebServer::Handle(HTTPRequest &rRequest, HTTPResponse &rResponse)
{
	// Test redirection mechanism
	if(rRequest.GetRequestURI() == "/redirect")
	{
		rResponse.SetAsRedirect("/redirected");
		return;
	}

	// Set a cookie?
	if(rRequest.GetRequestURI() == "/set-cookie")
	{
		rResponse.SetCookie("SetByServer", "Value1");
	}

	#define DEFAULT_RESPONSE_1 "<html>\n<head><title>TEST SERVER RESPONSE</title></head>\n<body><h1>Test response</h1>\n<p><b>URI:</b> "
	#define DEFAULT_RESPONSE_3 "</p>\n<p><b>Query string:</b> "
	#define DEFAULT_RESPONSE_4 "</p>\n<p><b>Method:</b> "
	#define DEFAULT_RESPONSE_5 "</p>\n<p><b>Decoded query:</b><br>"
	#define DEFAULT_RESPONSE_6 "</p>\n<p><b>Content type:</b> "
	#define DEFAULT_RESPONSE_7 "</p>\n<p><b>Content length:</b> "
	#define DEFAULT_RESPONSE_8 "</p>\n<p><b>Cookies:</b><br>\n"
	#define DEFAULT_RESPONSE_2 "</p>\n</body>\n</html>\n"

	rResponse.SetResponseCode(HTTPResponse::Code_OK);
	rResponse.SetContentType("text/html");
	rResponse.Write(DEFAULT_RESPONSE_1, sizeof(DEFAULT_RESPONSE_1) - 1);
	const std::string &ruri(rRequest.GetRequestURI());
	rResponse.Write(ruri.c_str(), ruri.size());
	rResponse.Write(DEFAULT_RESPONSE_3, sizeof(DEFAULT_RESPONSE_3) - 1);
	const std::string &rquery(rRequest.GetQueryString());
	rResponse.Write(rquery.c_str(), rquery.size());
	rResponse.Write(DEFAULT_RESPONSE_4, sizeof(DEFAULT_RESPONSE_4) - 1);
	{
		const char *m = "????";
		switch(rRequest.GetMethod())
		{
		case HTTPRequest::Method_GET: m = "GET "; break;
		case HTTPRequest::Method_HEAD: m = "HEAD"; break;
		case HTTPRequest::Method_POST: m = "POST"; break;
		default: m = "UNKNOWN";
		}
		rResponse.Write(m, 4);
	}
	rResponse.Write(DEFAULT_RESPONSE_5, sizeof(DEFAULT_RESPONSE_5) - 1);
	{
		const HTTPRequest::Query_t &rquery(rRequest.GetQuery());
		for(HTTPRequest::Query_t::const_iterator i(rquery.begin()); i != rquery.end(); ++i)
		{
			rResponse.Write("\nPARAM:", 7);
			rResponse.Write(i->first.c_str(), i->first.size());
			rResponse.Write("=", 1);
			rResponse.Write(i->second.c_str(), i->second.size());
			rResponse.Write("<br>\n", 4);
		}
	}
	rResponse.Write(DEFAULT_RESPONSE_6, sizeof(DEFAULT_RESPONSE_6) - 1);
	const std::string &rctype(rRequest.GetContentType());
	rResponse.Write(rctype.c_str(), rctype.size());
	rResponse.Write(DEFAULT_RESPONSE_7, sizeof(DEFAULT_RESPONSE_7) - 1);
	{
		char l[32];
		rResponse.Write(l, ::sprintf(l, "%d", rRequest.GetContentLength()));
	}
	rResponse.Write(DEFAULT_RESPONSE_8, sizeof(DEFAULT_RESPONSE_8) - 1);
	const HTTPRequest::CookieJar_t *pcookies = rRequest.GetCookies();
	if(pcookies != 0)
	{
		HTTPRequest::CookieJar_t::const_iterator i(pcookies->begin());
		for(; i != pcookies->end(); ++i)
		{
			char t[512];
			rResponse.Write(t, ::sprintf(t, "COOKIE:%s=%s<br>\n", i->first.c_str(), i->second.c_str()));
		}
	}
	rResponse.Write(DEFAULT_RESPONSE_2, sizeof(DEFAULT_RESPONSE_2) - 1);
}

TestWebServer::TestWebServer() {}
TestWebServer::~TestWebServer() {}

bool test_httpserver()
{
	SETUP();

	// Test that HTTPRequest can be written to and read from a stream.
	{
		HTTPRequest request(HTTPRequest::Method_PUT, "/newfile");
		request.SetHostName("quotes.s3.amazonaws.com");
		// Write headers in lower case.
		request.AddHeader("date", "Wed, 01 Mar  2006 12:00:00 GMT");
		request.AddHeader("authorization",
			"AWS foo:bar=");
		request.AddHeader("Content-Type", "text/plain");
		request.SetClientKeepAliveRequested(true);

		// Stream it to a CollectInBufferStream
		CollectInBufferStream request_buffer;

		// Because there isn't an HTTP server to respond to us, we can't use
		// SendWithStream, so just send the content after the request.
		request.SendHeaders(request_buffer, IOStream::TimeOutInfinite);
		FileStream fs("testfiles/photos/puppy.jpg");
		fs.CopyStreamTo(request_buffer);

		request_buffer.SetForReading();

		IOStreamGetLine getLine(request_buffer);
		HTTPRequest request2;
		TEST_THAT(request2.Receive(getLine, IOStream::TimeOutInfinite));

		TEST_EQUAL(HTTPRequest::Method_PUT, request2.GetMethod());
		TEST_EQUAL("PUT", request2.GetMethodName());
		TEST_EQUAL("/newfile", request2.GetRequestURI());
		TEST_EQUAL("quotes.s3.amazonaws.com", request2.GetHostName());
		TEST_EQUAL(80, request2.GetHostPort());
		TEST_EQUAL("", request2.GetQueryString());
		TEST_EQUAL("text/plain", request2.GetContentType());
		// Content-Length was not known when the stream was sent, so it should
		// be unknown in the received stream too (certainly before it has all
		// been read!)
		TEST_EQUAL(-1, request2.GetContentLength());
		const HTTPHeaders& headers(request2.GetHeaders());
		TEST_EQUAL("Wed, 01 Mar  2006 12:00:00 GMT",
			headers.GetHeaderValue("Date"));
		TEST_EQUAL("AWS foo:bar=",
			headers.GetHeaderValue("Authorization"));
		TEST_THAT(request2.GetClientKeepAliveRequested());

		CollectInBufferStream request_data;
		request2.ReadContent(request_data, IOStream::TimeOutInfinite);
		TEST_EQUAL(fs.GetPosition(), request_data.GetPosition());
		request_data.SetForReading();
		fs.Seek(0, IOStream::SeekType_Absolute);
		TEST_THAT(fs.CompareWith(request_data, IOStream::TimeOutInfinite));
	}

	// Test that HTTPResponse can be written to and read from a stream.
	// TODO FIXME: we should stream the response instead of buffering it, on both
	// sides (send and receive).
	{
		// Stream it to a CollectInBufferStream
		CollectInBufferStream response_buffer;

		HTTPResponse response(&response_buffer);
		FileStream fs("testfiles/photos/puppy.jpg");
		// Write headers in lower case.
		response.SetResponseCode(HTTPResponse::Code_OK);
		response.AddHeader("date", "Wed, 01 Mar  2006 12:00:00 GMT");
		response.AddHeader("authorization",
			"AWS foo:bar=");
		response.AddHeader("content-type", "text/perl");
		fs.CopyStreamTo(response);
		response.Send();
		response_buffer.SetForReading();

		HTTPResponse response2;
		response2.Receive(response_buffer);

		TEST_EQUAL(200, response2.GetResponseCode());
		TEST_EQUAL("text/perl", response2.GetContentType());

		// TODO FIXME: Content-Length was not known when the stream was sent,
		// so it should be unknown in the received stream too (certainly before
		// it has all been read!) This is currently wrong because we read the
		// entire response into memory immediately.
		TEST_EQUAL(fs.GetPosition(), response2.GetContentLength());

		HTTPHeaders& headers(response2.GetHeaders());
		TEST_EQUAL("Wed, 01 Mar  2006 12:00:00 GMT",
			headers.GetHeaderValue("Date"));
		TEST_EQUAL("AWS foo:bar=",
			headers.GetHeaderValue("Authorization"));

		CollectInBufferStream response_data;
		// request2.ReadContent(request_data, IOStream::TimeOutInfinite);
		response2.CopyStreamTo(response_data);
		TEST_EQUAL(fs.GetPosition(), response_data.GetPosition());
		response_data.SetForReading();
		fs.Seek(0, IOStream::SeekType_Absolute);
		TEST_THAT(fs.CompareWith(response_data, IOStream::TimeOutInfinite));
	}

#ifndef WIN32
	TEST_THAT(system("rm -rf *.memleaks") == 0);
#endif

	// Start the server
	int pid = StartDaemon(0, TEST_EXECUTABLE " server testfiles/httpserver.conf",
		"testfiles/httpserver.pid");
	TEST_THAT_OR(pid > 0, return 1);

	// Run the request script
	TEST_THAT(::system("perl testfiles/testrequests.pl") == 0);

	#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
	#endif

#ifdef ENABLE_KEEPALIVE_SUPPORT // incomplete, need chunked encoding support
	SocketStream sock;
	sock.Open(Socket::TypeINET, "localhost", 1080);

	for (int i = 0; i < 4; i++)
	{
		HTTPRequest request(HTTPRequest::Method_GET,
			"/test-one/34/341s/234?p1=vOne&p2=vTwo");

		if (i < 2)
		{
			// first set of passes has keepalive off by default,
			// so when i == 1 the socket has already been closed
			// by the server, and we'll get -EPIPE when we try
			// to send the request.
			request.SetClientKeepAliveRequested(false);
		}
		else
		{
			request.SetClientKeepAliveRequested(true);
		}

		if (i == 1)
		{
			sleep(1); // need time for our process to realise
			// that the peer has died, otherwise no SIGPIPE :(
			TEST_CHECK_THROWS(
				request.Send(sock, SHORT_TIMEOUT),
				ConnectionException, SocketWriteError);
			sock.Close();
			sock.Open(Socket::TypeINET, "localhost", 1080);
			continue;
		}
		else
		{
			request.Send(sock, SHORT_TIMEOUT);
		}

		HTTPResponse response;
		response.Receive(sock, SHORT_TIMEOUT);

		TEST_THAT(response.GetResponseCode() == HTTPResponse::Code_OK);
		TEST_THAT(response.GetContentType() == "text/html");

		IOStreamGetLine getline(response);
		std::string line;

		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<html>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<head><title>TEST SERVER RESPONSE</title></head>",
			line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<body><h1>Test response</h1>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<p><b>URI:</b> /test-one/34/341s/234</p>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<p><b>Query string:</b> p1=vOne&p2=vTwo</p>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<p><b>Method:</b> GET </p>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<p><b>Decoded query:</b><br>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("PARAM:p1=vOne<br>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("PARAM:p2=vTwo<br></p>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<p><b>Content type:</b> </p>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<p><b>Content length:</b> -1</p>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("<p><b>Cookies:</b><br>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("</p>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("</body>", line);
		TEST_THAT(getline.GetLine(line));
		TEST_EQUAL("</html>", line);

		if(!response.IsKeepAlive())
		{
			BOX_TRACE("Server will close the connection, closing our end too.");
			sock.Close();
			sock.Open(Socket::TypeINET, "localhost", 1080);
		}
		else
		{
			BOX_TRACE("Server will keep the connection open for more requests.");
		}
	}

	sock.Close();
#endif // ENABLE_KEEPALIVE_SUPPORT

	// Kill it
	TEST_THAT(StopDaemon(pid, "testfiles/httpserver.pid",
		"generic-httpserver.memleaks", true));
	TEARDOWN();
}

int test(int argc, const char *argv[])
{
	if(argc >= 2 && ::strcmp(argv[1], "server") == 0)
	{
		// Run a server
		TestWebServer server;
		return server.Main("doesnotexist", argc - 1, argv + 1);
	}

	if(argc >= 2 && ::strcmp(argv[1], "s3server") == 0)
	{
		// Run a server
		S3Simulator server;
		return server.Main("doesnotexist", argc - 1, argv + 1);
	}

	TEST_THAT(test_httpserver());

	return finish_test_suite();
}
