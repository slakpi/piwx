/**
 * @file wx.c
 * @ingroup WxModule
 */
#include "wx.h"
#include "geo.h"
#include "log.h"
#include "util.h"
#include "wx_type.h"
#include <ctype.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef void *yyscan_t;

#include "wx.lexer.h"

#define MAX_DATETIME_LEN 20
#define MAX_WEATHER_LEN  1024
#define MAX_IDENT_LEN    6

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

/**
 * @enum Intensity
 * @brief Weather intensity value.
 */
typedef enum { intensityInvalid, intensityLight, intensityModerate, intensityHeavy } Intensity;

/**
 * @struct METARCallbackData
 * @brief  User data structure for parsing METAR XML.
 */
typedef struct {
  xmlParserCtxtPtr ctxt;
} METARCallbackData;

/**
 * @typedef StationCompareFn
 * @brief   Weather station comparison function signature.
 * @param[in] a Left-hand side station.
 * @param[in] b Right-hand side station.
 * @returns -1 if @a a < @a b, 0 if @a a == @a b, 1 if @a a > @a b.
 */
typedef int (*StationCompareFn)(const WxStation *a, const WxStation *b);

// clang-format off
static const Tag gTags[] = {
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
static const char *gTagNames[] = {
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

static void addCloudLayer(xmlNodePtr node, WxStation *station, xmlHashTablePtr hash);

static void classifyDominantWeather(WxStation *station);

static int compareIdentifiers(const WxStation *a, const WxStation *b);

static int compareOrder(const WxStation *a, const WxStation *b);

static int comparePositions(const WxStation *a, const WxStation *b);

static char *dupNodeText(xmlNodePtr node, size_t maxLen);

static xmlNodePtr getChildTag(xmlNodePtr children, Tag tag, xmlHashTablePtr hash);

static bool getNodeAsDouble(double *v, xmlNodePtr node);

static bool getNodeAsInt(int *v, xmlNodePtr node);

static bool getNodeAsUTCDateTime(struct tm *tm, xmlNodePtr node);

static CloudCover getLayerCloudCover(xmlAttr *attr, xmlHashTablePtr hash);

static FlightCategory getStationFlightCategory(xmlNodePtr node, xmlHashTablePtr hash);

static unsigned int getStationOrder(xmlHashTablePtr hash, const char *id);

static Tag getTag(xmlHashTablePtr hash, const xmlChar *tag);

static void hashDealloc(void *payload, const xmlChar *name);

static xmlHashTablePtr initStationOrderHash(const char *stations);

static xmlHashTablePtr initTagHash();

static void insertStation(WxStation **start, WxStation *newStation, SortType sort);

static size_t metarCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

static void readStation(xmlNodePtr node, xmlHashTablePtr hash, WxStation *station);

static char *trimLocalId(const char *id, size_t maxLen);

void wx_freeStations(WxStation *stations) {
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

WxStation *wx_queryWx(const char *stations, SortType sort, DaylightSpan daylight, time_t curTime,
                      int *err) {
  CURL             *curlLib;
  CURLcode          res;
  char              url[4096];
  METARCallbackData data;
  xmlDocPtr         doc = NULL;
  xmlNodePtr        p;
  xmlHashTablePtr   hash      = NULL;
  xmlHashTablePtr   orderHash = NULL;
  Tag               tag;
  WxStation        *start = NULL;
  int               count, len;
  bool              ok = false;

  *err      = 0;
  hash      = initTagHash();
  orderHash = initStationOrderHash(stations);

  if (!hash || !orderHash) {
    *err = -1;
    goto cleanup;
  }

  // Build the query string to look for the most recent report for each station
  // within the last hour and a half. It is possible some stations lag more than
  // an hour, but typically not more than an hour and a half.
  count = strncpy_safe(url, COUNTOF(url),
                       "https://aviationweather.gov/api/data/metar?"
                       "hours=1.5&"
                       "format=xml&"
                       "ids=");

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
    WxStation *newStation;

    tag = getTag(hash, p->name);

    if (tag != tagMETAR) {
      p = p->next;
      continue;
    }

    newStation = malloc(sizeof(WxStation));

    if (!newStation) {
      break;
    }

    memset(newStation, 0, sizeof(*newStation)); // NOLINT -- Size known.

    readStation(p, hash, newStation);
    newStation->isNight    = geo_isNight(newStation->pos, curTime, daylight);
    newStation->blinkState = false;
    newStation->order      = getStationOrder(orderHash, newStation->id);
    classifyDominantWeather(newStation);

    insertStation(&start, newStation, sort);

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

  if (orderHash) {
    xmlHashFree(orderHash, hashDealloc);
  }

  if (!ok) {
    wx_freeStations(start);
    start = NULL;
  }

  return start;
}

void wx_updateDayNightState(WxStation *station, DaylightSpan daylight, time_t now) {
  station->isNight = geo_isNight(station->pos, now, daylight);

  // Update icons that have day/night variants.
  switch (station->wx) {
  case wxClearDay:
  case wxClearNight:
    station->wx = station->isNight ? wxClearNight : wxClearDay;
    break;
  case wxScatteredOrFewDay:
  case wxScatteredOrFewNight:
    station->wx = station->isNight ? wxScatteredOrFewNight : wxScatteredOrFewDay;
    break;
  case wxBrokenDay:
  case wxBrokenNight:
    station->wx = station->isNight ? wxBrokenNight : wxBrokenDay;
    break;
  default:
    break;
  }
}

/**
 * @brief   Trims a local airport ID for display.
 * @details Non-ICAO airport IDs include numbers and are three characters long,
 *          e.g. 7S3 or X01. However, AviationWeather.gov expects four-character
 *          IDs. So, for the US, 7S3 should be "K7S3". If the specified ID has
 *          a number in it, return a duplicate string that does not have the K.
 *          If the ID is an ICAO ID, return a duplicate of the original string.
 * @param[in] id     The airport ID of interest.
 * @param[in] maxLen The maximum number of characters permitted.
 * @returns A duplicate of either the original ID or the shortened non-ICAO ID.
 */
static char *trimLocalId(const char *id, size_t maxLen) {
  size_t      len;
  const char *p = id;

  if (!id) {
    return NULL;
  }

  len = strnlen(id, maxLen);

  if (len < 2) {
    return strndup(p, len);
  }

  for (int i = 0; i < len; ++i) {
    if (isdigit(id[i])) {
      p = id + 1;
      break;
    }
  }

  return strndup(p, len);
}

/**
 * @brief   Initialize the station query order hash.
 * @param[in] stations The list of stations to query.
 * @returns A new hash table pointer or NULL if there are no stations.
 */
static xmlHashTablePtr initStationOrderHash(const char *stations) {
  static const char *delim = ", \t\n";

  char            buf[4096];
  char           *p;
  uintptr_t       count = 0;
  xmlHashTablePtr hash;

  // First pass: count the number of stations.
  strncpy_safe(buf, COUNTOF(buf), stations);
  p = strtok(buf, delim);
  while (p) {
    ++count;
    p = strtok(NULL, delim);
  }

  if (count == 0) {
    return NULL;
  }

  // Second pass, allocate the hash table and add the entries.
  strncpy_safe(buf, COUNTOF(buf), stations);
  p     = strtok(buf, delim);
  hash  = xmlHashCreate(count);
  count = 0;

  if (!hash) {
    return NULL;
  }

  while (p) {
    xmlHashAddEntry(hash, (xmlChar *)p, (void *)count++);
    p = strtok(NULL, delim);
  }

  return hash;
}

/**
 * @brief   Initialize the tag map.
 * @returns A new hash table pointer.
 */
static xmlHashTablePtr initTagHash() {
  xmlHashTablePtr hash = xmlHashCreate(tagLast);

  if (!hash) {
    return NULL;
  }

  for (long i = 0; gTagNames[i]; ++i) {
    xmlHashAddEntry(hash, (xmlChar *)gTagNames[i], (void *)gTags[i]);
  }

  return hash;
}

/**
 * @brief   Hash destructor.
 * @details Placeholder only, there is nothing to deallocate.
 * @param[in] payload Data associated with tag.
 * @param[in] name    The tag name.
 */
static void hashDealloc(void *payload, const xmlChar *name) {}

/**
 * @brief   Retrieve the query order from a station from the hash table.
 * @param[in] hash The hash table to query.
 * @param[in] id   The station identifier to query.
 * @returns The query order for the station or 0 if the station is not found.
 */
static unsigned int getStationOrder(xmlHashTablePtr hash, const char *id) {
  uintptr_t t = (uintptr_t)xmlHashLookup(hash, (const xmlChar *)id);
  return (unsigned int)t;
}

/**
 * @brief   Lookup the tag ID for a given tag name.
 * @param[in] hash The tag hash map.
 * @param[in] tag  The tag name.
 * @returns The associated tag ID or tagInvalid.
 */
static Tag getTag(xmlHashTablePtr hash, const xmlChar *tag) {
  uintptr_t t = (uintptr_t)xmlHashLookup(hash, tag);

  if (t > tagLast) {
    return tagInvalid;
  }

  return (Tag)t;
}

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
  Tag tag;

  if (!node || !node->content) {
    return catInvalid;
  }

  tag = getTag(hash, node->content);

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
  bool          hasHeight = false;

  newLayer = malloc(sizeof(SkyCondition));

  if (!newLayer) {
    return;
  }

  memset(newLayer, 0, sizeof(SkyCondition)); // NOLINT -- Size known.

  // Get the layer information.
  while (a) {
    tag = getTag(hash, a->name);

    switch (tag) {
    case tagSkyCover:
      newLayer->coverage = getLayerCloudCover(a, hash);
      break;
    case tagCloudBase:
      hasHeight = getNodeAsInt(&newLayer->height, a->children);
      break;
    default:
      break;
    }

    a = a->next;
  }

  // If the coverage is not valid or the height is invalid, then the layer
  // provides no information, cannot be sorted, and should just be discarded.
  if (newLayer->coverage == skyInvalid || !hasHeight || newLayer->height < 0) {
    free(newLayer);
    return;
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
  xmlNodePtr c      = node->children;
  bool       hasLat = false, hasLon = false;

  while (c) {
    if (c->type == XML_TEXT_NODE) {
      c = c->next;
      continue;
    }

    tag = getTag(hash, c->name);

    switch (tag) {
    case tagRawText:
      station->raw = dupNodeText(c->children, MAX_WEATHER_LEN);
      break;
    case tagStationId:
      station->id      = dupNodeText(c->children, MAX_IDENT_LEN);
      station->localId = trimLocalId(station->id, MAX_IDENT_LEN);
      break;
    case tagObsTime:
      station->hasObsTime = getNodeAsUTCDateTime(&obs, c->children);
      station->obsTime    = timegm(&obs);
      break;
    case tagLat:
      hasLat = getNodeAsDouble(&station->pos.lat, c->children);
      break;
    case tagLon:
      hasLon = getNodeAsDouble(&station->pos.lon, c->children);
      break;
    case tagTemp:
      station->hasTemp = getNodeAsDouble(&station->temp, c->children);
      break;
    case tagDewpoint:
      station->hasDewPoint = getNodeAsDouble(&station->dewPoint, c->children);
      break;
    case tagWindDir:
      station->hasWindDir = getNodeAsInt(&station->windDir, c->children);
      break;
    case tagWindSpeed:
      station->hasWindSpeed = getNodeAsInt(&station->windSpeed, c->children);
      break;
    case tagWindGust:
      station->hasWindGust = getNodeAsInt(&station->windGust, c->children);
      break;
    case tagVis:
      station->hasVisibility = getNodeAsDouble(&station->visibility, c->children);
      break;
    case tagAlt:
      station->hasAlt = getNodeAsDouble(&station->alt, c->children);
      break;
    case tagWxString:
      station->wxString = dupNodeText(c->children, MAX_WEATHER_LEN);
      break;
    case tagCategory:
      station->cat = getStationFlightCategory(c->children, hash);
      break;
    case tagSkyCond:
      addCloudLayer(c, station, hash);
      break;
    case tagVertVis:
      station->hasVertVis = getNodeAsInt(&station->vertVis, c->children);
      break;
    default:
      break;
    }

    c = c->next;
  }

  station->hasPosition = (hasLat && hasLon);
}

/**
 * @brief Insert a new station into a circular list.
 * @param[in,out] start   The head of the list.
 * @param[in,out] station The station to insert.
 * @param[in]     sort    Station sort type.
 */
static void insertStation(WxStation **start, WxStation *newStation, SortType sort) {
  StationCompareFn comp;
  WxStation       *p;

  // If start is NULL, just make the new station the start of the list.
  if (!*start) {
    *start         = newStation;
    (*start)->next = newStation;
    (*start)->prev = newStation;
    return;
  }

  switch (sort) {
  case sortAlpha:
    comp = compareIdentifiers;
    break;
  case sortPosition:
    comp = comparePositions;
    break;
  case sortQuery:
    comp = compareOrder;
    break;
  default:
    comp = NULL;
    break;
  }

  p = *start;

  // If not sorting, set p to NULL. This will skip the loop and insert the new
  // station at the end of the list.
  if (!comp) {
    p = NULL;
  }

  while (p) {
    if (comp(newStation, p) < 0) {
      break;
    }

    if (p->next == *start) {
      p = NULL;
    } else {
      p = p->next;
    }
  }

  // If p is the start of the list, update the start pointer to the new station
  // since it will be inserted before the current start.
  if (p == *start) {
    *start = newStation;
  }

  // If p is NULL, set p to the start to insert the station at the end of the
  // list.
  if (!p) {
    p = *start;
  }

  newStation->next = p;
  newStation->prev = p->prev;
  p->prev->next    = newStation;
  p->prev          = newStation;
}

/**
 * @brief   Lexicographical sort of the station local identifiers.
 * @details If neither station has a local identifier, they compare equal. A
 *          station without a local identifier compares less than a station with
 *          a local identifier. Otherwise, a lexicographical comparison of the
 *          local identifiers is performed.
 * @see   @a StationCompareFn
 */
static int compareIdentifiers(const WxStation *a, const WxStation *b) {
  if (!a->localId && !b->localId) {
    return 0;
  } else if (!a->localId) {
    return -1;
  } else if (!b->localId) {
    return 1;
  }

  return strcmp(a->localId, b->localId);
}

/**
 * @brief Query order sort.
 * @see   @a StationCompareFn
 */
static int compareOrder(const WxStation *a, const WxStation *b) {
  if (a->order < b->order) {
    return -1;
  } else if (a->order == b->order) {
    return 0;
  } else {
    return 1;
  }
}

/**
 * @brief   Geographical sort by longitude, then latitude.
 * @details If neither station has a position, they compare equal. A station
 *          without a position compares less than a station with a position.
 *          Otherwise, sort stations West to East, then North to South.
 * @see   @a StationCompareFn
 */
static int comparePositions(const WxStation *a, const WxStation *b) {
  if (!a->hasPosition && !b->hasPosition) {
    return 0;
  } else if (!a->hasPosition || a->pos.lon < b->pos.lon) {
    return -1;
  } else if (!b->hasPosition || a->pos.lon > b->pos.lon) {
    return 1;
  }

  if (a->pos.lat > b->pos.lat) {
    return -1;
  } else if (a->pos.lat < b->pos.lat) {
    return 1;
  }

  return 0;
}

/**
 * @brief   Duplicate a node's text.
 * @param[in] node   The node to duplicate.
 * @param[in] maxLen The maximum number of characters to duplicate.
 * @returns The duplicate string or NULL if the node invalid.
 */
static char *dupNodeText(xmlNodePtr node, size_t maxLen) {
  size_t len;

  if (!node || !node->content) {
    return NULL;
  }

  len = strnlen((char *)node->content, maxLen);

  return strndup((char *)node->content, len);
}

/**
 * @brief   Convert a node's text to a double.
 * @param[out] v    Double value.
 * @param[in]  node The node to convert.
 * @returns True if the conversion succeeds, false if the node is invalid.
 */
static bool getNodeAsDouble(double *v, xmlNodePtr node) {
  char *end;

  if (!node || !node->content) {
    return false;
  }

  *v = strtod((char *)node->content, &end);

  return (end != (char *)node->content && isfinite(*v));
}

/**
 * @brief   Convert a node's text to an integer.
 * @param[out] v    Integer value.
 * @param[in]  node The node to convert.
 * @returns True if the conversion succeeds, false if the node is invalid.
 */
static bool getNodeAsInt(int *v, xmlNodePtr node) {
  char *end;

  if (!node || !node->content) {
    return false;
  }

  *v = (int)strtol((char *)node->content, &end, 10);

  return (end != (char *)node->content);
}

/**
 * @brief   Convert a node's text as an ISO-8601 date/time string.
 * @details Assumes UTC and ignores timezone information. Assumes integer
 *          seconds. Performs basic sanity checks, but does not check if the
 *          day exceeds the number of days in the specified month.
 *
 *          The output date/time will be zeroed if the date/time string is
 *          invalid.
 * @param[out] tm   Receives the date/time.
 * @param[in]  node The node to convert.
 * @returns True if successful, false otherwise.
 */
static bool getNodeAsUTCDateTime(struct tm *tm, xmlNodePtr node) {
  struct tm tmp_tm;

  memset(tm, 0, sizeof(*tm)); // NOLINT -- Size known.

  if (!node || !node->content) {
    return false;
  }

  // TODO: This should probably be a lexer at some point.
  // NOLINTNEXTLINE -- Not scanning to string buffers.
  sscanf((char *)node->content, "%d-%d-%dT%d:%d:%d", &tmp_tm.tm_year, &tmp_tm.tm_mon,
         &tmp_tm.tm_mday, &tmp_tm.tm_hour, &tmp_tm.tm_min, &tmp_tm.tm_sec);

  if (tmp_tm.tm_year < 1900) {
    return false;
  }

  if (!(tmp_tm.tm_mon >= 1 && tmp_tm.tm_mon <= 12)) {
    return false;
  }

  if (!(tmp_tm.tm_mday >= 1 && tmp_tm.tm_mday <= 31)) {
    return false;
  }

  if (!(tmp_tm.tm_hour >= 0 && tmp_tm.tm_hour <= 23)) {
    return false;
  }

  if (!(tmp_tm.tm_min >= 0 && tmp_tm.tm_min <= 59)) {
    return false;
  }

  if (!(tmp_tm.tm_sec >= 0 && tmp_tm.tm_sec <= 59)) {
    return false;
  }

  tmp_tm.tm_year -= 1900;
  tmp_tm.tm_mon -= 1;
  tmp_tm.tm_isdst = 0;

  *tm = tmp_tm;

  return true;
}

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

  // If there are no cloud layers and there are no reported phenomena, just
  // assume clear weather.
  if (!station->layers && !station->wxString) {
    station->wx = station->isNight ? wxClearNight : wxClearDay;
    return;
  }

  // First, find the most impactful cloud cover.
  station->wx = wxInvalid;
  s           = station->layers;
  h           = INT_MAX;

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
    case wxVC: // Nothing to do for in vicinity
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
