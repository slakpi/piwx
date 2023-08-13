/**
 * @file wx.c
 */
#include "wx.h"
#include "geo.h"
#include "log.h"
#include "util.h"
#include "wx_type.h"
#include <assert.h>
#include <ctype.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>
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
 * @param[in] id The airport ID of interest.
 * @returns A duplicate of either the original ID or the shortened non-ICAO ID.
 */
static char *trimLocalId(const char *id) {
  size_t      i, len = strlen(id);
  const char *p = id;

  if (len < 1) {
    return NULL;
  }

  for (i = 0; i < len; ++i) {
    if (isdigit(id[i])) {
      p = id + 1;
      break;
    }
  }

  return strdup(p);
}

/**
 * @brief   Simple ISO-8601 parser.
 * @details Assumes UTC and ignores timezone information. Assumes integer
 *          seconds. Performs basic sanity checks, but does not check if the
 *          day exceeds the number of days in the specified month.
 * @param[in] str The string to parse.
 * @param[in] tm  Receives the date time.
 * @returns True if successful, false otherwise.
 */
static bool parseUTCDateTime(const char *str, struct tm *tm) {
  // TODO: This should probably be a lexer at some point.
  // NOLINTNEXTLINE -- Not scanning to string buffers.
  sscanf(str, "%d-%d-%dT%d:%d:%d", &tm->tm_year, &tm->tm_mon, &tm->tm_mday, &tm->tm_hour,
         &tm->tm_min, &tm->tm_sec);

  if (tm->tm_year < 1900) {
    return false;
  }

  if (!(tm->tm_mon >= 1 && tm->tm_mon <= 12)) {
    return false;
  }

  if (!(tm->tm_mday >= 1 && tm->tm_mday <= 31)) {
    return false;
  }

  if (!(tm->tm_hour >= 0 && tm->tm_hour <= 23)) {
    return false;
  }

  if (!(tm->tm_min >= 0 && tm->tm_min <= 59)) {
    return false;
  }

  if (!(tm->tm_sec >= 0 && tm->tm_sec <= 59)) {
    return false;
  }

  tm->tm_year -= 1900;
  tm->tm_mon -= 1;
  tm->tm_isdst = 0;

  return true;
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
 *          determination without knowing the station's local time zone. This is
 *          possible since @a calcSunTransitTimes returns the transit times in
 *          UNIX time rather than an offset from midnight.
 * @param[in] lat     Observation latitude.
 * @param[in] lon     Observation longitude.
 * @param[in] obsTime UTC observation time (UNIX time).
 * @returns True if night, false if day or there is an error.
 */
static bool isNight(double lat, double lon, time_t obsTime) {
  time_t    t = obsTime;
  time_t    sr, ss;
  struct tm date;

  gmtime_r(&t, &date);

  if (!calcSunTransitTimes(lat, lon, date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, &sr,
                           &ss)) {
    return false;
  }

  // If the observation time is less than the sunrise date/time, check the
  // prior day. Otherwise, check the next day.
  if (obsTime < sr) {
    if (t < 86400) {
      return false; // Underflow
    }

    t -= 86400;
  } else if (obsTime >= ss) {
    if (t + 86400 < t) {
      return false; // Overflow
    }

    t += 86400;
  } else {
    return false; // Between sunrise and sunset; it's day time.
  }

  gmtime_r(&t, &date);

  if (!calcSunTransitTimes(lat, lon, date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, &sr,
                           &ss)) {
    return false;
  }

  // It's night time if greater than sunset on the previous day or less than
  // sunrise on the next day.
  return (obsTime >= ss || obsTime < sr);
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
 * @param[in] hash Tag hash map.
 */
static void initHash(xmlHashTablePtr hash) {
  for (long i = 0; tagNames[i]; ++i)
    xmlHashAddEntry(hash, (xmlChar *)tagNames[i], (void *)tags[i]);
}

/**
 * @brief   Hash destructor.
 * @details Placeholder only, there is nothing to deallocate.
 * @param[in] payload Data associated with tag.
 * @param[in] name    The tag name.
 */
static void hashDealloc(void *payload, xmlChar *name) {}

/**
 * @brief   Lookup the tag ID for a given tag name.
 * @param[in] hash The tag hash map.
 * @param[in] tag  The tag name.
 * @returns The associated tag ID or tagInvalid.
 */
static Tag getTag(xmlHashTablePtr hash, const xmlChar *tag) {
  void *p = xmlHashLookup(hash, tag);

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
 * @param[in] ptr      Data received.
 * @param[in] size     Size of a data item.
 * @param[in] nmemb    Data items received.
 * @param[in] userdata User callback data, i.e. Response object.
 * @returns Bytes processed.
 */
static size_t metarCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  METARCallbackData *data = (METARCallbackData *)userdata;
  size_t             res  = nmemb * size;

  if (!data->ctxt) {
    data->ctxt = xmlCreatePushParserCtxt(NULL, NULL, ptr, nmemb, NULL);

    if (!data->ctxt) {
      res = 0;
    }
  } else {
    if (xmlParseChunk(data->ctxt, ptr, nmemb, 0) != 0) {
      res = 0;
    }
  }

  return res;
}

/**
 * @brief   Converts category text to a FlightCategory value.
 * @param[in] node The flight category node.
 * @param[in] hash The tag hash map.
 * @returns The flight category or catInvalid.
 */
static FlightCategory getStationFlightCategory(xmlNodePtr node, xmlHashTablePtr hash) {
  Tag tag = getTag(hash, node->content);

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
 * @param[in] attr The cloud cover attribute.
 * @param[in] hash The tag hash map.
 * @returns The cloud cover or skyInvalid.
 */
static CloudCover getLayerCloudCover(xmlAttr *attr, xmlHashTablePtr hash) {
  Tag tag = getTag(hash, attr->children->content);

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
 * @param[in] node    The sky condition node.
 * @param[in] station The station to receive the new layer.
 * @param[in] hash    The tag hash map.
 */
static void addCloudLayer(xmlNodePtr node, WxStation *station, xmlHashTablePtr hash) {
  xmlAttr      *a = node->properties;
  SkyCondition *newLayer, *p;
  Tag           tag;

  newLayer = malloc(sizeof(SkyCondition));
  memset(newLayer, 0, sizeof(SkyCondition)); // NOLINT -- Size known.

  // Get the layer information.
  while (a) {
    tag = getTag(hash, a->name);

    switch (tag) {
    case tagSkyCover:
      newLayer->coverage = getLayerCloudCover(a, hash);
      break;
    case tagCloudBase:
      newLayer->height = (int)strtol((const char *)a->children->content, NULL, 10);
      break;
    default:
      break;
    }

    a = a->next;
  }

  // Add the layer in sorted order.
  if (!station->layers) {
    station->layers = newLayer;
  } else {
    p = station->layers;

    while (p) {
      if (newLayer->height < p->height) {
        // Insert the layer before the current layer. If the current layer is
        // the start of the list, update the list pointer.
        if (p == station->layers) {
          station->layers = newLayer;
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
 * @param[in] node    The station node.
 * @param[in] hash    The tag hash map.
 * @param[in] station The station object to receive the METAR information.
 */
static void readStation(xmlNodePtr node, xmlHashTablePtr hash, WxStation *station) {
  Tag        tag;
  struct tm  obs;
  xmlNodePtr c = node->children;

  while (c) {
    if (c->type == XML_TEXT_NODE) {
      c = c->next;
      continue;
    }

    tag = getTag(hash, c->name);

    switch (tag) {
    case tagRawText:
      station->raw = strdup((char *)c->children->content);
      break;
    case tagStationId:
      station->id      = strdup((char *)c->children->content);
      station->localId = trimLocalId(station->id);
      break;
    case tagObsTime:
      parseUTCDateTime((char *)c->children->content, &obs);
      station->obsTime = timegm(&obs);
      break;
    case tagLat:
      station->lat = strtod((char *)c->children->content, NULL);
      break;
    case tagLon:
      station->lon = strtod((char *)c->children->content, NULL);
      break;
    case tagTemp:
      station->temp = strtod((char *)c->children->content, NULL);
      break;
    case tagDewpoint:
      station->dewPoint = strtod((char *)c->children->content, NULL);
      break;
    case tagWindDir:
      station->windDir = atoi((char *)c->children->content);
      break;
    case tagWindSpeed:
      station->windSpeed = atoi((char *)c->children->content);
      break;
    case tagWindGust:
      station->windGust = atoi((char *)c->children->content);
      break;
    case tagVis:
      station->visibility = strtod((char *)c->children->content, NULL);
      break;
    case tagAlt:
      station->alt = strtod((char *)c->children->content, NULL);
      break;
    case tagWxString:
      station->wxString = strdup((char *)c->children->content);
      break;
    case tagCategory:
      station->cat = getStationFlightCategory(c->children, hash);
      break;
    case tagSkyCond:
      addCloudLayer(c, station, hash);
      break;
    case tagVertVis:
      station->vertVis = strtod((char *)c->children->content, NULL);
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
typedef enum { intensityInvalid, intensityLight, intensityModerate, intensityHeavy } Intensity;

/**
 * @brief   Classifies the dominant weather phenomenon.
 * @details Examines all of the reported weather phenomena and returns the
 *          dominant, i.e. most impactful, phenomenon.
 * @param[in] station The weather station to classify.
 */
static void classifyDominantWeather(WxStation *station) {
  yyscan_t        scanner;
  YY_BUFFER_STATE buf;
  int             c, h, descriptor;
  Intensity       intensity;
  SkyCondition   *s;

  station->wx = wxInvalid;

  s = station->layers;
  h = INT_MAX;

  // First, find the most impactful cloud cover.
  while (s) {
    if (s->coverage < skyScattered && station->wx < wxClearDay) {
      station->wx = (station->isNight ? wxClearNight : wxClearDay);
    } else if (s->coverage < skyBroken && station->wx < wxScatteredOrFewDay) {
      station->wx = (station->isNight ? wxScatteredOrFewNight : wxScatteredOrFewDay);
    } else if (s->coverage < skyOvercast && s->height < h && station->wx < wxBrokenDay) {
      station->wx = (station->isNight ? wxBrokenNight : wxBrokenDay);
      h           = s->height;
    } else if (station->wx < wxOvercast && s->height < h) {
      station->wx = wxOvercast;
      h           = s->height;
    }

    s = s->next;
  }

  // If there are no reported phenomena, just use the sky coverage.
  if (!station->wxString) {
    return;
  }

  // Tokenize the weather phenomena string.
  wxtype_lex_init(&scanner);
  buf = wxtype__scan_string(station->wxString, scanner);

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
      if (intensity < intensityModerate && station->wx < wxLightTstormsSqualls) {
        station->wx = wxLightTstormsSqualls;
      } else if (station->wx < wxTstormsSqualls) {
        station->wx = wxTstormsSqualls;
      }

      break;
    case wxBR: // Mist
    case wxHZ: // Haze
      if (station->wx < wxLightMistHaze) {
        station->wx = wxLightMistHaze;
      }

      break;
    case wxDZ: // Drizzle
               // Let drizzle fall through. DZ and RA will be categorized as
               // rain.
    case wxRA: // Rain
      if (descriptor != wxFZ) {
        if (intensity < intensityModerate && station->wx < wxLightDrizzleRain) {
          station->wx = wxLightDrizzleRain;
        } else if (station->wx < wxRain) {
          station->wx = wxRain;
        }
      } else {
        if (intensity < intensityModerate && station->wx < wxLightFreezingRain) {
          station->wx = wxLightFreezingRain;
        } else if (station->wx < wxFreezingRain) {
          station->wx = wxFreezingRain;
        }
      }

      break;
    case wxSN: // Snow
    case wxSG: // Snow grains
      if (intensity == intensityLight && station->wx < wxFlurries) {
        station->wx = wxFlurries;
      } else if (intensity == intensityModerate && station->wx < wxLightSnow) {
        station->wx = wxLightSnow;
      } else if (station->wx < wxSnow) {
        station->wx = wxSnow;
      }

      break;
    case wxIC: // Ice crystals
    case wxPL: // Ice pellets
    case wxGR: // Hail
    case wxGS: // Small hail
      // Reuse the freezing rain category.
      if (intensity < intensityModerate && station->wx < wxLightFreezingRain) {
        station->wx = wxLightFreezingRain;
      } else if (station->wx < wxFreezingRain) {
        station->wx = wxLightFreezingRain;
      }

      break;
    case wxFG: // Fog
    case wxFU: // Smoke
    case wxDU: // Dust
    case wxSS: // Sand storm
    case wxDS: // Dust storm
      if (station->wx < wxObscuration) {
        station->wx = wxObscuration;
      }

      break;
    case wxVA: // Volcanic ash
      if (station->wx < wxVolcanicAsh) {
        station->wx = wxVolcanicAsh;
      }

      break;
    case wxSQ: // Squalls
      if (intensity < intensityModerate && station->wx < wxLightTstormsSqualls) {
        station->wx = wxLightTstormsSqualls;
      } else if (station->wx < wxTstormsSqualls) {
        station->wx = wxTstormsSqualls;
      }

      break;
    case wxFC: // Funnel cloud
      if (station->wx < wxFunnelCloud) {
        station->wx = wxFunnelCloud;
      }

      break;
    }
  }

  wxtype__delete_buffer(buf, scanner);
  wxtype_lex_destroy(scanner);
}

/**
 * @brief   Search the parent's children for a specific tag.
 * @param[in] children The parent's children.
 * @param[in] tag      The tag to find.
 * @param[in] hash     The hash table to use.
 * @returns The first instance of the specified tag or null.
 */
static xmlNodePtr getChildTag(xmlNodePtr children, Tag tag, xmlHashTablePtr hash) {
  xmlNodePtr p = children;
  Tag        curTag;

  while (p) {
    curTag = getTag(hash, p->name);

    if (curTag == tag) {
      return p;
    }

    p = p->next;
  }

  return NULL;
}

WxStation *queryWx(const char *stations, int *err) {
  CURL             *curlLib;
  CURLcode          res;
  char              url[4096];
  METARCallbackData data;
  xmlDocPtr         doc = NULL;
  xmlNodePtr        p;
  xmlHashTablePtr   hash = NULL;
  Tag               tag;
  WxStation        *start = NULL, *cur, *newStation;
  int               count, len;
  bool              ok = false;

  *err = 0;

  // Build the query string to look for reports within the last hour and a half.
  // It is possible some stations lag more than an hour, but typically not more
  // than an hour and a half.
  count = strncpy_safe(url,
                       "https://aviationweather.gov/adds/dataserver_current/httpparam?"
                       "dataSource=metars&"
                       "requestType=retrieve&"
                       "format=xml&"
                       "hoursBeforeNow=1.5&"
                       "mostRecentForEachStation=true&"
                       "stationString=",
                       COUNTOF(url));

  assertLog(count < COUNTOF(url), "Base URL is too large.");
  count = COUNTOF(url) - strlen(url);
  len   = strlen(stations);

  // TODO: Eventually this should split the station string into multiple queries
  // if it is too long. For now, just abort.
  if (len >= count) {
    return NULL;
  }

  // NOLINTNEXTLINE -- strncat is sufficient; sizes checked above.
  strncat(url, stations, count);

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

  if (res != CURLE_OK || !data.ctxt) {
    *err = res;
    return NULL;
  }

  xmlParseChunk(data.ctxt, NULL, 0, 1);
  doc = data.ctxt->myDoc;
  xmlFreeParserCtxt(data.ctxt);

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
    memset(newStation, 0, sizeof(WxStation)); // NOLINT -- Size known.

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
    newStation->isNight    = isNight(newStation->lat, newStation->lon, newStation->obsTime);
    newStation->blinkState = 1;

    classifyDominantWeather(newStation);

    p = p->next;
  }

  ok = true;

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

void freeStations(WxStation *stations) {
  WxStation    *p;
  SkyCondition *s;

  if (!stations) {
    return;
  }

  // Break the circular list.
  stations->prev->next = NULL;

  while (stations) {
    p = stations;

    stations = stations->next;

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
