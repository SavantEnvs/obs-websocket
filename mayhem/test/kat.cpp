// Known-answer tests for obs-websocket's request handling and authentication, run against the real
// project code and a headless libobs. Prints one `PASS <name>` / `FAIL <name>` line per check.
#include <cstdio>
#include <string>

#include "../harness/env.hpp"
#include "requesthandler/RequestHandler.h"
#include "utils/Crypto.h"

static int passed = 0;
static int failed = 0;

static void check(const char *name, bool ok)
{
	if (ok)
		passed++;
	else
		failed++;
	printf("%s %s\n", ok ? "PASS" : "FAIL", name);
}

static RequestResult call(const std::string &type, const json &data = json::object())
{
	RequestHandler handler;
	return handler.ProcessRequest(Request(type, data));
}

int main()
{
	HarnessEnvInit();
	HarnessObsStart();

	check("secret = base64(sha256(password + salt))",
	      Utils::Crypto::GenerateSecret("fuzzpassword", "fuzzsalt") == "FQ/g5t4PbLXRuLgZwGDSMq5uzq2fBifEV1dZMVHYCNE=");
	check("authentication string accepted for the right challenge",
	      Utils::Crypto::CheckAuthenticationString("FQ/g5t4PbLXRuLgZwGDSMq5uzq2fBifEV1dZMVHYCNE=", "fuzzchallenge",
							"LFrbPYL2SecvCplGvgJ9YcvKjQePEIqGDPuCinHO+cs="));
	check("authentication string rejected for another challenge",
	      !Utils::Crypto::CheckAuthenticationString("FQ/g5t4PbLXRuLgZwGDSMq5uzq2fBifEV1dZMVHYCNE=", "otherchallenge",
							 "LFrbPYL2SecvCplGvgJ9YcvKjQePEIqGDPuCinHO+cs="));

	RequestResult version = call("GetVersion");
	check("GetVersion succeeds", version.StatusCode == RequestStatus::Success);
	check("GetVersion reports plugin version 5.7.6", version.ResponseData["obsWebSocketVersion"] == "5.7.6");
	check("GetVersion reports rpc version 1", version.ResponseData["rpcVersion"] == 1);

	check("unknown request type is UnknownRequestType",
	      call("NoSuchRequest").StatusCode == RequestStatus::UnknownRequestType);
	check("empty request type is MissingRequestType", call("").StatusCode == RequestStatus::MissingRequestType);

	RequestResult created = call("CreateScene", {{"sceneName", "KatScene"}});
	check("CreateScene succeeds", created.StatusCode == RequestStatus::Success);
	check("CreateScene twice is ResourceAlreadyExists",
	      call("CreateScene", {{"sceneName", "KatScene"}}).StatusCode == RequestStatus::ResourceAlreadyExists);
	check("CreateScene without a name is MissingRequestField",
	      call("CreateScene").StatusCode == RequestStatus::MissingRequestField);

	RequestResult scenes = call("GetSceneList");
	bool listed = false;
	for (const json &scene : scenes.ResponseData["scenes"])
		listed = listed || scene["sceneName"] == "KatScene";
	check("GetSceneList lists the created scene", scenes.StatusCode == RequestStatus::Success && listed);

	RequestResult input = call("CreateInput", {{"sceneName", "KatScene"},
						   {"inputName", "KatInput"},
						   {"inputKind", "fuzz_input"},
						   {"inputSettings", {{"text", "hello"}}}});
	check("CreateInput succeeds", input.StatusCode == RequestStatus::Success);
	check("CreateInput returns the first scene item id", input.ResponseData["sceneItemId"] == 1);

	RequestResult settings = call("GetInputSettings", {{"inputName", "KatInput"}});
	check("GetInputSettings returns the stored text", settings.ResponseData["inputSettings"]["text"] == "hello");
	check("GetInputSettings returns the input kind", settings.ResponseData["inputKind"] == "fuzz_input");

	call("SetInputSettings", {{"inputName", "KatInput"}, {"inputSettings", {{"text", "changed"}}}});
	check("SetInputSettings is visible to GetInputSettings",
	      call("GetInputSettings", {{"inputName", "KatInput"}}).ResponseData["inputSettings"]["text"] == "changed");

	call("CreateInput", {{"sceneName", "KatScene"}, {"inputName", "KatMic"}, {"inputKind", "fuzz_audio_input"}});

	check("input starts unmuted", call("GetInputMute", {{"inputName", "KatMic"}}).ResponseData["inputMuted"] == false);
	call("SetInputMute", {{"inputName", "KatMic"}, {"inputMuted", true}});
	check("SetInputMute mutes the input",
	      call("GetInputMute", {{"inputName", "KatMic"}}).ResponseData["inputMuted"] == true);

	call("SetInputVolume", {{"inputName", "KatMic"}, {"inputVolumeMul", 0.5}});
	check("SetInputVolume is visible to GetInputVolume",
	      call("GetInputVolume", {{"inputName", "KatMic"}}).ResponseData["inputVolumeMul"] == 0.5);

	check("GetInputMute on a missing input is ResourceNotFound",
	      call("GetInputMute", {{"inputName", "NoSuchInput"}}).StatusCode == RequestStatus::ResourceNotFound);

	RequestResult items = call("GetSceneItemList", {{"sceneName", "KatScene"}});
	check("GetSceneItemList lists the input's scene item",
	      items.ResponseData["sceneItems"].size() == 2 && items.ResponseData["sceneItems"][0]["sourceName"] == "KatInput");

	check("RemoveInput succeeds", call("RemoveInput", {{"inputName", "KatInput"}}).StatusCode == RequestStatus::Success);

	HarnessObsStop();
	printf("TOTAL passed=%d failed=%d\n", passed, failed);
	return failed == 0 ? 0 : 1;
}
