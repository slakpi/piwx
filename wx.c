#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <jansson.h>
#include "wx.h"

typedef struct __Response
{
  char *str;
  size_t len, bufLen;
} Response;

static void initResponse(Response *_res)
{
  _res->str = (char*)malloc(sizeof(char) * 256);
  _res->str[0] = 0;
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
      _res->str[_res->len] = 0;

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

static int parseUTCDateTime(struct tm *_tm, const char *_str)
{
  int ret = sscanf(_str, "%d-%d-%dT%d:%d:%d", &_tm->tm_year, &_tm->tm_mon,
    &_tm->tm_mday, &_tm->tm_hour, &_tm->tm_min, &_tm->tm_sec);

  if (ret < 6)
    return -1;
  if (_tm->tm_year < 1900)
    return -1;
  if (_tm->tm_mon < 1 || _tm->tm_mon > 12)
    return -1;

  _tm->tm_year -= 1900;
  _tm->tm_mon -= 1;
  _tm->tm_isdst = 0;

  return 0;
}

static size_t sunriseSunsetCallback(char *_ptr, size_t _size, size_t _nmemb,
  void *_userdata)
{
  Response *res = (Response*)_userdata;
  appendToResponse(res, _ptr, _nmemb);
  return _nmemb;
}

int getSunriseSunset(double _lat, double _lon, time_t *_sunrise,
  time_t *_sunset)
{
  CURL *curlLib;
  CURLcode res;
  json_t *root, *times, *sunrise, *sunset;
  json_error_t err;
  char url[257];
  Response json;
  struct tm dtSunrise, dtSunset;
  int ok = -1;

  curlLib = curl_easy_init();
  if (!curlLib)
    return -1;

  snprintf(url, 257, "https://api.sunrise-sunset.org/json?lat=%f&lng=%f&formatted=0",
    _lat, _lon);

  initResponse(&json);

  curl_easy_setopt(curlLib, CURLOPT_URL, url);
  curl_easy_setopt(curlLib, CURLOPT_WRITEFUNCTION, sunriseSunsetCallback);
  curl_easy_setopt(curlLib, CURLOPT_WRITEDATA, &json);
  res = curl_easy_perform(curlLib);
  curl_easy_cleanup(curlLib);

  if (res != CURLE_OK)
    return -1;

  root = json_loads(json.str, 0, &err);
  freeResponse(&json);

  if (!root)
    return -1;

  times = json_object_get(root, "results");
  if (!json_is_object(times))
    goto cleanup;

  sunrise = json_object_get(times, "sunrise");
  if (!json_is_string(sunrise))
    goto cleanup;

  sunset = json_object_get(times, "sunset");
  if (!json_is_string(sunset))
    goto cleanup;

  if (parseUTCDateTime(&dtSunrise, json_string_value(sunrise)) != 0)
    goto cleanup;
  
  if (parseUTCDateTime(&dtSunset, json_string_value(sunset)) != 0)
    goto cleanup;

  *_sunrise = timegm(&dtSunrise);
  *_sunset = timegm(&dtSunset);

  ok = 0;

cleanup:
  if (root)
    json_decref(root);
  if (ok != 0)
  {
    *_sunrise = 0;
    *_sunset = 0;
  }

  return ok;
}