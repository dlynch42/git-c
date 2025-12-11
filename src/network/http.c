#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"

// Callback function for curl; accumulates response data
static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    HttpResponse *response = (HttpResponse *)userp;

    // Reallocate memory to hold the new data
    unsigned char *newData = realloc(response->data, response->size + totalSize);
    if (newData == NULL) {
        // Memory allocation failed
        fprintf(stderr, "Error: Failed to alloate memory for HTTP response\n");
        return 0; // curl will abort the request
    }

    response->data = newData;
    memcpy(&(response->data[response->size]), contents, totalSize);
    response->size += totalSize;

    return totalSize;
}

// Generic GET request
int httpGet(const char *url, HttpResponse *response) {
    CURL *curl;
    CURLcode res;

    // Init response
    response->data = NULL;
    response->size = 0;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Failed to initialize curl\n");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "git/codecrafters"); // "libcurl-agent/1.0"

    res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Error: curl GET failed : %s\n", curl_easy_strerror(res));
        free(response->data);
        response->data = NULL;
        response->size = 0;
        return -1;
    }

    return 0;
};

// Generic POST request
int httpPost(const char *url, const char *contentType, const unsigned char *body, size_t bodyLen, HttpResponse *response) {
    CURL *curl;
    CURLcode res;

    // Init response
    response->data = NULL;
    response->size = 0;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Failed to initialize curl\n");
        return -1;
    }

    // set content-type header
    char contentTypeHeader[128];
    snprintf(contentTypeHeader, sizeof(contentTypeHeader), "Content-Type: %s", contentType);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, contentTypeHeader);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, bodyLen);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "git/codecrafters"); // "libcurl-agent/1.0"

    res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Error: curl POST failed : %s\n", curl_easy_strerror(res));
        free(response->data);
        response->data = NULL;
        response->size = 0;
        return -1;
    }

    return 0;
};