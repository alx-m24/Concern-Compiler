#include <curl/curl.h>
#include <cstdlib>
#include <iostream>
#include <string>

static size_t write_cb(char* p, size_t s, size_t n, void* out) {
    static_cast<std::string*>(out)->append(p, s * n);
    return s * n;
}

std::string fetch_csv(const std::string& url) {
    std::string body;
    CURL* c = curl_easy_init();
    if (!c) return "";

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);      // fail on HTTP 4xx/5xx
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);

    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK)
        std::cerr << "curl: " << curl_easy_strerror(rc) << '\n';
    curl_easy_cleanup(c);
    return rc == CURLE_OK ? body : "";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <csv-url>\n";
        return EXIT_FAILURE;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::string csv = fetch_csv(argv[1]);
    curl_global_cleanup();

    if (csv.empty()) return EXIT_FAILURE;
    std::cout << csv;
    return EXIT_SUCCESS;
}
