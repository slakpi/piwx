#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <curl/curl.h>
#include <jansson.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "wx.h"
#include "wxtype.h"

typedef void* yyscan_t;

#include "wxtype.lexer.h"

static char* trimLocalId(const char *_id)
{
  size_t i, len = strlen(_id);
  const char *p = _id;

  if (len < 1)
    return NULL;

  for (i = 0; i < len; ++i)
  {
    if (isdigit(_id[i]))
    {
      p = _id + 1;
      break;
    }
  }

  return strdup(p);
}

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
    if (_res->len + _len < newBuf)
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

    newBuf = (size_t)ceil(newBuf * 1.5);
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

static int getSunriseSunsetForDay(double _lat, double _lon, struct tm *_date,
  time_t *_sunrise, time_t *_sunset)
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

  snprintf(url, 257, "https://api.sunrise-sunset.org/json?"
    "lat=%f&lng=%f&formatted=0&date=%d-%d-%d",
    _lat, _lon, _date->tm_year + 1900, _date->tm_mon + 1,
    _date->tm_mday);

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

  sunrise = json_object_get(times, "civil_twilight_begin");
  if (!json_is_string(sunrise))
    goto cleanup;

  sunset = json_object_get(times, "civil_twilight_end");
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

static int isNight(double _lat, double _lon, time_t _obsTime)
{
  time_t n, sr, ss;
  struct tm date;

  /**
   * Without retrieving the local date for each station, isNight requires
   * checking either the previous or the next day's range since the UTC date
   * may be different from the local date. So, it is either two sunrise/sunset
   * API calls, or one along with an API call to a time zone map.
   */
  date = *gmtime(&_obsTime);
  if (getSunriseSunsetForDay(_lat, _lon, &date, &sr, &ss) != 0)
    return -1;

  if (_obsTime < sr)
  { // Check if < current day's sunrise and > previous day's sunset
    n = _obsTime - 86400;
    date = *gmtime(&n);
    if (getSunriseSunsetForDay(_lat, _lon, &date, &sr, &ss) != 0)
      return -1;

    return _obsTime > ss;
  }
  else if (_obsTime >= ss)
  { // Check if >= current day's sunset and < next day's sunrise
    n = _obsTime + 86400;
    date = *gmtime(&n);
    if (getSunriseSunsetForDay(_lat, _lon, &date, &sr, &ss) != 0)
      return -1;

    return _obsTime < sr;
  }

  // >= current day's sunrise and < current day's sunset.
  return 0;
}

typedef enum __Tag
{
  tagInvalid = -1,

  tagResponse = 1,

  tagData,

  tagRawText,     tagStationId,     tagObsTime,       tagLat,         tagLon,
  tagTemp,        tagDewpoint,      tagWindDir,       tagWindSpeed,
  tagWindGust,    tagVis,           tagAlt,           tagCategory,    tagWxString,
  tagSkyCond,     tagVertVis,

  tagSkyCover,    tagCloudBase,

  tagSCT,         tagFEW,           tagBKN,           tagOVC,         tagOVX,
  tagCLR,         tagSKC,           tagCAVOK,

  tagVFR,         tagMVFR,          tagIFR,           tagLIFR,

  tagFirst = tagResponse,
  tagLast = tagLIFR
} Tag;

static const Tag tags[] = {
  tagResponse,

  tagData,

  tagRawText,     tagStationId,     tagObsTime,       tagLat,         tagLon,
  tagTemp,        tagDewpoint,      tagWindDir,       tagWindSpeed,
  tagWindGust,    tagVis,           tagAlt,           tagCategory,    tagWxString,
  tagSkyCond,     tagVertVis,

  tagSkyCover,    tagCloudBase,

  tagSCT,         tagFEW,           tagBKN,           tagOVC,         tagOVX,
  tagCLR,         tagSKC,           tagCAVOK,

  tagVFR,         tagMVFR,          tagIFR,           tagLIFR,
};

static const char* tagNames[] = {
  "response",

  "data",

  "raw_text",       "station_id",     "observation_time",       "latitude",
  "longitude",      "temp_c",         "dewpoint_c",             "wind_dir_degrees",
  "wind_speed_kt",  "wind_gust_kt",   "visibility_statute_mi",  "altim_in_hg",
  "flight_category","wx_string",      "sky_condition",          "vert_vis_ft",

  "sky_cover",      "cloud_base_ft_agl",

  "SCT",            "FEW",            "BKN",                    "OVC",
  "OVX",            "CLR",            "SKC",                    "CAVOK",

  "VFR",            "MVFR",           "IFR",                    "LIFR",

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

static void readStation(xmlNodePtr _node, xmlHashTablePtr _hash,
  WxStation *_station)
{
  Tag tag;
  struct tm obs;
  SkyCondition *skyN, *skyP;
  xmlNodePtr c = _node->children;
  xmlAttr *a;

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
      _station->localId = trimLocalId(_station->id);
      break;
    case tagObsTime:
      parseUTCDateTime(&obs, (char*)c->children->content);
      _station->obsTime = timegm(&obs);
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
    case tagWindGust:
      _station->windGust = atoi((char*)c->children->content);
      break;
    case tagVis:
      _station->visibility = strtod((char*)c->children->content, NULL);
      break;
    case tagAlt:
      _station->alt = strtod((char*)c->children->content, NULL);
      break;
    case tagWxString:
      _station->wxString = strdup((char*)c->children->content);
      break;
    case tagCategory:
      tag = getTag(_hash, c->children->content);

      switch (tag)
      {
      case tagVFR:
        _station->cat = catVFR;
        break;
      case tagMVFR:
        _station->cat = catMVFR;
        break;
      case tagIFR:
        _station->cat = catIFR;
        break;
      case tagLIFR:
        _station->cat = catLIFR;
        break;
      default:
        _station->cat = catInvalid;
        break;
      }

      break;
    case tagSkyCond:
      a = c->properties;
      skyN = (SkyCondition*)malloc(sizeof(SkyCondition));
      memset(skyN, 0, sizeof(SkyCondition));

      while (a)
      {
        tag = getTag(_hash, a->name);

        switch (tag)
        {
        case tagSkyCover:
          tag = getTag(_hash, a->children->content);

          switch (tag)
          {
          case tagSKC:
          case tagCLR:
          case tagCAVOK:
            skyN->coverage = skyClear;
            break;
          case tagSCT:
            skyN->coverage = skyScattered;
            break;
          case tagFEW:
            skyN->coverage = skyFew;
            break;
          case tagBKN:
            skyN->coverage = skyBroken;
            break;
          case tagOVC:
            skyN->coverage = skyOvercast;
            break;
          case tagOVX:
            skyN->coverage = skyOvercastSurface;
            break;
          default:
            skyN->coverage = skyInvalid;
            break;
          }

          break;
        case tagCloudBase:
          skyN->height = atoi((char*)a->children->content);
          break;
        default:
          break;
        }

        a = a->next;
      }

      if (!_station->layers)
        _station->layers = skyN;
      else
      {
        skyP = _station->layers;

        while (skyP)
        {
          if (skyN->height < skyP->height)
          {
            if (skyP == _station->layers)
              _station->layers = skyN;
            else
              skyP->prev->next = skyN;

            skyP->prev = skyN;
            skyN->next = skyP;
            break;
          }

          skyP = skyP->next;
        }
      }

      break;
    case tagVertVis:
      _station->vertVis = strtod((char*)c->children->content, NULL);
      break;
    default:
      break;
    }

    c = c->next;
  }
}

static void classifyDominantWeather(WxStation *_station)
{
  yyscan_t scanner;
  YY_BUFFER_STATE buf;
  int c, h, intensity, descriptor;
  SkyCondition *s;

  _station->wx = wxInvalid;

  s = _station->layers;
  h = 99999;

  while (s)
  {
    if (s->coverage < skyScattered && _station->wx < wxClearDay)
      _station->wx = (_station->isNight ? wxClearNight : wxClearDay);
    else if (s->coverage < skyBroken && _station->wx < wxScatteredOrFewDay)
      _station->wx = (_station->isNight ? wxScatteredOrFewNight : wxScatteredOrFewDay);
    else if (s->coverage < skyOvercast && s->height < h &&
             _station->wx < wxBrokenDay)
    {
      _station->wx = (_station->isNight ? wxBrokenNight : wxBrokenDay);
      h = s->height;
    }
    else if (_station->wx < wxOvercast && s->height < h)
    {
      _station->wx = wxOvercast;
      h = s->height;
    }

    s = s->next;
  }

  if (_station->wx == wxInvalid)
    _station->wx = (_station->isNight ? wxClearNight : wxClearDay);

  if (!_station->wxString)
    return;

  wxtype_lex_init(&scanner);
  buf = wxtype__scan_string(_station->wxString, scanner);

  intensity = -1;
  descriptor = 0;

  while ((c = wxtype_lex(scanner)) != 0)
  {
    if (intensity < 0 && c != ' ' && c != '-' && c != '+' && c != wxVC)
      intensity = 2;

    switch (c)
    {
    case ' ':
      intensity = -1;
      descriptor = 0;
      break;
    case wxVC:
      intensity = 0;
      break;
    case '-':
      intensity = 1;
      break;
    case '+':
      intensity = 3;
      break;
    case wxMI: case wxPR: case wxBC: case wxDR:
    case wxBL: case wxSH: case wxFZ:
      descriptor = c;
      break;
    case wxTS:
      if (intensity < 2 && _station->wx < wxLightTstormsSqualls)
        _station->wx = wxLightTstormsSqualls;
      else if (_station->wx < wxTstormsSqualls)
        _station->wx = wxTstormsSqualls;

      break;
    case wxBR: case wxHZ:
      if (_station->wx < wxLightMistHaze)
        _station->wx = wxLightMistHaze;

      break;
    case wxDZ: case wxRA:
      if (descriptor != wxFZ)
      {
        if (intensity < 2 && _station->wx < wxLightDrizzleRain)
          _station->wx = wxLightDrizzleRain;
        else if (_station->wx < wxRain)
          _station->wx = wxRain;
      }
      else
      {
        if (intensity < 2 && _station->wx < wxLightFreezingRain)
          _station->wx = wxLightFreezingRain;
        else if (_station->wx < wxFreezingRain)
          _station->wx = wxFreezingRain;
      }

      break;
    case wxSN: case wxSG:
      if (intensity == 0 && _station->wx < wxFlurries)
        _station->wx = wxFlurries;
      else if (intensity == 1 && _station->wx < wxLightSnow)
        _station->wx = wxLightSnow;
      else if (_station->wx < wxSnow)
        _station->wx = wxSnow;

      break;
    case wxIC: case wxPL: case wxGR: case wxGS:
      if (intensity < 2 && _station->wx < wxLightFreezingRain)
        _station->wx = wxLightFreezingRain;
      else if (_station->wx < wxFreezingRain)
        _station->wx = wxLightFreezingRain;

      break;
    case wxFG: case wxFU: case wxDU: case wxSS:
    case wxDS:
      if (_station->wx < wxObscuration)
        _station->wx = wxObscuration;

      break;
    case wxVA:
      if (_station->wx < wxVolcanicAsh)
        _station->wx = wxVolcanicAsh;

      break;
    case wxSQ:
      if (intensity < 2 && _station->wx < wxLightTstormsSqualls)
        _station->wx = wxLightTstormsSqualls;
      else if (_station->wx < wxTstormsSqualls)
        _station->wx = wxTstormsSqualls;

      break;
    case wxFC:
      if (_station->wx < wxFunnelCloud)
        _station->wx = wxFunnelCloud;

      break;
    }
  }

  wxtype__delete_buffer(buf, scanner);
  wxtype_lex_destroy(scanner);
}

WxStation* queryWx(const char *_stations, int *err)
{
  CURL *curlLib;
  CURLcode res;
  char url[4096];
  METARCallbackData data;
  xmlDocPtr doc = NULL;
  xmlNodePtr p;
  xmlHashTablePtr hash = NULL;
  Tag tag;
  WxStation *start = NULL, *cur, *n;
  int ok = 0, count, len;

  *err = 0;

  strncpy(url,
    "https://aviationweather.gov/adds/dataserver_current/httpparam?"
    "dataSource=metars&"
    "requestType=retrieve&"
    "format=xml&"
    "hoursBeforeNow=1.5&"
    "mostRecentForEachStation=true&"
    "stationString=",
    4096);

  count = 4096 - strlen(url);
  len = strlen(_stations);
  if (len >= count)
    return NULL;

  strcat(url, _stations);

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
  {
    *err = res;
    goto cleanup;
  }

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
    {
      start = cur = n;
      cur->next = cur;
      cur->prev = cur;
    }
    else
    {
      start->prev = n;
      cur->next = n;
      n->next = start;
      n->prev = cur;
      cur = n;
    }

    readStation(p, hash, n);
    n->isNight = isNight(n->lat, n->lon, n->obsTime);
    n->blinkState = 1;
    classifyDominantWeather(n);

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

  _stations->prev->next = NULL;

  while (_stations)
  {
    p = _stations;

    _stations = _stations->next;

    if (p->id)
      free(p->id);
    if (p->localId)
      free(p->localId);
    if (p->raw)
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
