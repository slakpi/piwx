/**
 * @file wx.c
 */
#include "wx.h"
#include "util.h"
#include "wxtype.h"
#include <ctype.h>
#include <curl/curl.h>
#include <jansson.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef void *yyscan_t;

#include "wx.lexer.h"

#define MAX_DATETIME_LEN 20

/**
 * @brief   Trims a local airport ID for display.
 * @details Non-ICAO airport IDs include numbers and are three characters long,
 *          e.g. 7S3 or X01. However, AviationWeather.gov expects four-character
 *          IDs. So, for the US, 7S3 should be "K7S3". If the specified ID has
 *          a number in it, return a duplicate string that does not have the K.
 *          If the ID is an ICAO ID, return a duplicate of the original string.
 * @param[in] _id The airport ID of interest.
 * @returns A duplicate of either the original ID or the shortened non-ICAO ID.
 */
static char *trimLocalId(const char *_id) {
  size_t      i, len = strlen(_id);
  const char *p = _id;

  if (len < 1) {
    return NULL;
  }

  for (i = 0; i < len; ++i) {
    if (isdigit(_id[i])) {
      p = _id + 1;
      break;
    }
  }

  return strdup(p);
}

/**
 * @struct Response
 * @brief  Buffer to hold response data from cURL.
 */
typedef struct __Response {
  char * str;
  size_t len, bufLen;
} Response;

/**
 * @brief Initialize a response buffer.
 * @param[in] _res The response buffer.
 */
static void initResponse(Response *_res) {
  _res->str    = (char *)malloc(sizeof(char) * 256);
  _res->str[0] = 0;
  _res->len    = 0;
  _res->bufLen = 256;
}

/**
 * @brief   Appends new data from cURL to a response buffer.
 * @details If there is not enough room left in the buffer, the function will
 *          increase the buffer size by 1.5x.
 * @param[in] _res Response buffer to receive the new data.
 * @param[in] _str cURL data.
 * @param[in] _len Length of the cURL data.
 */
static void appendToResponse(Response *_res, const char *_str, size_t _len) {
  size_t newBuf = _res->bufLen;
  if (!_res->str) {
    return;
  }

  while (1) {
    if (_res->len + _len < newBuf) {
      if (_res->bufLen < newBuf) {
        _res->str    = realloc(_res->str, newBuf);
        _res->bufLen = newBuf;
      }

      memcpy(_res->str + _res->len, _str, _len);
      _res->len += _len;
      _res->str[_res->len] = 0;

      return;
    }

    newBuf = (size_t)(newBuf * 1.5) + 1;

    if (newBuf < _res->bufLen) {
      return;
    }
  }
}

/**
 * @brief Free a response buffer.
 * @param[in] _res The response buffer to free.
 */
static void freeResponse(Response *_res) {
  free(_res->str);
  _res->str    = NULL;
  _res->len    = 0;
  _res->bufLen = 0;
}

/**
 * @brief   Simple ISO-8601 parser.
 * @details Assumes UTC and ignores timezone information. Assumes integer
 *          seconds. Performs basic sanity checks, but does not check if the
 *          day exceeds the number of days in the specified month.
 * @param[in]: _str The string to parse.
 * @param[in]: _tm  Receives the date time.
 * @returns 0 if successful, non-zero otherwise.
 */
static int parseUTCDateTime(const char *_str, struct tm *_tm) {
  sscanf(_str, "%d-%d-%dT%d:%d:%d", &_tm->tm_year, &_tm->tm_mon, &_tm->tm_mday,
         &_tm->tm_hour, &_tm->tm_min, &_tm->tm_sec);

  if (_tm->tm_year < 1900) {
    return -1;
  }

  if (_tm->tm_mon < 1 || _tm->tm_mon > 12) {
    return -1;
  }

  if (_tm->tm_mday < 1 || _tm->tm_mday > 31) {
    return -1;
  }

  if (_tm->tm_hour < 0 || _tm->tm_hour > 23) {
    return -1;
  }

  if (_tm->tm_min < 0 || _tm->tm_min > 59) {
    return -1;
  }

  if (_tm->tm_sec < 0 || _tm->tm_sec > 59) {
    return -1;
  }

  _tm->tm_year -= 1900;
  _tm->tm_mon -= 1;
  _tm->tm_isdst = 0;

  return 0;
}

/**
 * @brief   cURL data callback for the sunrise/sunset API.
 * @param[in] _ptr      Data received.
 * @param[in] _size     Size of a data item.
 * @param[in] _nmemb    Data items received.
 * @param[in] _userdata User callback data, i.e. Response object.
 * @returns Bytes processed.
 */
static size_t sunriseSunsetCallback(char *_ptr, size_t _size, size_t _nmemb,
                                    void *_userdata) {
  Response *res   = (Response *)_userdata;
  size_t    bytes = _size * _nmemb;
  appendToResponse(res, _ptr, bytes);
  return bytes;
}

/**
 * @brief   Queries the sunrise/sunset API.
 * @details Note that the function returns the Civil Twilight range rather than
 *          the Sunrise and Sunset range.
 * @param[in]:  _lat     Latitude to query.
 * @param[in]:  _lon     Longitude to query.
 * @param[in]:  _date    Date to query.
 * @param[out]: _sunrise Sunrise in UTC.
 * @param[out]: _sunset  Sunset in UTC.
 * @returns 0 if successful, non-zero otherwise.
 */
static int getSunriseSunsetForDay(double _lat, double _lon, struct tm *_date,
                                  time_t *_sunrise, time_t *_sunset) {
  CURL *       curlLib;
  CURLcode     res;
  json_t *     root, *times, *sunrise, *sunset;
  json_error_t err;
  char         tmp[257];
  Response     json;
  struct tm    dtSunrise, dtSunset;
  int          ok = -1;

  *_sunrise = 0;
  *_sunset  = 0;

  curlLib = curl_easy_init();

  if (!curlLib) {
    return -1;
  }

  snprintf(tmp, _countof(tmp),
           "https://api.sunrise-sunset.org/json?"
           "lat=%f&lng=%f&formatted=0&date=%d-%d-%d",
           _lat, _lon, _date->tm_year + 1900, _date->tm_mon + 1,
           _date->tm_mday);

  initResponse(&json);

  curl_easy_setopt(curlLib, CURLOPT_URL, tmp);
  curl_easy_setopt(curlLib, CURLOPT_WRITEFUNCTION, sunriseSunsetCallback);
  curl_easy_setopt(curlLib, CURLOPT_WRITEDATA, &json);
  res = curl_easy_perform(curlLib);
  curl_easy_cleanup(curlLib);

  if (res != CURLE_OK) {
    return -1;
  }

  root = json_loads(json.str, 0, &err);
  freeResponse(&json);

  if (!root) {
    return -1;
  }

  times = json_object_get(root, "results");

  if (!json_is_object(times)) {
    goto cleanup;
  }

  sunrise = json_object_get(times, "civil_twilight_begin");

  if (!json_is_string(sunrise)) {
    goto cleanup;
  }

  sunset = json_object_get(times, "civil_twilight_end");

  if (!json_is_string(sunset)) {
    goto cleanup;
  }

  // Copy the date/time strings into the temporary string buffer with an
  // explicit limit on the correct format ("YYYY-MM-DDTHH:MM:SS\0").

  strncpy(tmp, json_string_value(sunrise), MAX_DATETIME_LEN);

  if (parseUTCDateTime(tmp, &dtSunrise) != 0) {
    goto cleanup;
  }

  strncpy(tmp, json_string_value(sunset), MAX_DATETIME_LEN);

  if (parseUTCDateTime(tmp, &dtSunset) != 0) {
    goto cleanup;
  }

  *_sunrise = timegm(&dtSunrise);
  *_sunset  = timegm(&dtSunset);

  ok = 0;

cleanup:
  if (root) {
    json_decref(root);
  }

  return ok;
}

/**
 * @brief   Checks if the given observation time is night at the given location.
 * @details Consider a report issued at 1753L on July 31 in the US Pacific
 *          Daylight time zone. The UTC date/time is 0053Z on Aug 1, so the
 *          query will return the sunrise/sunset for Aug 1, not July 31. Using
 *          the Aug 1 data, 0053Z will be less than the sunrise time and
 *          @a isNight would indicate night time despite it being day time PDT.
 *
 *          @a isNight checks the previous and next days as necessary to make a
 *          determination without knowing the station's local time zone.
 * @param[in]: _lat     Observation latitude.
 * @param[in]: _lon     Observation longitude.
 * @param[in]: _obsTime UTC observation time.
 * @returns 0 if day or there is an error, non-zero if night.
 */
static int isNight(double _lat, double _lon, time_t _obsTime) {
  time_t    sr, ss;
  struct tm date;

  gmtime_r(&_obsTime, &date);

  if (getSunriseSunsetForDay(_lat, _lon, &date, &sr, &ss) != 0) {
    return 0;
  }

  // If the observation time is less than the sunrise date/time, check the
  // prior day. Otherwise, check the next day.
  if (_obsTime < sr) {
    if (_obsTime < 86400) {
      return 0; // Underflow
    }

    _obsTime -= 86400;
  } else if (_obsTime >= ss) {
    if (_obsTime + 86400 < _obsTime) {
      return 0; // Overflow
    }

    _obsTime += 86400;
  } else {
    return 0; // Between sunrise and sunset; it's day time.
  }

  gmtime_r(&_obsTime, &date);

  if (getSunriseSunsetForDay(_lat, _lon, &date, &sr, &ss) != 0) {
    return 0;
  }

  // It's night time if greater than sunset on the previous day or less than
  // sunrise on the next day.
  return (_obsTime >= ss || _obsTime < sr);
}

/**
 * @enum  Tag
 * @brief METAR XML tag ID.
 */
// clang-format off
typedef enum {
  tagInvalid = -1,

  tagResponse = 1,

  tagData,

  tagMETAR,

  tagRawText,  tagStationId, tagObsTime,   tagLat,      tagLon, tagTemp,
  tagDewpoint, tagWindDir,   tagWindSpeed, tagWindGust, tagVis, tagAlt,
  tagCategory, tagWxString,  tagSkyCond,   tagVertVis,

  tagSkyCover, tagCloudBase,

  tagSCT,      tagFEW,       tagBKN,       tagOVC,      tagOVX, tagCLR,
  tagSKC,      tagCAVOK,

  tagVFR,      tagMVFR,      tagIFR,       tagLIFR,

  tagFirst = tagResponse,
  tagLast = tagLIFR
} Tag;
// clang-format on

// clang-format off
static const Tag tags[] = {
  tagResponse,

  tagData,

  tagMETAR,

  tagRawText,  tagStationId, tagObsTime,   tagLat,      tagLon, tagTemp,
  tagDewpoint, tagWindDir,   tagWindSpeed, tagWindGust, tagVis, tagAlt,
  tagCategory, tagWxString,  tagSkyCond,   tagVertVis,

  tagSkyCover, tagCloudBase,

  tagSCT,      tagFEW,       tagBKN,       tagOVC,      tagOVX, tagCLR,
  tagSKC,      tagCAVOK,

  tagVFR,      tagMVFR,      tagIFR,       tagLIFR,
};
// clang-format on

// clang-format off
static const char *tagNames[] = {
  "response",

  "data",

  "METAR",

  "raw_text",
  "station_id",
  "observation_time",
  "latitude",
  "longitude",
  "temp_c",
  "dewpoint_c",
  "wind_dir_degrees",
  "wind_speed_kt",
  "wind_gust_kt",
  "visibility_statute_mi",
  "altim_in_hg",
  "flight_category",
  "wx_string",
  "sky_condition",
  "vert_vis_ft",

  "sky_cover",
  "cloud_base_ft_agl",

  "SCT",
  "FEW",
  "BKN",
  "OVC",
  "OVX",
  "CLR",
  "SKC",
  "CAVOK",

  "VFR",
  "MVFR",
  "IFR",
  "LIFR",

  NULL
};
// clang-format on

/**
 * @brief Initialize the tag map.
 * @param[in]: _hash Tag hash map.
 */
static void initHash(xmlHashTablePtr _hash) {
  for (long i = 0; tagNames[i]; ++i)
    xmlHashAddEntry(_hash, (xmlChar *)tagNames[i], (void *)tags[i]);
}

/**
 * @brief   Hash destructor.
 * @details Placeholder only, there is nothing to deallocate.
 * @param[in] _payload: Data associated with tag.
 * @param[in] _name:    The tag name.
 */
static void hashDealloc(void *_payload, xmlChar *_name) {}

/**
 * @brief   Lookup the tag ID for a given tag name.
 * @param[in]: _hash The tag hash map.
 * @param[in]: _tag  The tag name.
 * @returns The associated tag ID or tagInvalid.
 */
static Tag getTag(xmlHashTablePtr _hash, const xmlChar *_tag) {
  void *p = xmlHashLookup(_hash, _tag);

  if (!p) {
    return tagInvalid;
  }

  return (Tag)p;
}

/**
 * @struct METARCallbackData
 * @brief  User data structure for parsing METAR XML.
 */
typedef struct {
  xmlParserCtxtPtr ctxt;
} METARCallbackData;

/**
 * @brief cURL data callback for the METAR XAML API.
 * @param[in] _ptr      Data received.
 * @param[in] _size     Size of a data item.
 * @param[in] _nmemb    Data items received.
 * @param[in] _userdata User callback data, i.e. Response object.
 * @returns Bytes processed.
 */
static size_t metarCallback(char *_ptr, size_t _size, size_t _nmemb,
                            void *_userdata) {
  METARCallbackData *data = (METARCallbackData *)_userdata;
  size_t             res  = _nmemb * _size;

  if (!data->ctxt) {
    data->ctxt = xmlCreatePushParserCtxt(NULL, NULL, _ptr, _nmemb, NULL);

    if (!data->ctxt) {
      res = 0;
    }
  } else {
    if (xmlParseChunk(data->ctxt, _ptr, _nmemb, 0) != 0) {
      res = 0;
    }
  }

  return res;
}

/**
 * @brief   Converts category text to a FlightCategory value.
 * @param[in]: _node The flight category node.
 * @param[in]: _hash The tag hash map.
 * @returns The flight category or catInvalid.
 */
static FlightCategory getStationFlightCategory(xmlNodePtr      _node,
                                               xmlHashTablePtr _hash) {
  Tag tag = getTag(_hash, _node->content);

  switch (tag) {
  case tagVFR:
    return catVFR;
  case tagMVFR:
    return catMVFR;
  case tagIFR:
    return catIFR;
  case tagLIFR:
    return catLIFR;
  default:
    return catInvalid;
  }
}

/**
 * @brief   Convert cloud cover text to a CloudCover value.
 * @param[in]: _attr The cloud cover attribute.
 * @param[in]: _hash The tag hash map.
 * @returns The cloud cover or skyInvalid.
 */
static CloudCover getLayerCloudCover(xmlAttr *_attr, xmlHashTablePtr _hash) {
  Tag tag = getTag(_hash, _attr->children->content);

  switch (tag) {
  case tagSKC:
  case tagCLR:
  case tagCAVOK:
    return skyClear;
  case tagSCT:
    return skyScattered;
  case tagFEW:
    return skyFew;
  case tagBKN:
    return skyBroken;
  case tagOVC:
    return skyOvercast;
  case tagOVX:
    return skyOvercastSurface;
  default:
    return skyInvalid;
  }
}

/**
 * @brief Adds a cloud layer to the list of layers.
 * @param[in]: _node    The sky condition node.
 * @param[in]: _station The station to receive the new layer.
 * @param[in]: _hash    The tag hash map.
 */
static void addCloudLayer(xmlNodePtr _node, WxStation *_station,
                          xmlHashTablePtr _hash) {
  xmlAttr *     a = _node->properties;
  SkyCondition *newLayer, *p;
  Tag           tag;

  newLayer = (SkyCondition *)malloc(sizeof(SkyCondition));
  memset(newLayer, 0, sizeof(SkyCondition));

  // Get the layer information.
  while (a) {
    tag = getTag(_hash, a->name);

    switch (tag) {
    case tagSkyCover:
      newLayer->coverage = getLayerCloudCover(a, _hash);
      break;
    case tagCloudBase:
      newLayer->height =
          (int)strtol((const char *)a->children->content, NULL, 10);
      break;
    default:
      break;
    }

    a = a->next;
  }

  // Add the layer in sorted order.
  if (!_station->layers) {
    _station->layers = newLayer;
  } else {
    p = _station->layers;

    while (p) {
      if (newLayer->height < p->height) {
        // Insert the layer before the current layer. If the current layer is
        // the start of the list, update the list pointer.
        if (p == _station->layers) {
          _station->layers = newLayer;
        } else {
          p->prev->next = newLayer;
        }

        p->prev        = newLayer;
        newLayer->next = p;
        break;
      } else if (!p->next) {
        // There are no more items in the list after this one. Thus, this layer
        // has to be placed after the current layer.
        p->next        = newLayer;
        newLayer->prev = p;
        break;
      }

      p = p->next;
    }
  }
}

/**
 * @brief Reads a station METAR group.
 * @param[in]: _node    The station node.
 * @param[in]: _hash    The tag hash map.
 * @param[in]: _station The station object to receive the METAR information.
 */
static void readStation(xmlNodePtr _node, xmlHashTablePtr _hash,
                        WxStation *_station) {
  Tag        tag;
  struct tm  obs;
  xmlNodePtr c = _node->children;

  while (c) {
    if (c->type == XML_TEXT_NODE) {
      c = c->next;
      continue;
    }

    tag = getTag(_hash, c->name);

    switch (tag) {
    case tagRawText:
      _station->raw = strdup((char *)c->children->content);
      break;
    case tagStationId:
      _station->id      = strdup((char *)c->children->content);
      _station->localId = trimLocalId(_station->id);
      break;
    case tagObsTime:
      parseUTCDateTime((char *)c->children->content, &obs);
      _station->obsTime = timegm(&obs);
      break;
    case tagLat:
      _station->lat = strtod((char *)c->children->content, NULL);
      break;
    case tagLon:
      _station->lon = strtod((char *)c->children->content, NULL);
      break;
    case tagTemp:
      _station->temp = strtod((char *)c->children->content, NULL);
      break;
    case tagDewpoint:
      _station->dewPoint = strtod((char *)c->children->content, NULL);
      break;
    case tagWindDir:
      _station->windDir = atoi((char *)c->children->content);
      break;
    case tagWindSpeed:
      _station->windSpeed = atoi((char *)c->children->content);
      break;
    case tagWindGust:
      _station->windGust = atoi((char *)c->children->content);
      break;
    case tagVis:
      _station->visibility = strtod((char *)c->children->content, NULL);
      break;
    case tagAlt:
      _station->alt = strtod((char *)c->children->content, NULL);
      break;
    case tagWxString:
      _station->wxString = strdup((char *)c->children->content);
      break;
    case tagCategory:
      _station->cat = getStationFlightCategory(c->children, _hash);
      break;
    case tagSkyCond:
      addCloudLayer(c, _station, _hash);
      break;
    case tagVertVis:
      _station->vertVis = strtod((char *)c->children->content, NULL);
      break;
    default:
      break;
    }

    c = c->next;
  }
}

/**
 * @enum Intensity
 * @brief Weather intensity value.
 */
typedef enum {
  intensityInvalid,
  intensityLight,
  intensityModerate,
  intensityHeavy
} Intensity;

/**
 * @brief   Classifies the dominant weather phenomenon.
 * @details Examines all of the reported weather phenomena and returns the
 *          dominant, i.e. most impactful, phenomenon.
 * @param[in]: _station The weather station to classify.
 */
static void classifyDominantWeather(WxStation *_station) {
  yyscan_t        scanner;
  YY_BUFFER_STATE buf;
  int             c, h, descriptor;
  Intensity       intensity;
  SkyCondition *  s;

  // Assume clear skies to start.
  _station->wx = (_station->isNight ? wxClearNight : wxClearDay);

  s = _station->layers;
  h = INT_MAX;

  // First, find the most impactful cloud cover.
  while (s) {
    if (s->coverage < skyScattered && _station->wx < wxClearDay) {
      _station->wx = (_station->isNight ? wxClearNight : wxClearDay);
    } else if (s->coverage < skyBroken && _station->wx < wxScatteredOrFewDay) {
      _station->wx =
          (_station->isNight ? wxScatteredOrFewNight : wxScatteredOrFewDay);
    } else if (s->coverage < skyOvercast && s->height < h &&
               _station->wx < wxBrokenDay) {
      _station->wx = (_station->isNight ? wxBrokenNight : wxBrokenDay);
      h            = s->height;
    } else if (_station->wx < wxOvercast && s->height < h) {
      _station->wx = wxOvercast;
      h            = s->height;
    }

    s = s->next;
  }

  // If there are no reported phenomena, just use the sky coverage.
  if (!_station->wxString) {
    return;
  }

  // Tokenize the weather phenomena string.
  wxtype_lex_init(&scanner);
  buf = wxtype__scan_string(_station->wxString, scanner);

  intensity  = intensityInvalid;
  descriptor = 0;

  while ((c = wxtype_lex(scanner)) != 0) {
    // If the intensity is invalid and the current token does not specify an
    // intensity level, just use moderate intensity, e.g. SH is moderate showers
    // versus -SH for light showers.
    if (intensity == intensityInvalid && c != ' ' && c != '-' && c != '+') {
      intensity = intensityModerate;
    }

    switch (c) {
    case ' ': // Reset token
      intensity  = intensityInvalid;
      descriptor = 0;
      break;
    case wxVC: // Nothing to do for in vincinity
      break;
    case '-':
      intensity = intensityLight;
      break;
    case '+':
      intensity = intensityHeavy;
      break;
    case wxMI: // Shallow descriptor
    case wxPR: // Partial descriptor
    case wxBC: // Patchy descriptor
    case wxDR: // Drifting descriptor
    case wxBL: // Blowing descriptor
    case wxSH: // Showery descriptor
    case wxFZ: // Freezing descriptor
      descriptor = c;
      break;
    case wxTS:
      // If the currently known phenomenon is a lower priority than
      // Thunderstorms, update it with the appropriate light or moderate/heavy
      // Thunderstorm classification.
      if (intensity < intensityModerate &&
          _station->wx < wxLightTstormsSqualls) {
        _station->wx = wxLightTstormsSqualls;
      } else if (_station->wx < wxTstormsSqualls) {
        _station->wx = wxTstormsSqualls;
      }

      break;
    case wxBR: // Mist
    case wxHZ: // Haze
      if (_station->wx < wxLightMistHaze) {
        _station->wx = wxLightMistHaze;
      }

      break;
    case wxDZ: // Drizzle
               // Let drizzle fall through. DZ and RA will be categorized as
               // rain.
    case wxRA: // Rain
      if (descriptor != wxFZ) {
        if (intensity < intensityModerate &&
            _station->wx < wxLightDrizzleRain) {
          _station->wx = wxLightDrizzleRain;
        } else if (_station->wx < wxRain) {
          _station->wx = wxRain;
        }
      } else {
        if (intensity < intensityModerate &&
            _station->wx < wxLightFreezingRain) {
          _station->wx = wxLightFreezingRain;
        } else if (_station->wx < wxFreezingRain) {
          _station->wx = wxFreezingRain;
        }
      }

      break;
    case wxSN: // Snow
    case wxSG: // Snow grains
      if (intensity == intensityLight && _station->wx < wxFlurries) {
        _station->wx = wxFlurries;
      } else if (intensity == intensityModerate && _station->wx < wxLightSnow) {
        _station->wx = wxLightSnow;
      } else if (_station->wx < wxSnow) {
        _station->wx = wxSnow;
      }

      break;
    case wxIC: // Ice crystals
    case wxPL: // Ice pellets
    case wxGR: // Hail
    case wxGS: // Small hail
      // Reuse the freezing rain category.
      if (intensity < intensityModerate && _station->wx < wxLightFreezingRain) {
        _station->wx = wxLightFreezingRain;
      } else if (_station->wx < wxFreezingRain) {
        _station->wx = wxLightFreezingRain;
      }

      break;
    case wxFG: // Fog
    case wxFU: // Smoke
    case wxDU: // Dust
    case wxSS: // Sand storm
    case wxDS: // Dust storm
      if (_station->wx < wxObscuration) {
        _station->wx = wxObscuration;
      }

      break;
    case wxVA: // Volcanic ash
      if (_station->wx < wxVolcanicAsh) {
        _station->wx = wxVolcanicAsh;
      }

      break;
    case wxSQ: // Squalls
      if (intensity < intensityModerate &&
          _station->wx < wxLightTstormsSqualls) {
        _station->wx = wxLightTstormsSqualls;
      } else if (_station->wx < wxTstormsSqualls) {
        _station->wx = wxTstormsSqualls;
      }

      break;
    case wxFC: // Funnel cloud
      if (_station->wx < wxFunnelCloud) {
        _station->wx = wxFunnelCloud;
      }

      break;
    }
  }

  wxtype__delete_buffer(buf, scanner);
  wxtype_lex_destroy(scanner);
}

/**
 * @brief   Search the parent's children for a specific tag.
 * @param[in]: _parent The parent tag.
 * @param[in]: _tag    The tag to find.
 * @returns The first instance of the specified tag or null.
 */
static xmlNodePtr getChildTag(xmlNodePtr _children, Tag _tag,
                              xmlHashTablePtr _hash) {
  xmlNodePtr p = _children;
  Tag        tag;

  while (p) {
    tag = getTag(_hash, p->name);

    if (tag == _tag) {
      return p;
    }

    p = p->next;
  }

  return NULL;
}

WxStation *queryWx(const char *_stations, int *err) {
  CURL *            curlLib;
  CURLcode          res;
  char              url[4096];
  METARCallbackData data;
  xmlDocPtr         doc = NULL;
  xmlNodePtr        p;
  xmlHashTablePtr   hash = NULL;
  Tag               tag;
  WxStation *       start = NULL, *cur, *newStation;
  int               ok    = 0, count, len;

  *err = 0;

  // Build the query string to look for reports within the last hour and a half.
  // It is possible some stations lag more than an hour, but typically not more
  // than an hour and a half.
  strncpy(url,
          "https://aviationweather.gov/adds/dataserver_current/httpparam?"
          "dataSource=metars&"
          "requestType=retrieve&"
          "format=xml&"
          "hoursBeforeNow=1.5&"
          "mostRecentForEachStation=true&"
          "stationString=",
          _countof(url));

  count = _countof(url) - strlen(url);
  len   = strlen(_stations);

  // TODO: Eventually this should split the station string into multiple queries
  // if it is too long. For now, just abort.
  if (len >= count) {
    return NULL;
  }

  strcat(url, _stations);

  curlLib = curl_easy_init();

  if (!curlLib) {
    return NULL;
  }

  data.ctxt = NULL;
  curl_easy_setopt(curlLib, CURLOPT_URL, url);
  curl_easy_setopt(curlLib, CURLOPT_WRITEFUNCTION, metarCallback);
  curl_easy_setopt(curlLib, CURLOPT_WRITEDATA, &data);
  res = curl_easy_perform(curlLib);
  curl_easy_cleanup(curlLib);

  if (data.ctxt) {
    xmlParseChunk(data.ctxt, NULL, 0, 1);
    doc = data.ctxt->myDoc;
    xmlFreeParserCtxt(data.ctxt);
  }

  if (res != CURLE_OK) {
    *err = res;
    goto cleanup;
  }

  hash = xmlHashCreate(tagLast);
  initHash(hash);

  // Find the response tag.
  p = getChildTag(doc->children, tagResponse, hash);

  if (!p) {
    goto cleanup;
  }

  // Find the data tag.
  p = getChildTag(p->children, tagData, hash);

  if (!p) {
    goto cleanup;
  }

  // Scan for METAR groups.
  p = p->children;

  while (p) {
    tag = getTag(hash, p->name);

    if (tag != tagMETAR) {
      p = p->next;
      continue;
    }

    newStation = (WxStation *)malloc(sizeof(WxStation));
    memset(newStation, 0, sizeof(WxStation));

    // Add the station to the circular list.
    if (!start) {
      start = cur = newStation;
      cur->next   = cur;
      cur->prev   = cur;
    } else {
      start->prev      = newStation;
      cur->next        = newStation;
      newStation->next = start;
      newStation->prev = cur;
      cur              = newStation;
    }

    // Read the station and get the night status for classifying weather.
    readStation(p, hash, newStation);
    newStation->isNight =
        isNight(newStation->lat, newStation->lon, newStation->obsTime);
    newStation->blinkState = 1;

    classifyDominantWeather(newStation);

    p = p->next;
  }

  ok = 1;

cleanup:
  if (doc) {
    xmlFreeDoc(doc);
  }

  if (hash) {
    xmlHashFree(hash, hashDealloc);
  }

  if (!ok) {
    freeStations(start);
    start = NULL;
  }

  return start;
}

void freeStations(WxStation *_stations) {
  WxStation *   p;
  SkyCondition *s;

  // Break the circular list.
  _stations->prev->next = NULL;

  while (_stations) {
    p = _stations;

    _stations = _stations->next;

    free(p->id);
    free(p->localId);
    free(p->raw);

    while (p->layers) {
      s         = p->layers;
      p->layers = p->layers->next;
      free(s);
    }

    free(p);
  }
}
