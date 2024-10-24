#include <curl/curl.h>
#include <memory>
#include "curlclient.hpp"

// helper functions
static long GetResponseCode(CURL *CurlHandle)
{
	long ResponseCode{-1};
	curl_easy_getinfo(CurlHandle, CURLINFO_RESPONSE_CODE, &ResponseCode);
	return ResponseCode;
}

static size_t CurlWriteDataCallback(char *ResponseData, size_t CharSize, size_t NumChars, void *userdata)
{
	std::optional<std::string> *OutOptString{static_cast<std::optional<std::string> *>(userdata)};
	OutOptString->emplace(std::string{}.append(ResponseData, NumChars));
	return CharSize * NumChars;
}

static CurlResponse SendRequest(CURL *CurlHandle, const std::string_view &HostName, const CurlRequest &Request, const bool ForcePost)
{
	std::string Url{std::string{HostName}.append(1, '/').append(Request.GetPath())};
	if (!Request.GetQuery().empty())
	{
		Url.append(Request.GetQuery());
	}
	curl_easy_setopt(CurlHandle, CURLOPT_URL, Url.c_str());

	if (Request.GetPostDataOpt().has_value() && !Request.GetPostDataOpt()->empty())
	{
		curl_easy_setopt(CurlHandle, CURLOPT_POSTFIELDS, Request.GetPostDataOpt()->data());
		curl_easy_setopt(CurlHandle, CURLOPT_POSTFIELDSIZE, Request.GetPostDataOpt()->size());
	}
	else
	{
		curl_easy_setopt(CurlHandle, CURLOPT_POSTFIELDS, NULL);
		curl_easy_setopt(CurlHandle, CURLOPT_POSTFIELDSIZE, 0L);
		if (ForcePost)
		{
			curl_easy_setopt(CurlHandle, CURLOPT_POST, 1L);
		}
		else
		{
			curl_easy_setopt(CurlHandle, CURLOPT_HTTPGET, 1L);
		}
	}

	CurlResponse Response{};
#ifdef DEBUG
	if (1)
#else
	if (Request.RequestedInformation.WantBody)
#endif
	{
		curl_easy_setopt(CurlHandle, CURLOPT_NOBODY, 0L);
		curl_easy_setopt(CurlHandle, CURLOPT_WRITEFUNCTION, CurlWriteDataCallback);
		curl_easy_setopt(CurlHandle, CURLOPT_WRITEDATA, &Response.Body);
	}
	else
	{
		curl_easy_setopt(CurlHandle, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(CurlHandle, CURLOPT_WRITEFUNCTION, nullptr);
	}

	Response.CurlResult = curl_easy_perform(CurlHandle);

	if (Request.RequestedInformation.WantResponseCode)
	{
		Response.ResponseCode = GetResponseCode(CurlHandle);
	}
	return Response;
}

// CurlRequest
void CurlRequest::AddQueryParameter(const std::string_view &Parameter, const std::string_view &Value)
{
	if (!Query.has_value())
	{
		Query.emplace(std::string{});
	}
	if (Query.value().empty())
	{
		Query->push_back('?');
	}
	else
	{
		Query->push_back('&');
	}
	Query->append(Parameter);
	if (!Value.empty())
	{
		Query->push_back('=');
		auto EscapedValue{curl_easy_escape(nullptr, Value.data(), Value.size())};
		Query->append(EscapedValue);
		curl_free(EscapedValue);
	}
}

// CurlClient
CurlClient::CurlClient(ILogWriter &LogWriter, const std::string_view &HostName, const long Port)
	 : Log{LogWriter},
		CurlHandle{curl_easy_init(), curl_easy_cleanup},
		HostName{HostName}, Port{Port}
{
	curl_easy_setopt(CurlHandle.get(), CURLOPT_DEFAULT_PROTOCOL, "http");
	curl_easy_setopt(CurlHandle.get(), CURLOPT_PORT, Port);
	curl_easy_setopt(CurlHandle.get(), CURLOPT_FOLLOWLOCATION, 1L);
	// RequestHeaders = curl_slist_append(RequestHeaders, "Accept: application/csv");	// not currently used
	// curl_easy_setopt(CurlHandle, CURLOPT_HTTPHEADER, RequestHeaders);	//not currently used
}

CurlClient::~CurlClient()
{
	// curl_slist_free_all(RequestHeaders);	// not currently used
}

CurlResponse CurlClient::Get(const CurlRequest &Request)
{
	return SendRequest(CurlHandle.get(), HostName, Request, false);
}

CurlResponse CurlClient::Post(const CurlRequest &Request)
{
	return SendRequest(CurlHandle.get(), HostName, Request, true);
}
