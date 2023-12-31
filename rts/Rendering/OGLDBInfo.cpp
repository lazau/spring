#include "OGLDBInfo.h"

#include "../../tools/pr-downloader/src/Downloader/CurlWrapper.h"
#include "System/StringUtil.h"
#include "System/ScopedResource.h"

#include "fmt/format.h"
#include <sstream>
#include <string>
#include <rapidjson/document.h>

namespace {
	static size_t WriteMemoryCallback(const char* in, size_t size, size_t num, char* out)
	{
		std::string data(in, (std::size_t)size * num);
		*((std::stringstream*)out) << data;
		return size * num;
	}
}

OGLDBInfo::OGLDBInfo(const std::string& glRenderer_, const std::string& myOS_)
	: glRenderer{ StringToLower(glRenderer_) }
	, myOS{ StringToLower(myOS_) }
	, maxVer{0, 0}
	, id{""}
	, drv{""}
{
	fut = std::async(std::launch::async, [this]() -> bool {
		try {
			std::stringstream httpData;

			spring::ScopedNullResource scw(
				[]() { CurlWrapper::InitCurl(); },
				[]() { CurlWrapper::KillCurl(); }
			);


			{
				CurlWrapper curlw;

				const std::string oglInfoURL = fmt::format(
					R"(https://opengl.gpuinfo.org/backend/reports.php?draw=4&columns%5B1%5D%5Bdata%5D=renderer&columns%5B1%5D%5Bname%5D=&columns%5B1%5D%5Bsearchable%5D=true&columns%5B1%5D%5Borderable%5D=true&columns%5B1%5D%5Bsearch%5D%5Bvalue%5D={}&columns%5B1%5D%5Bsearch%5D%5Bregex%5D=false&columns%5B2%5D%5Bdata%5D=version&columns%5B2%5D%5Bname%5D=&columns%5B2%5D%5Bsearchable%5D=true&columns%5B2%5D%5Borderable%5D=true&columns%5B2%5D%5Bsearch%5D%5Bvalue%5D=&columns%5B2%5D%5Bsearch%5D%5Bregex%5D=false&columns%5B3%5D%5Bdata%5D=glversion&columns%5B3%5D%5Bname%5D=&columns%5B3%5D%5Bsearchable%5D=true&columns%5B3%5D%5Borderable%5D=true&columns%5B3%5D%5Bsearch%5D%5Bvalue%5D=&columns%5B3%5D%5Bsearch%5D%5Bregex%5D=false&columns%5B4%5D%5Bdata%5D=glslversion&columns%5B4%5D%5Bname%5D=&columns%5B4%5D%5Bsearchable%5D=true&columns%5B4%5D%5Borderable%5D=true&columns%5B4%5D%5Bsearch%5D%5Bvalue%5D=&columns%5B4%5D%5Bsearch%5D%5Bregex%5D=false&columns%5B5%5D%5Bdata%5D=contexttype&columns%5B5%5D%5Bname%5D=&columns%5B5%5D%5Bsearchable%5D=true&columns%5B5%5D%5Borderable%5D=true&columns%5B5%5D%5Bsearch%5D%5Bvalue%5D=opengl&columns%5B5%5D%5Bsearch%5D%5Bregex%5D=false&columns%5B6%5D%5Bdata%5D=os&columns%5B6%5D%5Bname%5D=&columns%5B6%5D%5Bsearchable%5D=true&columns%5B6%5D%5Borderable%5D=true&columns%5B6%5D%5Bsearch%5D%5Bvalue%5D=&columns%5B6%5D%5Bsearch%5D%5Bregex%5D=false&order%5B0%5D%5Bcolumn%5D=glversion&order%5B0%5D%5Bdir%5D=desc)",
					CurlWrapper::EscapeUrl(glRenderer)
				);

				curl_easy_setopt(curlw.GetHandle(), CURLOPT_URL, oglInfoURL.c_str());

				curl_easy_setopt(curlw.GetHandle(), CURLOPT_SSL_VERIFYPEER, 0L);
				curl_easy_setopt(curlw.GetHandle(), CURLOPT_SSL_VERIFYHOST, 2L);

				curl_easy_setopt(curlw.GetHandle(), CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
				curl_easy_setopt(curlw.GetHandle(), CURLOPT_WRITEDATA, (void*)&httpData);
				curl_easy_setopt(curlw.GetHandle(), CURLOPT_NOPROGRESS, 1L);

				const CURLcode curlres = curl_easy_perform(curlw.GetHandle());

				if (curlres != CURLE_OK)
					return false;
			}

			rapidjson::Document doc;
			rapidjson::ParseResult ok = doc.Parse(httpData);
			if (!ok) { return false; }
			auto data_it = doc.FindMember("data");
			if (data_it == doc.MemberEnd() || !data_it->value.IsArray()) {
				return false;
			}

			for (const auto& dataItem : data_it->value.GetArray()) {
				auto os_it = dataItem.FindMember("os");
				if (os_it == dataItem.MemberEnd() || !os_it->value.IsString()) {
					continue;
				}
				std::string os = StringToLower(os_it->value.GetString());
				if (os.find(myOS) != std::string::npos) { //OS match
					auto glversion_it = dataItem.FindMember("glversion");
					if (glversion_it == dataItem.MemberEnd() ||
							!glversion_it->value.IsString()) {
						continue;
					}
					const std::string glVerStr = glversion_it->value.GetString();

					int2 glVer = { 0, 0 };
					if ((sscanf(glVerStr.c_str(), "%i.%i", &glVer.x, &glVer.y) == 2) && (10 * glVer.x + glVer.y > 10 * maxVer.x + maxVer.y)) {
						maxVer = glVer;

						if (auto it = dataItem.FindMember("id"); it != dataItem.MemberEnd() &&
								it->value.IsInt()) {
							id = std::to_string(it->value.GetInt());
						}
						if (auto it = dataItem.FindMember("version"); it != dataItem.MemberEnd() &&
								it->value.IsString()) {
							drv = it->value.GetString();
						}
					}
				}
			}

			return !id.empty();
		}
		catch (...) {
			return false;
		}
	});

	using namespace std::chrono_literals;
	fut.wait_for(1ms);
}

bool OGLDBInfo::IsReady(uint32_t waitTimeMS) const
{
	auto status = fut.wait_for(std::chrono::milliseconds(waitTimeMS));
	return (status == std::future_status::ready);
}

bool OGLDBInfo::GetResult(int2& maxCtx_, std::string& url_, std::string& drv_)
{
	if (!fut.get())
		return false;

	constexpr const char* myfmt = R"(https://opengl.gpuinfo.org/displayreport.php?id={})";

	url_ = fmt::format(myfmt, id);
	drv_ = drv;
	maxCtx_ = maxVer;

	return true;
}
