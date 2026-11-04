// RetroAchievements' internet requests (achievements.cpp) and in-app updater (updater.cpp).
// On Nintendo Switch: devkitPro's libcurl over libnx's BSD sockets.
#include <string>

#if defined(__SWITCH__)
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <switch.h>
#include <curl/curl.h>
#endif

namespace Achievements
{

#if defined(__SWITCH__)

const char* SystemName() { return "Nintendo Switch"; }
bool Available() { return true; }

namespace
{
std::once_flag g_curlInit;
bool g_curlOk = false;
std::atomic<bool> g_stop{ false };

int Progress(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return g_stop ? 1 : 0;
}

// (Not curl_global_cleanup: the achievements' request may still be on its way on a thread of
// its own, left to finish, which then just fails.)
void CloseNetwork()
{
    g_stop = true;
    socketExit();
}

struct Sink
{
    std::string* body;
    size_t limit;
};
size_t Write(char* data, size_t size, size_t count, void* user)
{
    Sink* out = (Sink*)user;
    if (out->body->size() + size * count > out->limit) return 0;
    out->body->append(data, size * count);
    return size * count;
}

bool Request(const std::string& url, const std::string& post, const std::string& contentType,
             const std::string& userAgent, long timeout, size_t limit, int& status, std::string& body, std::string& error)
{
    // The console's network (libnx's sockets), started the first time it is needed and closed
    // as the program ends. It must be closed: the Homebrew Menu's loader starts the next .nro
    // (the game chosen, the list again, a new version) in this same process, and with the
    // sockets left open that start never came (a black screen).
    std::call_once(g_curlInit, [] {
        if (R_FAILED(socketInitializeDefault())) return;
        g_curlOk = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        atexit(CloseNetwork);
    });
    if (!g_curlOk) { error = "no network (sockets)"; return false; }
    if (g_stop) { error = "stopped"; return false; }
    CURL* c = curl_easy_init();
    if (!c) { error = "no curl"; return false; }
    curl_slist* headers = nullptr;
    if (!post.empty()) headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
    body.clear();
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout);
    if (!post.empty())
    {
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, post.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)post.size());
    }
    if (headers) curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    Sink sink = { &body, limit };
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, Write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, Progress);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    const CURLcode res = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    if (res != CURLE_OK) error = std::string("no connection (") + curl_easy_strerror(res) + ")";
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(c);
    status = (int)code;
    return res == CURLE_OK;
}
}

bool HttpPost(const std::string& url, const std::string& post, const std::string& contentType,
              const std::string& userAgent, int& status, std::string& body, std::string& error)
{
    return Request(url, post, contentType, userAgent, 30L, 8u << 20, status, body, error);
}

// (The updater's download, updater.cpp: the whole program, which may take minutes.)
bool HttpDownload(const std::string& url, const std::string& userAgent, int& status, std::string& body, std::string& error)
{
    return Request(url, "", "", userAgent, 900L, 512u << 20, status, body, error);
}

// (While on, a transfer going on another thread ends at once, failing, and none starts.)
void HttpStop(bool on)
{
    g_stop = on;
}

#else

const char* SystemName() { return "Desktop/Test"; }
bool Available() { return false; }

bool HttpPost(const std::string&, const std::string&, const std::string&, const std::string&, int&, std::string&,
              std::string& error)
{
    error = "not available on non-Switch test host";
    return false;
}

bool HttpDownload(const std::string&, const std::string&, int&, std::string&, std::string& error)
{
    error = "not available on non-Switch test host";
    return false;
}

void HttpStop(bool) {}

#endif

} // namespace Achievements
