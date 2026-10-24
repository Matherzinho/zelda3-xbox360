# Adds languages to an existing zelda3_assets.dat, so a translation can be added without the US ROM at hand.
#
#   python3 restool.py --extract-dialogue -r <translated ROM>     (writes dialogue_<lang>.txt and font_<lang>.png)
#   python3 add_language.py ../zelda3_assets.dat pt-br             (adds or replaces pt-br in that file)
#
# Only the three dialogue assets change (kDialogue, kDialogueFont, kDialogueMap); every other asset is copied as it is,
# so the file keeps its signature and works with the same build of the game. The first language stays the default.
import array
import os
import struct
import sys

ARGS = [os.path.abspath(a) if i == 1 else a for i, a in enumerate(sys.argv)]  # before leaving the caller's folder
os.chdir(os.path.dirname(os.path.abspath(__file__)))
import text_compression
import sprite_sheets

HEADER = 16 + 32 + 32  # signature text, sha256 of the key names, reserved


def read_assets(path):
  data = open(path, 'rb').read()
  count, keys_len = struct.unpack_from('<II', data, HEADER)
  sizes = array.array('I', data[HEADER + 8:HEADER + 8 + 4 * count])
  if sys.byteorder != 'little': sizes.byteswap()
  pos = HEADER + 8 + 4 * count
  keys = data[pos:pos + keys_len].split(b'\0')[:count]
  pos += keys_len
  assets = []
  for key, size in zip(keys, sizes):
    pos = (pos + 3) & ~3
    assets.append([key.decode(), data[pos:pos + size]])
    pos += size
  return data[:HEADER], keys_len, assets


def write_assets(path, header, assets):
  key_sig = b''.join(k.encode() + b'\0' for k, _ in assets)
  out = bytearray(header + struct.pack('<II', len(assets), len(key_sig)))
  out += b''.join(struct.pack('<I', len(v)) for _, v in assets) + key_sig
  for _, v in assets:
    while len(out) & 3: out += b'\0'
    out += v
  open(path, 'wb').write(out)


def unpack_array(blob):  # inverse of compile_resources.pack_arrays
  last = struct.unpack_from('<H', blob, len(blob) - 2)[0]
  wide = last >= 8192
  n = (last - 8192 if wide else last) + 1
  fmt, sz = ('<I', 4) if wide else ('<H', 2)
  offs = [struct.unpack_from(fmt, blob, i * sz)[0] for i in range(n - 1)]
  base, end = (n - 1) * sz, len(blob) - 2
  bounds = [0] + offs + [end - base]
  return [blob[base + bounds[i]:base + bounds[i + 1]] for i in range(n)]


def pack_arrays(arr):  # as compile_resources.pack_arrays
  all_offs, offs = [], 0
  for a in arr[:-1]:
    offs += len(a)
    all_offs.append(offs)
  if offs < 65536 and len(arr) <= 8192:
    return b''.join([struct.pack('<H', i) for i in all_offs] + [bytes(a) for a in arr] + [struct.pack('<H', len(arr) - 1)])
  return b''.join([struct.pack('<I', i) for i in all_offs] + [bytes(a) for a in arr] + [struct.pack('<H', 8192 + len(arr) - 1)])


def compress_dialogue(lang):
  lines = [line.split(': ', 1)[1] for line in open(text_compression.dialogue_filename(lang), encoding='utf8').read().splitlines()]
  return text_compression.compress_strings(lines, lang)


def main():
  if len(ARGS) < 3:
    sys.exit('usage: add_language.py <zelda3_assets.dat> <lang>[,<lang>...]')
  path = ARGS[1]
  header, _, assets = read_assets(path)
  index = {k: i for i, (k, _) in enumerate(assets)}
  langs, fonts, maps = (unpack_array(assets[index[k]][1]) for k in ('kDialogue', 'kDialogueFont', 'kDialogueMap'))
  names = [unpack_array(m)[0].decode() for m in maps]
  for lang in ARGS[2].split(','):
    if lang not in text_compression.kLanguages:
      sys.exit(f'unknown language {lang}')
    if lang == names[0]:
      sys.exit(f'{lang} is the default language of this file')
    dialogue = pack_arrays([pack_arrays(text_compression.encode_dictionary(lang)), pack_arrays(compress_dialogue(lang))])
    font_data, font_width = sprite_sheets.encode_font_from_png(lang)
    font = pack_arrays([font_data, font_width])
    slot = names.index(lang) if lang in names else len(names)
    if slot == len(names):
      langs.append(b''); fonts.append(b''); maps.append(b''); names.append(lang)
    langs[slot], fonts[slot] = dialogue, font
    flags = (1 if text_compression.uses_new_format(lang) else 0) | 2  # 2: not the language of the US ROM
    maps[slot] = pack_arrays([lang.encode(), bytes([slot, slot, flags])])
    print(f'{lang}: {len(dialogue)} bytes of dialogue, slot {slot}')
  assets[index['kDialogue']][1] = pack_arrays(langs)
  assets[index['kDialogueFont']][1] = pack_arrays(fonts)
  assets[index['kDialogueMap']][1] = pack_arrays(maps)
  write_assets(path, header, assets)
  print(f'{path}: languages {", ".join(names)}')


if __name__ == '__main__':
  main()
