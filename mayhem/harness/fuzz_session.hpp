// Shared body of the obs-websocket protocol harnesses. Each input is delivered to the real
// WebSocketServer::onMessage (decode -> ProcessMessage -> RequestHandler / RequestBatchHandler) of a
// fresh session, against a freshly started headless libobs.
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <QThreadPool>

#include "Config.h"
#include "env.hpp"
#include "utils/Crypto.h"
#include "websocketserver/WebSocketServer.h"
#include <websocketpp/utf8_validator.hpp>

// WebSocketServer keeps onMessage and its session table private. Explicit instantiation is exempt
// from access checks, which is how the harness reaches them without editing upstream.
using MessagePtr = websocketpp::server<websocketpp::config::asio>::message_ptr;
using OnMessageFn = void (WebSocketServer::*)(websocketpp::connection_hdl, MessagePtr);
using SessionMap = std::map<websocketpp::connection_hdl, SessionPtr, std::owner_less<websocketpp::connection_hdl>>;

template <typename Tag, typename Tag::type Member> struct Steal {
	friend typename Tag::type Get(Tag) { return Member; }
};
struct OnMessageTag {
	using type = OnMessageFn;
	friend type Get(OnMessageTag);
};
struct SessionsTag {
	using type = SessionMap WebSocketServer::*;
	friend type Get(SessionsTag);
};
struct ThreadPoolTag {
	using type = QThreadPool WebSocketServer::*;
	friend type Get(ThreadPoolTag);
};
template struct Steal<OnMessageTag, &WebSocketServer::onMessage>;
template struct Steal<SessionsTag, &WebSocketServer::_sessions>;
template struct Steal<ThreadPoolTag, &WebSocketServer::_threadPool>;

enum class SessionKind {
	// Already identified; messages are JSON text frames, one per line.
	IdentifiedJson,
	// Not identified, password authentication required; messages are JSON text frames, one per line.
	AuthJson,
	// Already identified; the whole input is one MessagePack binary frame.
	IdentifiedMsgPack,
};

static constexpr size_t kMaxMessages = 64;
static const char *const kPassword = "fuzzpassword";
static const char *const kSalt = "fuzzsalt";
static const char *const kChallenge = "fuzzchallenge";

static void RunMessage(const std::string &payload, websocketpp::frame::opcode::value opcode)
{
	static auto manager = std::make_shared<websocketpp::config::asio::con_msg_manager_type>();
	MessagePtr message = manager->get_message(opcode, payload.size());
	message->set_payload(payload);

	WebSocketServer &server = HarnessServer();
	(server.*Get(OnMessageTag()))(websocketpp::connection_hdl(), message);
	(server.*Get(ThreadPoolTag())).waitForDone();
}

static int RunSession(const uint8_t *data, size_t size, SessionKind kind)
{
	std::vector<std::string> frames;
	bool text = kind != SessionKind::IdentifiedMsgPack;
	if (text) {
		// Text frames reach the message handler only after websocketpp has validated them as UTF-8
		// (websocketpp/processors/hybi13.hpp), so the handler never sees anything else.
		if (!websocketpp::utf8_validator::validate(std::string(reinterpret_cast<const char *>(data), size)))
			return -1;
		size_t start = 0;
		while (start <= size && frames.size() < kMaxMessages) {
			size_t end = start;
			while (end < size && data[end] != '\n')
				end++;
			if (end > start)
				frames.emplace_back(reinterpret_cast<const char *>(data) + start, end - start);
			start = end + 1;
		}
	} else {
		frames.emplace_back(reinterpret_cast<const char *>(data), size);
	}
	if (frames.empty())
		return -1;

	HarnessObsStart();

	WebSocketServer &server = HarnessServer();
	auto session = std::make_shared<WebSocketSession>();
	session->SetEncoding(text ? WebSocketServer::Json : WebSocketServer::MsgPack);
	if (kind == SessionKind::AuthJson) {
		session->SetAuthenticationRequired(true);
		session->SetSecret(Utils::Crypto::GenerateSecret(kPassword, kSalt));
		session->SetChallenge(kChallenge);
	} else {
		session->SetIsIdentified(true);
	}
	(server.*Get(SessionsTag()))[websocketpp::connection_hdl()] = session;

	std::atomic<bool> ticking{true};
	std::thread frameClock([&ticking] {
		while (ticking.load(std::memory_order_relaxed))
			HarnessTickFrames();
	});

	for (const std::string &frame : frames)
		RunMessage(frame, text ? websocketpp::frame::opcode::text : websocketpp::frame::opcode::binary);

	ticking = false;
	frameClock.join();
	(server.*Get(SessionsTag())).clear();
	HarnessObsStop();
	return 0;
}

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	HarnessEnvInit();
	return 0;
}
