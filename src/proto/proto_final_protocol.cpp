//------------------------------------------------------------------------------
// Automatically generated by the Fast Binary Encoding compiler, do not modify!
// https://github.com/chronoxor/FastBinaryEncoding
// Source: message.fbe
// FBE version: 1.14.5.0
//------------------------------------------------------------------------------

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4065) // C4065: switch statement contains 'default' but no 'case' labels
#endif

#include "proto_final_protocol.h"

namespace FBE {

namespace proto {

size_t FinalSender::send(const ::proto::MessageRequest& value)
{
    // Serialize the value into the FBE stream
    size_t serialized = MessageRequestModel.serialize(value);
    assert((serialized > 0) && "proto::MessageRequest serialization failed!");
    assert(MessageRequestModel.verify() && "proto::MessageRequest validation failed!");

    // Log the value
    if (this->_logging)
    {
        std::string message = value.string();
        this->onSendLog(message);
    }

    // Send the serialized value
    return this->send_serialized(serialized);
}

size_t FinalSender::send(const ::proto::MessageResponse& value)
{
    // Serialize the value into the FBE stream
    size_t serialized = MessageResponseModel.serialize(value);
    assert((serialized > 0) && "proto::MessageResponse serialization failed!");
    assert(MessageResponseModel.verify() && "proto::MessageResponse validation failed!");

    // Log the value
    if (this->_logging)
    {
        std::string message = value.string();
        this->onSendLog(message);
    }

    // Send the serialized value
    return this->send_serialized(serialized);
}

size_t FinalSender::send(const ::proto::MessageReject& value)
{
    // Serialize the value into the FBE stream
    size_t serialized = MessageRejectModel.serialize(value);
    assert((serialized > 0) && "proto::MessageReject serialization failed!");
    assert(MessageRejectModel.verify() && "proto::MessageReject validation failed!");

    // Log the value
    if (this->_logging)
    {
        std::string message = value.string();
        this->onSendLog(message);
    }

    // Send the serialized value
    return this->send_serialized(serialized);
}

size_t FinalSender::send(const ::proto::MessageNotify& value)
{
    // Serialize the value into the FBE stream
    size_t serialized = MessageNotifyModel.serialize(value);
    assert((serialized > 0) && "proto::MessageNotify serialization failed!");
    assert(MessageNotifyModel.verify() && "proto::MessageNotify validation failed!");

    // Log the value
    if (this->_logging)
    {
        std::string message = value.string();
        this->onSendLog(message);
    }

    // Send the serialized value
    return this->send_serialized(serialized);
}

size_t FinalSender::send(const ::proto::DisconnectRequest& value)
{
    // Serialize the value into the FBE stream
    size_t serialized = DisconnectRequestModel.serialize(value);
    assert((serialized > 0) && "proto::DisconnectRequest serialization failed!");
    assert(DisconnectRequestModel.verify() && "proto::DisconnectRequest validation failed!");

    // Log the value
    if (this->_logging)
    {
        std::string message = value.string();
        this->onSendLog(message);
    }

    // Send the serialized value
    return this->send_serialized(serialized);
}

bool FinalReceiver::onReceive(size_t type, const void* data, size_t size)
{
    switch (type)
    {
        case FBE::proto::MessageRequestFinalModel::fbe_type():
        {
            // Deserialize the value from the FBE stream
            MessageRequestModel.attach(data, size);
            assert(MessageRequestModel.verify() && "proto::MessageRequest validation failed!");
            [[maybe_unused]] size_t deserialized = MessageRequestModel.deserialize(MessageRequestValue);
            assert((deserialized > 0) && "proto::MessageRequest deserialization failed!");

            // Log the value
            if (this->_logging)
            {
                std::string message = MessageRequestValue.string();
                this->onReceiveLog(message);
            }

            // Call receive handler with deserialized value
            onReceive(MessageRequestValue);
            return true;
        }
        case FBE::proto::MessageResponseFinalModel::fbe_type():
        {
            // Deserialize the value from the FBE stream
            MessageResponseModel.attach(data, size);
            assert(MessageResponseModel.verify() && "proto::MessageResponse validation failed!");
            [[maybe_unused]] size_t deserialized = MessageResponseModel.deserialize(MessageResponseValue);
            assert((deserialized > 0) && "proto::MessageResponse deserialization failed!");

            // Log the value
            if (this->_logging)
            {
                std::string message = MessageResponseValue.string();
                this->onReceiveLog(message);
            }

            // Call receive handler with deserialized value
            onReceive(MessageResponseValue);
            return true;
        }
        case FBE::proto::MessageRejectFinalModel::fbe_type():
        {
            // Deserialize the value from the FBE stream
            MessageRejectModel.attach(data, size);
            assert(MessageRejectModel.verify() && "proto::MessageReject validation failed!");
            [[maybe_unused]] size_t deserialized = MessageRejectModel.deserialize(MessageRejectValue);
            assert((deserialized > 0) && "proto::MessageReject deserialization failed!");

            // Log the value
            if (this->_logging)
            {
                std::string message = MessageRejectValue.string();
                this->onReceiveLog(message);
            }

            // Call receive handler with deserialized value
            onReceive(MessageRejectValue);
            return true;
        }
        case FBE::proto::MessageNotifyFinalModel::fbe_type():
        {
            // Deserialize the value from the FBE stream
            MessageNotifyModel.attach(data, size);
            assert(MessageNotifyModel.verify() && "proto::MessageNotify validation failed!");
            [[maybe_unused]] size_t deserialized = MessageNotifyModel.deserialize(MessageNotifyValue);
            assert((deserialized > 0) && "proto::MessageNotify deserialization failed!");

            // Log the value
            if (this->_logging)
            {
                std::string message = MessageNotifyValue.string();
                this->onReceiveLog(message);
            }

            // Call receive handler with deserialized value
            onReceive(MessageNotifyValue);
            return true;
        }
        case FBE::proto::DisconnectRequestFinalModel::fbe_type():
        {
            // Deserialize the value from the FBE stream
            DisconnectRequestModel.attach(data, size);
            assert(DisconnectRequestModel.verify() && "proto::DisconnectRequest validation failed!");
            [[maybe_unused]] size_t deserialized = DisconnectRequestModel.deserialize(DisconnectRequestValue);
            assert((deserialized > 0) && "proto::DisconnectRequest deserialization failed!");

            // Log the value
            if (this->_logging)
            {
                std::string message = DisconnectRequestValue.string();
                this->onReceiveLog(message);
            }

            // Call receive handler with deserialized value
            onReceive(DisconnectRequestValue);
            return true;
        }
        default: break;
    }

    return false;
}

std::future<::proto::MessageResponse> FinalClient::request(const ::proto::MessageRequest& value, uint64_t timeout)
{
    std::scoped_lock locker(this->_lock);

    std::promise<::proto::MessageResponse> promise;
    std::future<::proto::MessageResponse> future = promise.get_future();

    uint64_t current = utc();

    // Send the request message
    size_t serialized = FinalSender::send(value);
    if (serialized > 0)
    {
        // Calculate the unique timestamp
        this->_timestamp = (current <= this->_timestamp) ? this->_timestamp + 1 : current;

        // Register the request
        _requests_by_id_MessageResponse.insert(std::make_pair(value.id, std::make_tuple(this->_timestamp, timeout * 1000000, std::move(promise))));
        if (timeout > 0)
            _requests_by_timestamp_MessageResponse.insert(std::make_pair(this->_timestamp, value.id));
    }
    else
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Send request failed!")));

    return future;
}

std::future<void> FinalClient::request(const ::proto::DisconnectRequest& value, uint64_t timeout)
{
    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    // Send the request message
    size_t serialized = FinalSender::send(value);
    if (serialized > 0)
        promise.set_value();
    else
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Send request failed!")));

    return future;
}

bool FinalClient::onReceiveResponse(const ::proto::MessageResponse& response)
{
    std::scoped_lock locker(this->_lock);

    auto it_MessageResponse = _requests_by_id_MessageResponse.find(response.id);
    if (it_MessageResponse != _requests_by_id_MessageResponse.end())
    {
        auto timestamp = std::get<0>(it_MessageResponse->second);
        [[maybe_unused]] auto timespan = std::get<1>(it_MessageResponse->second);
        auto& promise = std::get<2>(it_MessageResponse->second);
        promise.set_value(response);
        _requests_by_id_MessageResponse.erase(response.id);
        _requests_by_timestamp_MessageResponse.erase(timestamp);
        return true;
    }

    return false;
}

bool FinalClient::onReceiveReject(const ::proto::MessageReject& reject)
{
    std::scoped_lock locker(this->_lock);

    auto it_MessageResponse = _requests_by_id_MessageResponse.find(reject.id);
    if (it_MessageResponse != _requests_by_id_MessageResponse.end())
    {
        auto timestamp = std::get<0>(it_MessageResponse->second);
        [[maybe_unused]] auto timespan = std::get<1>(it_MessageResponse->second);
        auto& promise = std::get<2>(it_MessageResponse->second);
        promise.set_exception(std::make_exception_ptr(std::runtime_error(reject.string())));
        _requests_by_id_MessageResponse.erase(reject.id);
        _requests_by_timestamp_MessageResponse.erase(timestamp);
        return true;
    }

    return false;
}

void FinalClient::reset_requests()
{
    FinalSender::reset();
    FinalReceiver::reset();

    for (auto& request : _requests_by_id_MessageResponse)
        std::get<2>(request.second).set_exception(std::make_exception_ptr(std::runtime_error("Reset client!")));
    _requests_by_id_MessageResponse.clear();
    _requests_by_timestamp_MessageResponse.clear();
}

void FinalClient::watchdog_requests(uint64_t utc)
{
    auto it_request_by_timestamp_MessageResponse = _requests_by_timestamp_MessageResponse.begin();
    while (it_request_by_timestamp_MessageResponse != _requests_by_timestamp_MessageResponse.end())
    {
        auto& it_request_by_id_MessageResponse = _requests_by_id_MessageResponse[it_request_by_timestamp_MessageResponse->second];
        auto id = it_request_by_timestamp_MessageResponse->second;
        auto timestamp = std::get<0>(it_request_by_id_MessageResponse);
        auto timespan = std::get<1>(it_request_by_id_MessageResponse);
        if ((timestamp + timespan) <= utc)
        {
            auto& promise = std::get<2>(it_request_by_id_MessageResponse);
            promise.set_exception(std::make_exception_ptr(std::runtime_error("Timeout!")));
            _requests_by_id_MessageResponse.erase(id);
            _requests_by_timestamp_MessageResponse.erase(timestamp);
            it_request_by_timestamp_MessageResponse = _requests_by_timestamp_MessageResponse.begin();
            continue;
        }
        else
            break;
    }

}

} // namespace proto

} // namespace FBE

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
