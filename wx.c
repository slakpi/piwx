#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "wx.h"

typedef struct __Response
{
  char *str;
  size_t len, bufLen;
} Response;

static void initResponse(Response *_res)
{
  _res->str = (char*)malloc(sizeof(char) * 256);
  _res->len = 0;
  _res->bufLen = 256;
}

static void appendToResponse(Response *_res, const char *_str, size_t _len)
{
  size_t newBuf = _res->bufLen;

  if (!_res->str)
    return;

  while (1)
  {
    if (_res->len + _len < newBuf - 1)
    {
      if (_res->bufLen < newBuf)
      {
        _res->str = realloc(_res->str, newBuf);
        _res->bufLen = newBuf;
      }

      memcpy(_res->str + _res->len, _str, _len);
      _res->len += _len;

      return;
    }

    newBuf = _res->bufLen << 1;
    if (newBuf < _res->bufLen)
      return;
  }
}

static void freeResponse(Response *_res)
{
  free(_res->str);
  _res->str = NULL;
  _res->len = 0;
  _res->bufLen = 0;
}

static size_t sunriseSunsetCallback(char *_ptr, size_t _size, size_t _nmemb,
  void *_userdata)
{
  Response *res = (Response*)_userdata;
  appendToResponse(res, _ptr, _nmemb);
  return _nmemb;
}

int getSunriseSunset(struct tm *_sunrise, struct tm *_sunset)
{
  CURL *curlLib;
  CURLcode res;
  Response json;

  memset(_sunrise, 0, sizeof(struct tm));
  memset(_sunset, 0, sizeof(struct tm));

  curlLib = curl_easy_init();
  if (!curlLib)
    return -1;

  initResponse(&json);

  curl_easy_setopt(curlLib, CURLOPT_URL, "https://api.sunrise-sunset.org/json?lat=45&lng=-122");
  curl_easy_setopt(curlLib, CURLOPT_WRITEFUNCTION, sunriseSunsetCallback);
  curl_easy_setopt(curlLib, CURLOPT_WRITEDATA, &json);
  res = curl_easy_perform(curlLib);
  curl_easy_cleanup(curlLib);

  if (res != CURLE_OK)
    return -1;

  json.str[json.len] = 0;
  printf("%s\n", json.str);
  freeResponse(&json);

  return 0;
}
