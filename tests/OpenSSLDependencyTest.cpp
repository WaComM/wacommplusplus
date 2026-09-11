#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <curl/curl.h>

#include <cassert>
#include <cstring>

int main() {
    assert(OpenSSL_version_num()==OPENSSL_VERSION_NUMBER);
    assert(std::strcmp(OPENSSL_VERSION_STR, WACOMM_EXPECTED_OPENSSL)==0);
    assert(std::strcmp(OpenSSL_version(OPENSSL_VERSION_STRING), WACOMM_EXPECTED_OPENSSL)==0);
    SSL_CTX *context=SSL_CTX_new(TLS_client_method());
    assert(context!=nullptr);
    SSL_CTX_free(context);
    const curl_version_info_data *version=curl_version_info(CURLVERSION_NOW);
    assert(version!=nullptr);
    assert(version->features&CURL_VERSION_SSL);
    assert(version->ssl_version!=nullptr);
    assert(std::strstr(version->ssl_version, "OpenSSL/" WACOMM_EXPECTED_OPENSSL)!=nullptr);
}
