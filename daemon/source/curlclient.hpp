#pragma once

#include <bit>
#include <curl/curl.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "logwriter.hpp"

class CurlRequest
{
private:
	std::optional<std::string> Path;
	std::optional<std::string> Query;
	std::optional<std::string> PostData;

public:
	/// @brief Request builder for CurlClient
	/// @param WantHeaders If true, include headers in response
	/// @param WantBody If true, include body in response
	/// @param WantResponseCode If true, include response code in response
	CurlRequest(const bool WantHeaders = false, const bool WantBody = false, const bool WantResponseCode = false)
		 : RequestedInformation{.WantHeaders = WantHeaders, .WantBody = WantBody, .WantResponseCode = WantResponseCode} {}
	virtual ~CurlRequest() = default;

	struct ri
	{
		unsigned char WantHeaders : 1;
		unsigned char WantBody : 1;
		unsigned char WantResponseCode : 1;
	} RequestedInformation;
	void SetPath(std::string_view NewPath) { Path = NewPath; } // no references, let small string optimization figure this out
	std::string GetPath() const { return Path.value_or(std::string{}); }
	void SetQuery(std::string &NewQuery) { Query = NewQuery; }
	void ClearQuery() { Query.reset(); }
	std::string GetQuery() const { return Query.value_or(std::string{}); }
	void AddQueryParameter(const std::string_view &Parameter, const std::string_view &Value = std::string_view{});
	void SetPostData(std::string &&NewPostData) { PostData = std::move(NewPostData); } // this could be a large string, so move it. caller can make their own copy if they want one
	void ClearPostData() { PostData.reset(); }
	const std::optional<std::string> &GetPostDataOpt() const { return PostData; }
};

class CurlResponse
{
public:
	CURLcode CurlResult{CURLE_SEND_ERROR};
	long ResponseCode{-1};
	std::optional<std::string> Header;
	std::optional<std::string> Body;
};

class CurlClient
{
private:
	ILogWriter &Log;
	std::unique_ptr<CURL, void (*)(CURL *)> CurlHandle;
	// curl_slist *RequestHeaders{nullptr};	// not currently used

public:
	CurlClient(ILogWriter &LogWriter, const std::string_view &HostName, const long Port = 80);
	CurlClient(const CurlClient &) = delete;
	CurlClient &operator=(const CurlClient &) = delete;
	CurlClient(CurlClient &&) = delete;
	CurlClient &operator=(CurlClient &&) = delete;
	~CurlClient();

	std::string HostName;
	long Port;

	CurlResponse Get(const CurlRequest &Request);
	CurlResponse Post(const CurlRequest &Request);
};