import os
import re
import sys

def main(args):
  if len(args) < 3:
    return -1

  file = os.path.basename(args[1])
  header = re.sub("( |\.)", "_", file).upper()
  constant = header + "_SRC"
  header_guard = header + "_H"

  in_file = open(args[1], "r")
  out_file = open(args[2], "w")

  out_file.write("#if !defined " + header_guard + "\n")
  out_file.write("#define " + header_guard + "\n\n")
  out_file.write("#define " + constant + " \\\n")

  while True:
    line = in_file.readline()

    if not line:
      break
    
    line = re.sub("^(.*)$", "  \"\\1\" \\\\", line)
    out_file.write(line)

  out_file.write("\n\n")
  out_file.write("#endif\n")

  return 0


if __name__ == "__main__":
  main(sys.argv)