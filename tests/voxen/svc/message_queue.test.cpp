#include <voxen/svc/message_queue.hpp>

#include <voxen/svc/engine.hpp>
#include <voxen/svc/messaging_service.hpp>

#include "../../voxen_test_common.hpp"

namespace voxen::svc
{

namespace
{

struct TestUnicastMessage {
	constexpr static UID MESSAGE_UID = UID("1fc82db5-ea75f28a-c21c223b-10663645");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;

	std::unique_ptr<int> owned_object;
};

struct TestUnicastSignal {
	constexpr static UID MESSAGE_UID = UID("c2b6fae1-a1aded58-0f054134-53d47bec");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;
};

struct TestRequestMessage {
	constexpr static UID MESSAGE_UID = UID("dc098141-b47700f8-2d43b146-c5c74611");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Request;

	int a;
	int b;
	int sum;
};

constexpr UID U1("8819c518-0260c91d-db31ab20-f0daee10");
constexpr UID U2("eb934a1d-ea3777fe-8aeaf67f-13149325");
constexpr UID U3("5eba2318-3dd0e03a-7101e4e9-e7b8dbea");

} // namespace

TEST_CASE("'MessageQueue' basic unicast test", "[voxen::svc::message_queue]")
{
	auto engine = Engine::create();

	std::unique_ptr<int> received_object;
	MessageQueue mq1;
	MessageQueue mq2;

	auto u2_message_handler = [&](TestUnicastMessage& msg, MessageInfo& info) {
		CHECK(info.senderUid() == U1);
		CHECK(msg.owned_object != nullptr);
		CHECK(*msg.owned_object == 10);
		*msg.owned_object += 10;
		received_object = std::move(msg.owned_object);

		mq2.send<TestUnicastSignal>(U1);
	};

	auto u1_signal_handler = [&](MessageInfo& info) {
		CHECK(info.senderUid() == U2);
		CHECK(received_object != nullptr);
		CHECK(*received_object == 20);
		*received_object += 10;
	};

	{
		auto& msg = engine->serviceLocator().requestService<MessagingService>();

		mq1 = msg.registerAgent(U1);
		mq1.registerHandler<TestUnicastSignal>(u1_signal_handler);

		mq2 = msg.registerAgent(U2);
		mq2.registerHandler<TestUnicastMessage>(u2_message_handler);
	}

	// Send `TestUnicastMessage` from U1 to U2
	mq1.send<TestUnicastMessage>(U2, std::make_unique<int>(10));
	// U2 will send `TestUnicastSignal` back to U1 in handler
	mq2.waitMessages();
	// U1 will receive this signal
	mq1.waitMessages();

	// All expected message handling must occur
	CHECK(received_object != nullptr);
	CHECK(*received_object == 30);
}

TEST_CASE("'MessageQueue' basic request test", "[voxen::svc::message_queue]")
{
	auto engine = Engine::create();

	MessageQueue mq1;
	MessageQueue mq2;
	MessageQueue mq3;

	auto good_request_handler = [&](TestRequestMessage& msg, MessageInfo& info) {
		CHECK(info.senderUid() == U1);
		msg.sum = msg.a + msg.b;
	};

	auto bad_request_handler = [&](TestRequestMessage& msg, MessageInfo& info) {
		CHECK(info.senderUid() == U1);
		msg.sum = 1337;
		throw std::runtime_error("boom");
	};

	{
		auto& msg = engine->serviceLocator().requestService<MessagingService>();

		mq1 = msg.registerAgent(U1);

		mq2 = msg.registerAgent(U2);
		mq2.registerHandler<TestRequestMessage>(good_request_handler);

		mq3 = msg.registerAgent(U3);
		mq3.registerHandler<TestRequestMessage>(bad_request_handler);
	}

	// First check it with handle-based tracking

	{
		INFO("Sending request to good handler with handle-based tracking");

		auto rqh = mq1.requestWithHandle<TestRequestMessage>(U2, 5, 10, -1);
		CHECK(rqh.status() == RequestStatus::Pending);

		mq2.waitMessages();
		CHECK(rqh.status() == RequestStatus::Complete);
		CHECK(rqh.payload().sum == 15);
	}

	{
		INFO("Sending requets to bad handler with handle-based tracking");

		auto rqh = mq1.requestWithHandle<TestRequestMessage>(U3, 5, 10, -1);
		CHECK(rqh.status() == RequestStatus::Pending);

		mq3.waitMessages();
		CHECK(rqh.status() == RequestStatus::Failed);
		CHECK(rqh.payload().sum == 1337);
		CHECK_THROWS_WITH(rqh.rethrowIfFailed(), "boom");
	}

	{
		INFO("Sending request to missing handler with handle-based tracking");

		// Yes, send to itself, a nice bonus test case
		auto rqh = mq1.requestWithHandle<TestRequestMessage>(U1, 5, 10, -1);
		CHECK(rqh.status() == RequestStatus::Pending);

		mq1.waitMessages();
		CHECK(rqh.status() == RequestStatus::Dropped);
		CHECK(rqh.payload().sum == -1);
	}

	{
		INFO("Sending request to invalid address with handle-based tracking");

		auto rqh = mq1.requestWithHandle<TestRequestMessage>(UID(0, 0), 5, 10, -1);
		rqh.wait();
		CHECK(rqh.status() == RequestStatus::Dropped);
		CHECK(rqh.payload().sum == -1);
	}

	// Now do the same with completion messages

	{
		INFO("Sending request to good handler with completion message");

		bool received = false;

		mq1.registerCompletionHandler<TestRequestMessage>([&](TestRequestMessage& msg, RequestCompletionInfo& info) {
			CHECK(info.status() == RequestStatus::Complete);
			CHECK(msg.sum == 15);
			received = true;
		});

		mq1.requestWithCompletion<TestRequestMessage>(U2, 5, 10, -1);
		// Wait for this request to process
		mq2.waitMessages();
		// Wait for completion message
		mq1.waitMessages();

		CHECK(received == true);
	}

	{
		INFO("Sending requets to bad handler with completion message");

		bool received = false;

		mq1.registerCompletionHandler<TestRequestMessage>([&](TestRequestMessage& msg, RequestCompletionInfo& info) {
			CHECK(info.status() == RequestStatus::Failed);
			CHECK(msg.sum == 1337);
			CHECK_THROWS_WITH(info.rethrowIfFailed(), "boom");
			received = true;
		});

		mq1.requestWithCompletion<TestRequestMessage>(U3, 5, 10, -1);
		// Wait for this request to process
		mq3.waitMessages();
		// Wait for completion message
		mq1.waitMessages();

		CHECK(received == true);
	}

	{
		INFO("Sending request to missing handler with completion message");

		bool received = false;

		mq1.registerCompletionHandler<TestRequestMessage>([&](TestRequestMessage& msg, RequestCompletionInfo& info) {
			CHECK(info.status() == RequestStatus::Dropped);
			CHECK(msg.sum == -1);
			received = true;
		});

		// Yes, send to itself, a nice bonus test case
		mq1.requestWithCompletion<TestRequestMessage>(U1, 5, 10, -1);

		// There are several events:
		// 1. Routing request message from U1 to U1
		// 2. Receiving request message (and dropping it)
		// 3. Routing completion message from U1 to U1
		// 4. Receiving completion message (and handling it)
		//
		// Everything except (4) can be either synchronous or not - it's an implementation detail.
		//
		// If we call `waitMessages()` once it could process just (2) and never get to (4).
		// On the other hand, if we call it twice, the first call could process both (2) and (4),
		// making the second call deadlock (waiting infinitely for nothing).
		//
		// So specify some short but reasonable timeout for the second call.
		mq1.waitMessages();
		// 10 milliseconds should be more than enough to deliver a single message
		mq1.waitMessages(10);

		CHECK(received == true);
	}

	{
		INFO("Sending request to invalid address with completion message");

		bool received = false;

		mq1.registerCompletionHandler<TestRequestMessage>([&](TestRequestMessage& msg, RequestCompletionInfo& info) {
			CHECK(info.status() == RequestStatus::Dropped);
			CHECK(msg.sum == -1);
			received = true;
		});

		mq1.requestWithCompletion<TestRequestMessage>(UID(0, 0), 5, 10, -1);
		// Wait for completion message
		mq1.waitMessages();

		CHECK(received == true);
	}
}

} // namespace voxen::svc
