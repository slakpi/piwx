#include <string.h>
#include <config.h>

static char* appendFileToPath(const char *_prefix, const char *_file, char *_path, size_t _len)
{
  size_t pl = strlen(_prefix), fl = strlen(_file);

  if (_prefix[pl - 1] != '/')
    ++pl;
  if (pl + fl + 1 >= _len)
    return NULL;

  _path[0] = 0;

  strcat(_path, _prefix);

  if (_path[pl - 1] != '/')
  {
    _path[pl - 1] = '/';
    _path[pl] = 0;
  }

  strcat(_path, _file);

  return _path;
}

char* getPathForBitmap(const char *_file, char *_path, size_t _len)
{
  return appendFileToPath(IMAGE_RESOURCES, _file, _path, _len);
}

char* getPathForFont(const char *_file, char *_path, size_t _len)
{
  return appendFileToPath(FONT_RESOURCES, _file, _path, _len);
}
