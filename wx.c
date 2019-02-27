#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include "wx.h"

static size_t sunriseSunsetCallback(char *_ptr, size_t _size, size_t _nmemb, void *_userdata)
{
  fwrite(_ptr, 1, _nmemb, stdout);

  return _nmemb;
}

int getSunriseSunset(struct tm *_sunrise, struct tm *_sunset)
{
  CURL *curlLib;
  CURLcode res;

  memset(_sunrise, 0, sizeof(struct tm));
  memset(_sunset, 0, sizeof(struct tm));

  curlLib = curl_easy_init();
  if (!curlLib)
    return -1;

  curl_easy_setopt(curlLib, CURLOPT_URL, "https://api.sunrise-sunset.org/json?lat=45&lng=-122");
  curl_easy_setopt(curlLib, CURLOPT_WRITEFUNCTION, sunriseSunsetCallback);
  res = curl_easy_perform(curlLib);
  curl_easy_cleanup(curlLib);

  if (res != CURLE_OK)
    return -1;

  return 0;
}
