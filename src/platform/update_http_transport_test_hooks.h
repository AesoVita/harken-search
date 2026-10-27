#pragma once

#ifndef ARIS_UPDATE_HTTP_TRANSPORT_TESTING
#error "update_http_transport_test_hooks.h is test-only"
#endif

#include "platform/update_http_transport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Platform::UpdateHttp::Testing {

enum class NativeReadStatus { Chunk, Eof, Error };

struct NativeReadResult {
    NativeReadStatus status = NativeReadStatus::Error;
    std::vector<std::uint8_t> bytes;
    std::uint32_t systemError = 0;
};

class NativeHttpApi {
public:
    virtual ~NativeHttpApi() = default;
    virtual bool Open(const std::string& url,
                      const std::string& headers,
                      std::uint32_t& systemError) = 0;
    virtual bool QueryResponseHead(aris::update::HttpResponseHead& response,
                                   std::uint32_t& systemError) = 0;
    virtual NativeReadResult Read() = 0;
    virtual void Interrupt() = 0;
    virtual void Close() = 0;
};

std::unique_ptr<aris::update::HttpTransportPort> CreateWithNativeApi(
    std::unique_ptr<NativeHttpApi> api);

}  // namespace Platform::UpdateHttp::Testing
