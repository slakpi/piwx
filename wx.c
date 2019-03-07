#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <curl/curl.h>
#include <jansson.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
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

static int getSunriseSunset(double _lat, double _lon, time_t *_sunrise,
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

typedef enum __Tag
{
  tagInvalid = -1,

  tagResponse = 1,

  tagData,

  tagRawText,     tagStationId,     tagObsTime,       tagLat,         tagLon,
  tagTemp,        tagDewpoint,      tagWindDir,       tagWindSpeed,
  tagWindGust,    tagVis,           tagAlt,           tagCategory,

  tagFirst = tagResponse,
  tagLast = tagCategory
} Tag;

static const Tag tags[] = {
  tagResponse,

  tagData,

  tagRawText,     tagStationId,     tagObsTime,       tagLat,         tagLon,
  tagTemp,        tagDewpoint,      tagWindDir,       tagWindSpeed,
  tagWindGust,    tagVis,           tagAlt,           tagCategory
};

static const char* tagNames[] = {
  "response",

  "data",

  "raw_text",       "station_id",     "observation_time",       "latitude",
  "longitude",      "temp_c",         "dewpoint_c",             "wind_dir_degrees",
  "wind_speed_kt",  "wind_gust_kt",   "visibility_statute_mi",  "altim_in_hg",
  "flight_category",

  NULL
};

static void initHash(xmlHashTablePtr _hash)
{
  for (long i = 0; tagNames[i]; ++i)
    xmlHashAddEntry(_hash, (xmlChar*)tagNames[i], (void*)tags[i]);
}

static void hashDealloc(void *_payload, xmlChar *_name)
{

}

static Tag getTag(xmlHashTablePtr _hash, const xmlChar *_tag)
{
  void *p = xmlHashLookup(_hash, _tag);

  if (!p)
    return tagInvalid;

  return (Tag)p;
}

typedef struct __METARCallbackData
{
  xmlParserCtxtPtr ctxt;
} METARCallbackData;

static size_t metarCallback(char *_ptr, size_t _size, size_t _nmemb,
  void *_userdata)
{
  METARCallbackData *data = (METARCallbackData*)_userdata;
  size_t res = _nmemb;

  if (!data->ctxt)
  {
    data->ctxt = xmlCreatePushParserCtxt(NULL, NULL, _ptr, _nmemb, NULL);
    if (!data->ctxt)
      res = 0;
  }
  else
  {
    if (xmlParseChunk(data->ctxt, _ptr, _nmemb, 0) != 0)
      res = 0;
  }

  return res;
}

static void readLayers(xmlNodePtr _node, xmlHashTablePtr _hash,
  WxStation *_station)
{

}

static void readStation(xmlNodePtr _node, xmlHashTablePtr _hash,
  WxStation *_station)
{
  Tag tag;
  struct tm obs;
  xmlNodePtr c = _node->children;

  while (c)
  {
    if (c->type == XML_TEXT_NODE)
    {
      c = c->next;
      continue;
    }

    tag = getTag(_hash, c->name);
    switch (tag)
    {
    case tagRawText:
      _station->raw = strdup((char*)c->children->content);
      break;
    case tagStationId:
      _station->id = strdup((char*)c->children->content);
      break;
    case tagObsTime:
      parseUTCDateTime(&obs, (char*)c->children->content);
      _station->obsTime = mktime(&obs);
      break;
    case tagLat:
      _station->lat = strtod((char*)c->children->content, NULL);
      break;
    case tagLon:
      _station->lon = strtod((char*)c->children->content, NULL);
      break;
    case tagTemp:
      _station->temp = strtod((char*)c->children->content, NULL);
      break;
    case tagDewpoint:
      _station->dewPoint = strtod((char*)c->children->content, NULL);
      break;
    case tagWindDir:
      _station->windDir = atoi((char*)c->children->content);
      break;
    case tagWindSpeed:
      _station->windSpeed = atoi((char*)c->children->content);
      break;
    case tagVis:
      _station->visibility = strtod((char*)c->children->content, NULL);
      break;
    case tagAlt:
      _station->alt = strtod((char*)c->children->content, NULL);
      break;
    default:
      break;
    }

    c = c->next;
  }
}

WxStation* queryWx(int _stations, ...)
{
  va_list args;
  CURL *curlLib;
  CURLcode res;
  char url[4096];
  size_t count, len;
  METARCallbackData data;
  xmlDocPtr doc = NULL;
  xmlNodePtr p;
  xmlHashTablePtr hash = NULL;
  Tag tag;
  WxStation *start = NULL, *cur, *n;
  int ok = 0;

  if (_stations < 1)
    return NULL;

  strncpy(url,
    "https://aviationweather.gov/adds/dataserver_current/httpparam?"
    "dataSource=metars&"
    "requestType=retrieve&"
    "format=xml&"
    "hoursBeforeNow=1&"
    "stationString=",
    4096);
  count = 4096 - strlen(url);

  va_start(args, _stations);

  for (int i = 0; i < _stations; ++i)
  {
    const char *station = va_arg(args, const char*);
    len = strlen(station);
    if (i < _stations - 1)
      len += 3;

    if (len > count)
      break;

    strcat(url, station);
    if (i < _stations - 1)
      strcat(url, "%20");
  }

  va_end(args);

  curlLib = curl_easy_init();
  if (!curlLib)
    return NULL;

  data.ctxt = NULL;
  curl_easy_setopt(curlLib, CURLOPT_URL, url);
  curl_easy_setopt(curlLib, CURLOPT_WRITEFUNCTION, metarCallback);
  curl_easy_setopt(curlLib, CURLOPT_WRITEDATA, &data);
  res = curl_easy_perform(curlLib);
  curl_easy_cleanup(curlLib);

  if (data.ctxt)
  {
    xmlParseChunk(data.ctxt, NULL, 0, 1);
    doc = data.ctxt->myDoc;
    xmlFreeParserCtxt(data.ctxt);
  }

  if (res != CURLE_OK)
    goto cleanup;

  hash = xmlHashCreate(tagLast);
  initHash(hash);

  p = doc->children;
  while (p)
  {
    tag = getTag(hash, p->name);
    if (tag == tagResponse)
      break;

    p = p->next;
  }

  if (!p)
    goto cleanup;

  p = p->children;
  while (p)
  {
    tag = getTag(hash, p->name);
    if (tag == tagData)
      break;

    p = p->next;
  }

  if (!p)
    goto cleanup;

  p = p->children;
  while (p)
  {
    if (p->type == XML_TEXT_NODE)
    {
      p = p->next;
      continue;
    }

    n = (WxStation*)malloc(sizeof(WxStation));
    memset(n, 0, sizeof(WxStation));

    if (!start)
      start = cur = n;
    else
    {
      cur->next = n;
      cur = n;
    }

    readStation(p, hash, n);
    getSunriseSunset(n->lat, n->lon, &n->sunrise, &n->sunset);

    p = p->next;
  }

  ok = 1;

cleanup:
  if (doc)
    xmlFreeDoc(doc);
  if (hash)
    xmlHashFree(hash, hashDealloc);
  if (start && !ok)
  {
    freeStations(start);
    start = NULL;
  }

  return start;
}

void freeStations(WxStation *_stations)
{
  WxStation *p;
  SkyCondition *s;

  while (_stations)
  {
    p = _stations;
    _stations = _stations->next;

    free(p->id);
    free(p->raw);

    while (p->layers)
    {
      s = p->layers;
      p->layers = p->layers->next;
      free(s);
    }

    free(p);
  }
}
